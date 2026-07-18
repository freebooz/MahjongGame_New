#pragma once

#include "CoreMinimal.h"
#include "Lobby/GuiyangLobbyBackend.h"

class FJsonObject;
class UGuiyangLobbySubsystem;
struct FMahjongCreateRoomRequest;

struct GUIYANGMAHJONG_API FGuiyangRemoteLobbySettings
{
    FString BaseUrl;
    float RequestTimeoutSeconds = 10.0f;
    float RoutePollIntervalSeconds = 0.25f;
    int32 RoutePollMaxAttempts = 120;
};

/** 无网络副作用的 RemoteLobby v1 编解码器，供运行时和契约测试共用。 */
struct GUIYANGMAHJONG_API FGuiyangRemoteLobbyCodec
{
    static bool NormalizeBaseUrl(const FString& Value, FString& OutBaseUrl);
    static FString SerializeCreateRoom(const FMahjongCreateRoomRequest& Request);
    static FString SerializeReconnectRouteRequest(const FString& RoomId, const FString& MatchId);
    static bool TryParseBootstrap(const FString& Json, FGuiyangLobbyBootstrap& OutBootstrap);
    static bool TryParseRoute(const FString& Json, const FString& ExpectedPlayerId,
        FGuiyangGameServerRoute& OutRoute);
    static EGuiyangLobbyErrorCode MapErrorCode(const FString& StableCode);
};

GUIYANGMAHJONG_API TSharedPtr<ILobbyBackend> CreateRemoteLobbyBackend(
    UGuiyangLobbySubsystem& Owner, const FGuiyangRemoteLobbySettings& Settings);
