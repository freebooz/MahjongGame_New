#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Server/GuiyangServerTicketVerifier.h"
#include "UObject/Object.h"
#include "GuiyangGameServerBridge.generated.h"

struct GUIYANGMAHJONG_API FGuiyangGameServerLaunchConfig
{
    FString RoomId;
    FString MatchId;
    FString ServerInstanceId;
    FString LobbyInternalUrl;
    FString RegistrationCredential;
    FString BuildVersion;
    FString AdvertisedIp;
    FString JoinTicketSigningKey;
    FString MatchResultOutboxPath;
    int32 Port = 0;

    static bool TryParse(const TCHAR* CommandLine, const FString& SigningKey,
        const FString& RegistrationCredential, const FString& MatchResultOutboxPath,
        FGuiyangGameServerLaunchConfig& OutConfig, FString& OutError);
};

struct FMahjongFinalSettlementResult;

UCLASS()
class GUIYANGMAHJONG_API UGuiyangGameServerBridge final : public UObject
{
    GENERATED_BODY()

public:
    bool Initialize(UWorld* InWorld, const FGuiyangGameServerLaunchConfig& InConfig, FString& OutError);
    void Shutdown();
    virtual void BeginDestroy() override;

    bool IsRegistered() const { return bRegistered; }
    bool ValidateAndConsumeJoinTicket(const FString& Ticket, const FString& PlayerId,
        FGuiyangJoinTicketClaims& OutClaims, FString& OutError);
    void QueueFinalSettlement(const FMahjongFinalSettlementResult& Result, int64 ResultSequence);
    const FGuiyangGameServerLaunchConfig& GetConfig() const { return Config; }

private:
    void SendRegistration();
    void HandleRegistrationResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSucceeded);
    void SendHeartbeat();
    void HandleHeartbeatResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSucceeded);
    void SendPendingMatchResult();
    void HandleMatchResultResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSucceeded);
    void ScheduleMatchResultRetry();
    bool PersistPendingMatchResult(const TSharedRef<FJsonObject>& Report) const;
    void DeletePersistedMatchResult() const;
    FString BuildHeartbeatLifecycle(int32& OutRoundId) const;

    TWeakObjectPtr<UWorld> World;
    FGuiyangGameServerLaunchConfig Config;
    TUniquePtr<FGuiyangJoinTicketValidator> TicketValidator;
    FString HeartbeatCredential;
    FString ResultCredential;
    FString PendingMatchResultBody;
    FString PendingMatchId;
    FTimerHandle HeartbeatTimer;
    FTimerHandle MatchResultRetryTimer;
    int32 HeartbeatIntervalSeconds = 3;
    int64 PendingResultSequence = 0;
    int32 MatchResultAttempt = 0;
    bool bRegistered = false;
    bool bShuttingDown = false;
    bool bMatchResultRequestInFlight = false;
};
