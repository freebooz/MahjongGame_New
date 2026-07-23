#pragma once

#include "CoreMinimal.h"

struct FGuiyangGameServerLaunchConfig;

/** Verified, server-authoritative claims carried by a short-lived Lobby join ticket. */
struct GUIYANGMAHJONGSERVER_API FGuiyangJoinTicketClaims
{
    FString PlayerId;
    FString RoomId;
    FString MatchId;
    FString ServerInstanceId;
    FString Nonce;
    int64 ExpiresAtUnixSeconds = 0;
};

/** HMAC verifier with scope, expiry and one-time nonce enforcement. */
class GUIYANGMAHJONGSERVER_API FGuiyangJoinTicketValidator
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

