#pragma once

#include "CoreMinimal.h"
#include "GuiyangLobbyTypes.generated.h"

/** 大厅接入模式。LocalLegacy 保留现有 UE RPC 链路，RemoteLobby 使用独立大厅服务。 */
UENUM(BlueprintType)
enum class EGuiyangLobbyBackendMode : uint8
{
    LocalLegacy UMETA(DisplayName="本地兼容模式"),
    RemoteLobby UMETA(DisplayName="远程大厅模式")
};

/** Lobby API v1 的稳定错误码；中文提示仅用于显示，不参与程序分支。 */
UENUM(BlueprintType)
enum class EGuiyangLobbyErrorCode : uint8
{
    None,
    InvalidRequest,
    SessionExpired,
    RequestInProgress,
    RoomNotFound,
    RoomFull,
    RoomClosed,
    PasswordRequired,
    WrongPassword,
    RateLimited,
    ServerUnavailable,
    TicketExpired,
    VersionMismatch,
    Timeout,
    Cancelled,
    BackendNotConfigured,
    InternalError
};

/** 独立大厅中的房间生命周期；局内牌桌阶段仍由 GameServer 权威维护。 */
UENUM(BlueprintType)
enum class EGuiyangLobbyRoomLifecycle : uint8
{
    Creating,
    Allocating,
    Waiting,
    Playing,
    Settling,
    Closed,
    Failed
};

/** 大厅启动信息。客户端不得把在线人数或公告当作牌桌权威状态。 */
USTRUCT(BlueprintType)
struct GUIYANGMAHJONG_API FGuiyangLobbyBootstrap
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString RequestId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString PlayerId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 OnlinePlayerCount = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FString> Announcements;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ProtocolVersion = 1;
};

/** 大厅分配的 GameServer 路由。JoinTicket 属于短期敏感数据，禁止写入日志。 */
USTRUCT(BlueprintType)
struct GUIYANGMAHJONG_API FGuiyangGameServerRoute
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString RequestId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString PlayerId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString RoomId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString ServerInstanceId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString MatchId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString ServerIP;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ServerPort = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString JoinTicket;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FDateTime TicketExpireAtUtc;

    bool HasValidEndpoint() const
    {
        return !ServerIP.IsEmpty() && ServerIP.Len() <= 255
            && !ServerIP.Contains(TEXT("?")) && !ServerIP.Contains(TEXT("#"))
            && !ServerIP.Contains(TEXT("/")) && !ServerIP.Contains(TEXT("\\"))
            && !ServerIP.Contains(TEXT("@")) && !ServerIP.Contains(TEXT(" "))
            && ServerPort >= 1 && ServerPort <= 65535;
    }
};

/** 统一请求结果，错误码用于逻辑，ChineseMessage 用于界面。 */
USTRUCT(BlueprintType)
struct GUIYANGMAHJONG_API FGuiyangLobbyOperationResult
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAccepted = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString RequestId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EGuiyangLobbyErrorCode ErrorCode = EGuiyangLobbyErrorCode::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString ChineseMessage;
};
