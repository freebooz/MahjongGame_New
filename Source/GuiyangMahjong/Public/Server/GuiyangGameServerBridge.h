#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
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
    int32 Port = 0;

    static bool TryParse(const TCHAR* CommandLine, const FString& SigningKey,
        const FString& RegistrationCredential,
        FGuiyangGameServerLaunchConfig& OutConfig, FString& OutError);
};

struct GUIYANGMAHJONG_API FGuiyangJoinTicketClaims
{
    FString PlayerId;
    FString RoomId;
    FString MatchId;
    FString ServerInstanceId;
    FString Nonce;
    int64 ExpiresAtUnixSeconds = 0;
};

class GUIYANGMAHJONG_API FGuiyangJoinTicketValidator
{
public:
    explicit FGuiyangJoinTicketValidator(const FGuiyangGameServerLaunchConfig& Config);

    bool ValidateAndConsume(const FString& Ticket, const FString& SuppliedPlayerId,
        int64 NowUnixSeconds, FGuiyangJoinTicketClaims& OutClaims, FString& OutError);

private:
    FString SigningKey;
    FString ExpectedRoomId;
    FString ExpectedMatchId;
    FString ExpectedServerInstanceId;
    TMap<FString, int64> UsedNonces;
};

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
    const FGuiyangGameServerLaunchConfig& GetConfig() const { return Config; }

private:
    void SendRegistration();
    void HandleRegistrationResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSucceeded);
    void SendHeartbeat();
    void HandleHeartbeatResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSucceeded);
    FString BuildHeartbeatLifecycle(int32& OutRoundId) const;

    TWeakObjectPtr<UWorld> World;
    FGuiyangGameServerLaunchConfig Config;
    TUniquePtr<FGuiyangJoinTicketValidator> TicketValidator;
    FString HeartbeatCredential;
    FTimerHandle HeartbeatTimer;
    int32 HeartbeatIntervalSeconds = 3;
    bool bRegistered = false;
    bool bShuttingDown = false;
};
