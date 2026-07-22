#include "Server/GuiyangAgonesLifecycleSubsystem.h"

#include "GuiyangMahjong.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
    bool ReadAnnotation(const FGameServerResponse& Response, const TCHAR* Key, FString& OutValue)
    {
        const FString* Value = Response.ObjectMeta.Annotations.Find(Key);
        if (!Value) return false;
        OutValue = Value->TrimStartAndEnd();
        return !OutValue.IsEmpty();
    }
}

bool UGuiyangAgonesLifecycleSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    return IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

bool UGuiyangAgonesLifecycleSubsystem::IsAgonesRequested(
    const TCHAR* CommandLine, const FString& EnvironmentValue)
{
    FString Orchestrator;
    if (CommandLine)
    {
        FParse::Value(CommandLine, TEXT("MahjongOrchestrator="), Orchestrator);
    }
    if (Orchestrator.IsEmpty())
    {
        Orchestrator = EnvironmentValue;
    }
    Orchestrator.TrimStartAndEndInline();
    return Orchestrator.Equals(TEXT("Agones"), ESearchCase::IgnoreCase);
}

bool UGuiyangAgonesLifecycleSubsystem::TryBuildLaunchConfig(
    const FGameServerResponse& Response,
    const FString& SigningKey,
    const FString& MatchResultOutboxPath,
    FGuiyangGameServerLaunchConfig& OutConfig,
    FString& OutError)
{
    OutConfig = {};
    OutError.Reset();
    if (!Response.Status.State.Equals(TEXT("Allocated"), ESearchCase::IgnoreCase))
    {
        OutError = TEXT("AGONES_GAMESERVER_NOT_ALLOCATED");
        return false;
    }
    if (!ReadAnnotation(Response, TEXT("mahjong.freebooz/room-id"), OutConfig.RoomId)
        || !ReadAnnotation(Response, TEXT("mahjong.freebooz/match-id"), OutConfig.MatchId)
        || !ReadAnnotation(Response, TEXT("mahjong.freebooz/server-instance-id"), OutConfig.ServerInstanceId)
        || !ReadAnnotation(Response, TEXT("mahjong.freebooz/registration-credential"), OutConfig.RegistrationCredential)
        || !ReadAnnotation(Response, TEXT("mahjong.freebooz/lobby-internal-url"), OutConfig.LobbyInternalUrl)
        || !ReadAnnotation(Response, TEXT("mahjong.freebooz/build-version"), OutConfig.BuildVersion))
    {
        OutError = TEXT("AGONES_ALLOCATION_METADATA_INCOMPLETE");
        return false;
    }
    OutConfig.AdvertisedIp = Response.Status.Address.TrimStartAndEnd();
    const FPort* GamePort = Response.Status.Ports.FindByPredicate(
        [](const FPort& Port) { return Port.Name.Equals(TEXT("game"), ESearchCase::IgnoreCase); });
    if (!GamePort && !Response.Status.Ports.IsEmpty()) GamePort = &Response.Status.Ports[0];
    OutConfig.Port = GamePort ? GamePort->Port : 0;
    OutConfig.JoinTicketSigningKey = SigningKey;
    OutConfig.MatchResultOutboxPath = MatchResultOutboxPath;
    if (OutConfig.AdvertisedIp.IsEmpty() || OutConfig.Port <= 0 || OutConfig.Port > 65535
        || OutConfig.JoinTicketSigningKey.Len() < 32
        || OutConfig.RegistrationCredential.Len() < 16
        || OutConfig.MatchResultOutboxPath.IsEmpty())
    {
        OutError = TEXT("AGONES_ALLOCATION_CONFIGURATION_INVALID");
        return false;
    }
    return true;
}

void UGuiyangAgonesLifecycleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    const FString EnvironmentValue =
        FPlatformMisc::GetEnvironmentVariable(TEXT("MAHJONG_ORCHESTRATOR"));
    if (!IsAgonesRequested(FCommandLine::Get(), EnvironmentValue))
    {
        return;
    }

    Collection.InitializeDependency<UAgonesSubsystem>();
    Agones = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAgonesSubsystem>() : nullptr;
    if (!Agones)
    {
        UE_LOG(LogMahjongServer, Error, TEXT("Agones orchestrator selected but SDK subsystem is unavailable"));
        return;
    }

    bActive = true;
    Agones->ConnectedDelegate.AddUniqueDynamic(this, &ThisClass::HandleConnected);
    FGameServerDelegate WatchDelegate;
    WatchDelegate.BindDynamic(this, &ThisClass::HandleGameServerUpdated);
    Agones->WatchGameServer(WatchDelegate);
    // The SDK subsystem exists in every dedicated-server process. Start its health timer only
    // after this project has explicitly selected Agones, otherwise local Allocator servers would
    // continuously call a sidecar that is intentionally absent.
    Agones->HealthPing(Agones->HealthRateSeconds);
    Agones->Connect();
    UE_LOG(LogMahjongServer, Display, TEXT("Agones lifecycle connection started"));
}

void UGuiyangAgonesLifecycleSubsystem::Deinitialize()
{
    if (Agones)
    {
        Agones->ConnectedDelegate.RemoveDynamic(this, &ThisClass::HandleConnected);
    }
    RequestShutdown();
    Agones = nullptr;
    bActive = false;
    bReady = false;
    AllocationConfig.Reset();
    AllocationReady.Clear();
    Super::Deinitialize();
}

void UGuiyangAgonesLifecycleSubsystem::HandleConnected(const FGameServerResponse& Response)
{
    bReady = true;
    UE_LOG(LogMahjongServer, Display, TEXT("Agones GameServer ready Name=%s State=%s Address=%s"),
        *Response.ObjectMeta.Name, *Response.Status.State, *Response.Status.Address);
    HandleGameServerUpdated(Response);

    FSetPlayerCapacityDelegate Success;
    Success.BindDynamic(this, &ThisClass::HandleEmptySuccess);
    FAgonesErrorDelegate Error;
    Error.BindDynamic(this, &ThisClass::HandleError);
    Agones->SetPlayerCapacity(4, Success, Error);
}

void UGuiyangAgonesLifecycleSubsystem::HandleGameServerUpdated(const FGameServerResponse& Response)
{
    if (AllocationConfig.IsSet()
        || !Response.Status.State.Equals(TEXT("Allocated"), ESearchCase::IgnoreCase)) return;
    FGuiyangGameServerLaunchConfig Config;
    FString Error;
    if (!TryBuildLaunchConfig(
        Response,
        FPlatformMisc::GetEnvironmentVariable(TEXT("MAHJONG_JOIN_TICKET_SIGNING_KEY")),
        FPlatformMisc::GetEnvironmentVariable(TEXT("MAHJONG_MATCH_RESULT_OUTBOX_PATH")),
        Config,
        Error))
    {
        UE_LOG(LogMahjongServer, Error, TEXT("Agones allocation rejected: %s"), *Error);
        return;
    }
    AllocationConfig = Config;
    UE_LOG(LogMahjongServer, Display,
        TEXT("Agones allocation accepted InstanceId=%s RoomId=%s Address=%s:%d"),
        *Config.ServerInstanceId, *Config.RoomId, *Config.AdvertisedIp, Config.Port);
    AllocationReady.Broadcast(AllocationConfig.GetValue());
}

bool UGuiyangAgonesLifecycleSubsystem::TryGetAllocationConfig(
    FGuiyangGameServerLaunchConfig& OutConfig) const
{
    if (!AllocationConfig.IsSet()) return false;
    OutConfig = AllocationConfig.GetValue();
    return true;
}

void UGuiyangAgonesLifecycleSubsystem::NotifyPlayerConnected(const FString& PlayerId)
{
    if (!bActive || !bReady || !Agones || PlayerId.IsEmpty()) return;
    FPlayerConnectDelegate Success;
    Success.BindDynamic(this, &ThisClass::HandlePlayerConnected);
    FAgonesErrorDelegate Error;
    Error.BindDynamic(this, &ThisClass::HandleError);
    Agones->PlayerConnect(PlayerId, Success, Error);
}

void UGuiyangAgonesLifecycleSubsystem::NotifyPlayerDisconnected(const FString& PlayerId)
{
    if (!bActive || !Agones || PlayerId.IsEmpty()) return;
    FPlayerDisconnectDelegate Success;
    Success.BindDynamic(this, &ThisClass::HandlePlayerDisconnected);
    FAgonesErrorDelegate Error;
    Error.BindDynamic(this, &ThisClass::HandleError);
    Agones->PlayerDisconnect(PlayerId, Success, Error);
}

void UGuiyangAgonesLifecycleSubsystem::RequestShutdown()
{
    if (!bActive || bShutdownRequested || !Agones) return;
    bShutdownRequested = true;
    FShutdownDelegate Success;
    Success.BindDynamic(this, &ThisClass::HandleEmptySuccess);
    FAgonesErrorDelegate Error;
    Error.BindDynamic(this, &ThisClass::HandleError);
    Agones->Shutdown(Success, Error);
}

void UGuiyangAgonesLifecycleSubsystem::HandleError(const FAgonesError& Error)
{
    UE_LOG(LogMahjongServer, Error, TEXT("Agones lifecycle request failed: %s"), *Error.ErrorMessage);
}

void UGuiyangAgonesLifecycleSubsystem::HandleEmptySuccess(const FEmptyResponse& Response)
{
}

void UGuiyangAgonesLifecycleSubsystem::HandlePlayerConnected(const FConnectedResponse& Response)
{
    if (!Response.bConnected)
    {
        UE_LOG(LogMahjongServer, Warning, TEXT("Agones did not confirm player connection"));
    }
}

void UGuiyangAgonesLifecycleSubsystem::HandlePlayerDisconnected(const FDisconnectResponse& Response)
{
    if (!Response.bDisconnected)
    {
        UE_LOG(LogMahjongServer, Warning, TEXT("Agones did not confirm player disconnection"));
    }
}
