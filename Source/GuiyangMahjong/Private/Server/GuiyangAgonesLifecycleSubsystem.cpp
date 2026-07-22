#include "Server/GuiyangAgonesLifecycleSubsystem.h"

#include "GuiyangMahjong.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

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
    Super::Deinitialize();
}

void UGuiyangAgonesLifecycleSubsystem::HandleConnected(const FGameServerResponse& Response)
{
    bReady = true;
    UE_LOG(LogMahjongServer, Display, TEXT("Agones GameServer ready Name=%s State=%s Address=%s"),
        *Response.ObjectMeta.Name, *Response.Status.State, *Response.Status.Address);

    FSetPlayerCapacityDelegate Success;
    Success.BindDynamic(this, &ThisClass::HandleEmptySuccess);
    FAgonesErrorDelegate Error;
    Error.BindDynamic(this, &ThisClass::HandleError);
    Agones->SetPlayerCapacity(4, Success, Error);
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

