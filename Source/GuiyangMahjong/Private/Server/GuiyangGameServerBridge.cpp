#include "Server/GuiyangGameServerBridge.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Game/GuiyangMahjongGameMode.h"
#include "Game/GuiyangMahjongGameState.h"
#include "GameFramework/PlayerController.h"
#include "GuiyangMahjong.h"
#include "HttpModule.h"
#include "Misc/Base64.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"
#include "Room/GuiyangManagedRoomDefinition.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "TimerManager.h"

THIRD_PARTY_INCLUDES_START
#define UI OPENSSL_UI
#include <openssl/evp.h>
#include <openssl/hmac.h>
#undef UI
THIRD_PARTY_INCLUDES_END

namespace GuiyangGameServerPrivate
{
    bool ReadRequiredValue(const TCHAR* CommandLine, const TCHAR* Match, FString& OutValue)
    {
        return FParse::Value(CommandLine, Match, OutValue) && !OutValue.TrimStartAndEnd().IsEmpty();
    }

    bool Base64UrlDecode(const FString& Value, TArray<uint8>& OutBytes)
    {
        FString Normalized = Value;
        Normalized.ReplaceInline(TEXT("-"), TEXT("+"));
        Normalized.ReplaceInline(TEXT("_"), TEXT("/"));
        while (Normalized.Len() % 4 != 0) Normalized.AppendChar(TEXT('='));
        return FBase64::Decode(Normalized, OutBytes);
    }

    bool ComputeHmacSha256(const FString& Key, const FString& Data, TArray<uint8>& OutDigest)
    {
        FTCHARToUTF8 KeyUtf8(*Key);
        FTCHARToUTF8 DataUtf8(*Data);
        OutDigest.SetNumZeroed(32);
        unsigned int DigestLength = 0;
        const unsigned char* Result = HMAC(
            EVP_sha256(),
            KeyUtf8.Get(),
            KeyUtf8.Length(),
            reinterpret_cast<const unsigned char*>(DataUtf8.Get()),
            static_cast<size_t>(DataUtf8.Length()),
            OutDigest.GetData(),
            &DigestLength);
        if (!Result || DigestLength != 32)
        {
            OutDigest.Reset();
            return false;
        }
        return true;
    }

    bool ConstantTimeEquals(const TArray<uint8>& Left, const TArray<uint8>& Right)
    {
        uint32 Difference = static_cast<uint32>(Left.Num() ^ Right.Num());
        const int32 Count = FMath::Max(Left.Num(), Right.Num());
        for (int32 Index = 0; Index < Count; ++Index)
        {
            const uint8 LeftByte = Left.IsValidIndex(Index) ? Left[Index] : 0;
            const uint8 RightByte = Right.IsValidIndex(Index) ? Right[Index] : 0;
            Difference |= static_cast<uint32>(LeftByte ^ RightByte);
        }
        return Difference == 0;
    }

    FString SerializeJson(const TSharedRef<FJsonObject>& Object)
    {
        FString Body;
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
        FJsonSerializer::Serialize(Object, Writer);
        return Body;
    }
}

bool FGuiyangGameServerLaunchConfig::TryParse(const TCHAR* CommandLine, const FString& SigningKey,
    const FString& RegistrationCredential,
    FGuiyangGameServerLaunchConfig& OutConfig, FString& OutError)
{
    OutConfig = FGuiyangGameServerLaunchConfig();
    if (!FParse::Param(CommandLine, TEXT("MahjongManagedGameServer")))
    {
        OutError = TEXT("Managed GameServer flag is missing");
        return false;
    }
    if (!GuiyangGameServerPrivate::ReadRequiredValue(CommandLine, TEXT("RoomId="), OutConfig.RoomId)
        || !GuiyangGameServerPrivate::ReadRequiredValue(CommandLine, TEXT("MatchId="), OutConfig.MatchId)
        || !GuiyangGameServerPrivate::ReadRequiredValue(
            CommandLine, TEXT("ServerInstanceId="), OutConfig.ServerInstanceId)
        || !GuiyangGameServerPrivate::ReadRequiredValue(
            CommandLine, TEXT("LobbyInternalUrl="), OutConfig.LobbyInternalUrl)
        || !GuiyangGameServerPrivate::ReadRequiredValue(
            CommandLine, TEXT("BuildVersion="), OutConfig.BuildVersion)
        || !GuiyangGameServerPrivate::ReadRequiredValue(
            CommandLine, TEXT("AdvertisedIp="), OutConfig.AdvertisedIp)
        || !FParse::Value(CommandLine, TEXT("Port="), OutConfig.Port))
    {
        OutError = TEXT("Managed GameServer launch arguments are incomplete");
        return false;
    }

    OutConfig.RoomId.TrimStartAndEndInline();
    OutConfig.MatchId.TrimStartAndEndInline();
    OutConfig.ServerInstanceId.TrimStartAndEndInline();
    OutConfig.LobbyInternalUrl.TrimStartAndEndInline();
    OutConfig.LobbyInternalUrl.RemoveFromEnd(TEXT("/"));
    OutConfig.RegistrationCredential = RegistrationCredential;
    OutConfig.RegistrationCredential.TrimStartAndEndInline();
    OutConfig.BuildVersion.TrimStartAndEndInline();
    OutConfig.AdvertisedIp.TrimStartAndEndInline();
    OutConfig.JoinTicketSigningKey = SigningKey;
    FGuid ParsedGuid;
    if (!FGuid::Parse(OutConfig.RoomId, ParsedGuid)
        || !FGuid::Parse(OutConfig.MatchId, ParsedGuid)
        || !FGuid::Parse(OutConfig.ServerInstanceId, ParsedGuid)
        || OutConfig.Port < 1024 || OutConfig.Port > 65535
        || (!OutConfig.LobbyInternalUrl.StartsWith(TEXT("http://"))
            && !OutConfig.LobbyInternalUrl.StartsWith(TEXT("https://")))
        || OutConfig.BuildVersion.Len() > 80
        || OutConfig.AdvertisedIp.Len() > 255
        || OutConfig.RegistrationCredential.Len() < 32
        || OutConfig.JoinTicketSigningKey.Len() < 32)
    {
        OutError = TEXT("Managed GameServer launch arguments failed validation");
        return false;
    }
    return true;
}

FGuiyangJoinTicketValidator::FGuiyangJoinTicketValidator(const FGuiyangGameServerLaunchConfig& Config)
    : SigningKey(Config.JoinTicketSigningKey)
    , ExpectedRoomId(Config.RoomId)
    , ExpectedMatchId(Config.MatchId)
    , ExpectedServerInstanceId(Config.ServerInstanceId)
{
}

bool FGuiyangJoinTicketValidator::ValidateAndConsume(const FString& Ticket, const FString& SuppliedPlayerId,
    const int64 NowUnixSeconds, FGuiyangJoinTicketClaims& OutClaims, FString& OutError)
{
    if (Ticket.IsEmpty() || Ticket.Len() > 4096 || SuppliedPlayerId.IsEmpty())
    {
        OutError = TEXT("JOIN_TICKET_INVALID");
        return false;
    }

    FString EncodedPayload;
    FString EncodedSignature;
    if (!Ticket.Split(TEXT("."), &EncodedPayload, &EncodedSignature)
        || EncodedPayload.IsEmpty() || EncodedSignature.IsEmpty())
    {
        OutError = TEXT("JOIN_TICKET_INVALID");
        return false;
    }

    TArray<uint8> SuppliedSignature;
    TArray<uint8> ExpectedSignature;
    if (!GuiyangGameServerPrivate::Base64UrlDecode(EncodedSignature, SuppliedSignature)
        || !GuiyangGameServerPrivate::ComputeHmacSha256(SigningKey, EncodedPayload, ExpectedSignature)
        || !GuiyangGameServerPrivate::ConstantTimeEquals(SuppliedSignature, ExpectedSignature))
    {
        OutError = TEXT("JOIN_TICKET_SIGNATURE_INVALID");
        return false;
    }

    TArray<uint8> PayloadBytes;
    if (!GuiyangGameServerPrivate::Base64UrlDecode(EncodedPayload, PayloadBytes))
    {
        OutError = TEXT("JOIN_TICKET_INVALID");
        return false;
    }
    PayloadBytes.Add(0);
    const FString PayloadJson = UTF8_TO_TCHAR(reinterpret_cast<const ANSICHAR*>(PayloadBytes.GetData()));
    TSharedPtr<FJsonObject> Payload;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PayloadJson);
    if (!FJsonSerializer::Deserialize(Reader, Payload) || !Payload.IsValid()
        || !Payload->TryGetStringField(TEXT("playerId"), OutClaims.PlayerId)
        || !Payload->TryGetStringField(TEXT("roomId"), OutClaims.RoomId)
        || !Payload->TryGetStringField(TEXT("matchId"), OutClaims.MatchId)
        || !Payload->TryGetStringField(TEXT("serverInstanceId"), OutClaims.ServerInstanceId)
        || !Payload->TryGetStringField(TEXT("nonce"), OutClaims.Nonce)
        || !Payload->TryGetNumberField(TEXT("expiresAtUnixSeconds"), OutClaims.ExpiresAtUnixSeconds))
    {
        OutError = TEXT("JOIN_TICKET_INVALID");
        return false;
    }

    if (OutClaims.PlayerId != SuppliedPlayerId
        || OutClaims.RoomId != ExpectedRoomId
        || OutClaims.MatchId != ExpectedMatchId
        || OutClaims.ServerInstanceId != ExpectedServerInstanceId)
    {
        OutError = TEXT("JOIN_TICKET_SCOPE_MISMATCH");
        return false;
    }
    if (OutClaims.ExpiresAtUnixSeconds <= NowUnixSeconds
        || OutClaims.ExpiresAtUnixSeconds > NowUnixSeconds + 120)
    {
        OutError = TEXT("JOIN_TICKET_EXPIRED");
        return false;
    }
    if (OutClaims.Nonce.Len() < 16 || OutClaims.Nonce.Len() > 128 || UsedNonces.Contains(OutClaims.Nonce))
    {
        OutError = TEXT("JOIN_TICKET_REPLAYED");
        return false;
    }

    for (auto It = UsedNonces.CreateIterator(); It; ++It)
    {
        if (It.Value() <= NowUnixSeconds) It.RemoveCurrent();
    }
    UsedNonces.Add(OutClaims.Nonce, OutClaims.ExpiresAtUnixSeconds);
    return true;
}

bool UGuiyangGameServerBridge::Initialize(
    UWorld* InWorld, const FGuiyangGameServerLaunchConfig& InConfig, FString& OutError)
{
    if (!InWorld || !IsRunningDedicatedServer())
    {
        OutError = TEXT("Managed bridge requires a Dedicated Server world");
        return false;
    }
    World = InWorld;
    Config = InConfig;
    TicketValidator = MakeUnique<FGuiyangJoinTicketValidator>(Config);
    SendRegistration();
    return true;
}

void UGuiyangGameServerBridge::Shutdown()
{
    bShuttingDown = true;
    bRegistered = false;
    HeartbeatCredential.Reset();
    TicketValidator.Reset();
    if (World.IsValid()) World->GetTimerManager().ClearTimer(HeartbeatTimer);
}

void UGuiyangGameServerBridge::BeginDestroy()
{
    Shutdown();
    Super::BeginDestroy();
}

bool UGuiyangGameServerBridge::ValidateAndConsumeJoinTicket(const FString& Ticket, const FString& PlayerId,
    FGuiyangJoinTicketClaims& OutClaims, FString& OutError)
{
    if (!bRegistered || !TicketValidator)
    {
        OutError = TEXT("GAMESERVER_NOT_REGISTERED");
        return false;
    }
    return TicketValidator->ValidateAndConsume(
        Ticket, PlayerId, FDateTime::UtcNow().ToUnixTimestamp(), OutClaims, OutError);
}

void UGuiyangGameServerBridge::SendRegistration()
{
    const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("serverInstanceId"), Config.ServerInstanceId);
    Body->SetStringField(TEXT("roomId"), Config.RoomId);
    Body->SetStringField(TEXT("matchId"), Config.MatchId);
    Body->SetStringField(TEXT("listenIp"), Config.AdvertisedIp);
    Body->SetNumberField(TEXT("listenPort"), Config.Port);
    Body->SetStringField(TEXT("buildVersion"), Config.BuildVersion);
    Body->SetStringField(TEXT("registrationCredential"), Config.RegistrationCredential);

    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Config.LobbyInternalUrl + TEXT("/internal/gameservers/register"));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("X-Request-Id"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
    Request->SetContentAsString(GuiyangGameServerPrivate::SerializeJson(Body));
    Request->OnProcessRequestComplete().BindUObject(this, &ThisClass::HandleRegistrationResponse);
    Request->ProcessRequest();
}

void UGuiyangGameServerBridge::HandleRegistrationResponse(
    FHttpRequestPtr Request, FHttpResponsePtr Response, const bool bSucceeded)
{
    (void)Request;
    if (bShuttingDown) return;
    TSharedPtr<FJsonObject> Body;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(
        Response.IsValid() ? Response->GetContentAsString() : FString());
    bool bAccepted = false;
    if (!bSucceeded || !Response.IsValid() || Response->GetResponseCode() < 200
        || Response->GetResponseCode() >= 300 || !FJsonSerializer::Deserialize(Reader, Body)
        || !Body.IsValid() || !Body->TryGetBoolField(TEXT("accepted"), bAccepted) || !bAccepted
        || !Body->TryGetNumberField(TEXT("heartbeatIntervalSeconds"), HeartbeatIntervalSeconds)
        || !Body->TryGetStringField(TEXT("heartbeatCredential"), HeartbeatCredential)
        || HeartbeatCredential.Len() < 32)
    {
        UE_LOG(LogMahjongServer, Error,
            TEXT("Managed GameServer registration failed InstanceId=%s RoomId=%s Status=%d"),
            *Config.ServerInstanceId, *Config.RoomId, Response.IsValid() ? Response->GetResponseCode() : 0);
        return;
    }

    const TSharedPtr<FJsonObject>* BootstrapPointer = nullptr;
    FGuiyangManagedRoomDefinition Definition;
    FString BootstrapError;
    AGuiyangMahjongGameMode* GameMode = World.IsValid()
        ? World->GetAuthGameMode<AGuiyangMahjongGameMode>() : nullptr;
    if (!Body->TryGetObjectField(TEXT("roomBootstrap"), BootstrapPointer)
        || !BootstrapPointer
        || !FGuiyangManagedRoomDefinition::TryParse(*BootstrapPointer,
            Config.RoomId, Config.MatchId, Definition, BootstrapError)
        || !GameMode
        || !GameMode->InitializeManagedRoomAuthority(Definition, BootstrapError))
    {
        HeartbeatCredential.Reset();
        UE_LOG(LogMahjongServer, Error,
            TEXT("Managed GameServer bootstrap rejected InstanceId=%s RoomId=%s Reason=%s"),
            *Config.ServerInstanceId, *Config.RoomId,
            BootstrapError.IsEmpty() ? TEXT("ROOM_BOOTSTRAP_INVALID") : *BootstrapError);
        return;
    }

    Config.RegistrationCredential.Reset();
    HeartbeatIntervalSeconds = FMath::Clamp(HeartbeatIntervalSeconds, 1, 60);
    bRegistered = true;
    if (World.IsValid())
    {
        World->GetTimerManager().SetTimer(
            HeartbeatTimer, this, &ThisClass::SendHeartbeat,
            static_cast<float>(HeartbeatIntervalSeconds), true,
            static_cast<float>(HeartbeatIntervalSeconds));
    }
    UE_LOG(LogMahjongServer, Display,
        TEXT("Managed GameServer registered InstanceId=%s RoomId=%s Port=%d"),
        *Config.ServerInstanceId, *Config.RoomId, Config.Port);
}

void UGuiyangGameServerBridge::SendHeartbeat()
{
    if (!bRegistered || bShuttingDown || !World.IsValid()) return;
    int32 RoundId = 0;
    const FString Lifecycle = BuildHeartbeatLifecycle(RoundId);
    int32 ConnectedPlayers = 0;
    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        ConnectedPlayers += It->IsValid() ? 1 : 0;
    }

    const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("roomId"), Config.RoomId);
    Body->SetStringField(TEXT("heartbeatCredential"), HeartbeatCredential);
    Body->SetNumberField(TEXT("connectedPlayers"), ConnectedPlayers);
    Body->SetStringField(TEXT("roomLifecycle"), Lifecycle);
    Body->SetNumberField(TEXT("roundId"), RoundId);
    Body->SetStringField(TEXT("buildVersion"), Config.BuildVersion);
    Body->SetStringField(TEXT("sentAtUtc"), FDateTime::UtcNow().ToIso8601());

    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Config.LobbyInternalUrl + TEXT("/internal/gameservers/")
        + Config.ServerInstanceId + TEXT("/heartbeat"));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("X-Request-Id"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
    Request->SetContentAsString(GuiyangGameServerPrivate::SerializeJson(Body));
    Request->OnProcessRequestComplete().BindUObject(this, &ThisClass::HandleHeartbeatResponse);
    Request->ProcessRequest();
}

void UGuiyangGameServerBridge::HandleHeartbeatResponse(
    FHttpRequestPtr Request, FHttpResponsePtr Response, const bool bSucceeded)
{
    (void)Request;
    if (!bShuttingDown && (!bSucceeded || !Response.IsValid()
        || Response->GetResponseCode() < 200 || Response->GetResponseCode() >= 300))
    {
        UE_LOG(LogMahjongServer, Warning,
            TEXT("Managed GameServer heartbeat failed InstanceId=%s Status=%d"),
            *Config.ServerInstanceId, Response.IsValid() ? Response->GetResponseCode() : 0);
    }
}

FString UGuiyangGameServerBridge::BuildHeartbeatLifecycle(int32& OutRoundId) const
{
    OutRoundId = 0;
    const AGuiyangMahjongGameState* State = World.IsValid()
        ? World->GetGameState<AGuiyangMahjongGameState>() : nullptr;
    if (!State) return TEXT("Waiting");
    OutRoundId = State->PublicTableState.RoundId;
    switch (State->RoomState.Lifecycle)
    {
    case EMahjongRoomLifecycle::Playing: return TEXT("Playing");
    case EMahjongRoomLifecycle::Settlement: return TEXT("Settling");
    case EMahjongRoomLifecycle::Closed: return TEXT("Closed");
    default: return TEXT("Waiting");
    }
}
