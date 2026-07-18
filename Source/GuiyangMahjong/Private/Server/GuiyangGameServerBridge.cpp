#include "Server/GuiyangGameServerBridge.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Game/GuiyangMahjongGameMode.h"
#include "Game/GuiyangMahjongGameState.h"
#include "GameFramework/PlayerController.h"
#include "GuiyangMahjong.h"
#include "HttpModule.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Base64.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
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
    const FString& RegistrationCredential, const FString& MatchResultOutboxPath,
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
    OutConfig.MatchResultOutboxPath = MatchResultOutboxPath.TrimStartAndEnd();
    FPaths::NormalizeFilename(OutConfig.MatchResultOutboxPath);
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
        || OutConfig.JoinTicketSigningKey.Len() < 32
        || OutConfig.MatchResultOutboxPath.IsEmpty()
        || OutConfig.MatchResultOutboxPath.Len() > 1024
        || FPaths::IsRelative(OutConfig.MatchResultOutboxPath)
        || !FPaths::GetExtension(OutConfig.MatchResultOutboxPath).Equals(TEXT("json"), ESearchCase::IgnoreCase)
        || !FPaths::GetBaseFilename(OutConfig.MatchResultOutboxPath).Equals(
            OutConfig.ServerInstanceId, ESearchCase::IgnoreCase))
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
    ResultCredential.Reset();
    TicketValidator.Reset();
    if (World.IsValid())
    {
        World->GetTimerManager().ClearTimer(HeartbeatTimer);
        World->GetTimerManager().ClearTimer(MatchResultRetryTimer);
    }
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

void UGuiyangGameServerBridge::QueueFinalSettlement(
    const FMahjongFinalSettlementResult& Result, const int64 ResultSequence)
{
    if (!bRegistered || bShuttingDown || ResultCredential.Len() < 32
        || Result.MatchId != Config.MatchId || Result.RoomId != Config.RoomId
        || ResultSequence < 1 || Result.CompletedRounds < 1 || Result.CompletedRounds > 16
        || Result.Players.IsEmpty() || Result.Players.Num() > 4)
    {
        UE_LOG(LogMahjongServer, Error,
            TEXT("Final settlement report rejected locally InstanceId=%s MatchId=%s Sequence=%lld"),
            *Config.ServerInstanceId, *Result.MatchId, ResultSequence);
        return;
    }
    if (!PendingMatchResultBody.IsEmpty())
    {
        if (PendingMatchId == Result.MatchId && PendingResultSequence == ResultSequence) return;
        UE_LOG(LogMahjongServer, Error,
            TEXT("A different final settlement is already pending InstanceId=%s MatchId=%s"),
            *Config.ServerInstanceId, *Config.MatchId);
        return;
    }

    const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("roomId"), Result.RoomId);
    Body->SetStringField(TEXT("serverInstanceId"), Config.ServerInstanceId);
    Body->SetNumberField(TEXT("resultSequence"), static_cast<double>(ResultSequence));
    Body->SetNumberField(TEXT("completedRounds"), Result.CompletedRounds);
    TArray<TSharedPtr<FJsonValue>> Players;
    TSet<FString> PlayerIds;
    for (const FMahjongFinalPlayerResult& Player : Result.Players)
    {
        if (Player.PlayerId.IsEmpty() || Player.PlayerId.Len() > 80
            || Player.SeatIndex < 0 || Player.SeatIndex > 3
            || Player.Rank < 1 || Player.Rank > 4 || PlayerIds.Contains(Player.PlayerId))
        {
            UE_LOG(LogMahjongServer, Error,
                TEXT("Final settlement player data is invalid InstanceId=%s MatchId=%s"),
                *Config.ServerInstanceId, *Config.MatchId);
            return;
        }
        PlayerIds.Add(Player.PlayerId);
        const TSharedRef<FJsonObject> PlayerObject = MakeShared<FJsonObject>();
        PlayerObject->SetStringField(TEXT("playerId"), Player.PlayerId);
        PlayerObject->SetNumberField(TEXT("seatIndex"), Player.SeatIndex);
        PlayerObject->SetNumberField(TEXT("rank"), Player.Rank);
        PlayerObject->SetNumberField(TEXT("totalScore"), Player.TotalScore);
        Players.Add(MakeShared<FJsonValueObject>(PlayerObject));
    }
    Body->SetArrayField(TEXT("players"), Players);
    if (!PersistPendingMatchResult(Body))
    {
        UE_LOG(LogMahjongServer, Error,
            TEXT("Final settlement outbox persistence failed InstanceId=%s MatchId=%s"),
            *Config.ServerInstanceId, *Config.MatchId);
        return;
    }
    PendingMatchResultBody = GuiyangGameServerPrivate::SerializeJson(Body);
    PendingMatchId = Result.MatchId;
    PendingResultSequence = ResultSequence;
    MatchResultAttempt = 0;
    SendPendingMatchResult();
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
        || !Body->TryGetStringField(TEXT("resultCredential"), ResultCredential)
        || HeartbeatCredential.Len() < 32 || ResultCredential.Len() < 32)
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

void UGuiyangGameServerBridge::SendPendingMatchResult()
{
    if (bShuttingDown || !bRegistered || bMatchResultRequestInFlight
        || PendingMatchResultBody.IsEmpty() || ResultCredential.Len() < 32)
        return;
    bMatchResultRequestInFlight = true;
    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Config.LobbyInternalUrl + TEXT("/internal/matches/")
        + PendingMatchId + TEXT("/result"));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + ResultCredential);
    Request->SetHeader(TEXT("X-Request-Id"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
    Request->SetHeader(TEXT("Idempotency-Key"),
        FString::Printf(TEXT("%s:%lld"), *PendingMatchId, PendingResultSequence));
    Request->SetTimeout(10.0f);
    Request->SetContentAsString(PendingMatchResultBody);
    Request->OnProcessRequestComplete().BindUObject(this, &ThisClass::HandleMatchResultResponse);
    if (!Request->ProcessRequest())
    {
        Request->OnProcessRequestComplete().Unbind();
        bMatchResultRequestInFlight = false;
        ScheduleMatchResultRetry();
    }
}

void UGuiyangGameServerBridge::HandleMatchResultResponse(
    FHttpRequestPtr Request, FHttpResponsePtr Response, const bool bSucceeded)
{
    (void)Request;
    bMatchResultRequestInFlight = false;
    if (bShuttingDown || PendingMatchResultBody.IsEmpty()) return;
    TSharedPtr<FJsonObject> Body;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(
        Response.IsValid() ? Response->GetContentAsString() : FString());
    bool bAccepted = false;
    int64 AckSequence = 0;
    FString AckMatchId;
    if (bSucceeded && Response.IsValid() && Response->GetResponseCode() >= 200
        && Response->GetResponseCode() < 300 && FJsonSerializer::Deserialize(Reader, Body)
        && Body.IsValid() && Body->TryGetBoolField(TEXT("accepted"), bAccepted) && bAccepted
        && Body->TryGetStringField(TEXT("matchId"), AckMatchId)
        && Body->TryGetNumberField(TEXT("resultSequence"), AckSequence)
        && AckMatchId == PendingMatchId && AckSequence == PendingResultSequence)
    {
        UE_LOG(LogMahjongServer, Display,
            TEXT("Final settlement acknowledged InstanceId=%s MatchId=%s Sequence=%lld"),
            *Config.ServerInstanceId, *PendingMatchId, PendingResultSequence);
        DeletePersistedMatchResult();
        PendingMatchResultBody.Reset();
        PendingMatchId.Reset();
        PendingResultSequence = 0;
        MatchResultAttempt = 0;
        return;
    }
    UE_LOG(LogMahjongServer, Warning,
        TEXT("Final settlement report will retry InstanceId=%s MatchId=%s Sequence=%lld Status=%d"),
        *Config.ServerInstanceId, *PendingMatchId, PendingResultSequence,
        Response.IsValid() ? Response->GetResponseCode() : 0);
    ScheduleMatchResultRetry();
}

void UGuiyangGameServerBridge::ScheduleMatchResultRetry()
{
    if (bShuttingDown || !World.IsValid() || PendingMatchResultBody.IsEmpty()) return;
    ++MatchResultAttempt;
    const float DelaySeconds = FMath::Min(30.0f,
        static_cast<float>(1 << FMath::Min(MatchResultAttempt - 1, 5)));
    World->GetTimerManager().SetTimer(
        MatchResultRetryTimer, this, &ThisClass::SendPendingMatchResult, DelaySeconds, false);
}

bool UGuiyangGameServerBridge::PersistPendingMatchResult(const TSharedRef<FJsonObject>& Report) const
{
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (PlatformFile.FileExists(*Config.MatchResultOutboxPath)) return false;

    const TSharedRef<FJsonObject> Envelope = MakeShared<FJsonObject>();
    Envelope->SetNumberField(TEXT("version"), 1);
    Envelope->SetStringField(TEXT("matchId"), Config.MatchId);
    Envelope->SetObjectField(TEXT("report"), Report);
    const FString TemporaryPath = Config.MatchResultOutboxPath + TEXT(".tmp");
    PlatformFile.DeleteFile(*TemporaryPath);
    if (!FFileHelper::SaveStringToFile(
            GuiyangGameServerPrivate::SerializeJson(Envelope), *TemporaryPath,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        return false;
    }
    if (!PlatformFile.MoveFile(*Config.MatchResultOutboxPath, *TemporaryPath))
    {
        PlatformFile.DeleteFile(*TemporaryPath);
        return false;
    }
    return true;
}

void UGuiyangGameServerBridge::DeletePersistedMatchResult() const
{
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (PlatformFile.FileExists(*Config.MatchResultOutboxPath)
        && !PlatformFile.DeleteFile(*Config.MatchResultOutboxPath))
    {
        UE_LOG(LogMahjongServer, Warning,
            TEXT("Acknowledged final settlement outbox could not be deleted InstanceId=%s MatchId=%s"),
            *Config.ServerInstanceId, *Config.MatchId);
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
