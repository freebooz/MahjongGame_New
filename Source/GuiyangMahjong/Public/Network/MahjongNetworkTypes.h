#pragma once

#include "CoreMinimal.h"
#include "Core/MahjongTypes.h"
#include "Rules/GuiyangRuleSnapshot.h"
#include "MahjongNetworkTypes.generated.h"

UENUM(BlueprintType)
enum class EMahjongRoomLifecycle : uint8
{
    Creating,
    WaitingForPlayers,
    ReadyCheck,
    Starting,
    Playing,
    Settlement,
    WaitingNextRound,
    Closing,
    Closed
};

UENUM(BlueprintType)
enum class EMahjongRoomError : uint8
{
    None,
    SessionExpired,
    InvalidRequest,
    AlreadyInRoom,
    RoomNotFound,
    RoomClosed,
    RoomFull,
    GameAlreadyStarted,
    PasswordRequired,
    WrongPassword,
    TooManyPasswordAttempts,
    NotRoomOwner,
    NotInRoom,
    VersionMismatch
};

USTRUCT(BlueprintType)
struct GUIYANGMAHJONG_API FMahjongCreateRoomRequest
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RoundCount = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMahjongRuleConfig Rules;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bPublicRoom = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAutoStart = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEnablePassword = false;
    /** 仅用于 Client->Server 请求，不得复制到 GameState 或写入日志。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Password;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ClientSequence = 0;
};

USTRUCT(BlueprintType)
struct GUIYANGMAHJONG_API FMahjongJoinRoomRequest
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString RoomCode;
    /** 仅用于 Client->Server 请求，不得复制到 GameState 或写入日志。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Password;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ClientSequence = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ClientProtocolVersion = 1;
};

USTRUCT(BlueprintType)
struct GUIYANGMAHJONG_API FMahjongSeatInfo
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SeatIndex = INDEX_NONE;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString PlayerId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString PlayerName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bOwner = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bOccupied = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bReady = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bOnline = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 HandTileCount = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Score = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PingMilliseconds = 0;
};

USTRUCT(BlueprintType)
struct GUIYANGMAHJONG_API FMahjongRoomInfo
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString RoomId = TEXT("100001");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString RuleSummary = TEXT("贵阳捉鸡·四人房");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaxPlayers = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString OwnerPlayerId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RoundCount = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 CurrentRound = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 DealerSeat = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 BaseScore = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bPublicRoom = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bPasswordProtected = false;
};

USTRUCT(BlueprintType)
struct GUIYANGMAHJONG_API FMahjongRoomState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMahjongRoomInfo RoomInfo;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuiyangRuleSnapshot RuleSnapshot;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EMahjongRoomLifecycle Lifecycle = EMahjongRoomLifecycle::WaitingForPlayers;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FMahjongSeatInfo> Seats;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bGameStarting = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 StateSequence = 0;
};

USTRUCT(BlueprintType)
struct GUIYANGMAHJONG_API FMahjongFinalPlayerResult
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Rank = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SeatIndex = INDEX_NONE;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString PlayerName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 TotalScore = 0;
};

USTRUCT(BlueprintType)
struct GUIYANGMAHJONG_API FMahjongFinalSettlementResult
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString RoomId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 CompletedRounds = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FMahjongFinalPlayerResult> Players;
};

/** 可公开复制给所有客户端的牌桌快照，严格不包含手牌内容和牌墙顺序。 */
USTRUCT(BlueprintType)
struct GUIYANGMAHJONG_API FMahjongPublicTableState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RoundId = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 TurnId = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ServerActionSequence = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EMahjongTablePhase Phase = EMahjongTablePhase::WaitingForPlayers;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 CurrentTurnSeat = INDEX_NONE;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RemainingTileCount = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ActionTimeoutSeconds = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double ActionDeadlineServerTimeSeconds = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FMahjongSeatInfo> Seats;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FMahjongDiscardRecord> Discards;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FMahjongMeld> PublicMelds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<int32> WinningSeats;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMahjongTile LastDiscard;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMahjongTile FlippedJiTile;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FMahjongJiEvent> JiEvents;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 StateSequence = 0;
};

/** 仅通过所属 PlayerController 的 Client RPC 下发给单个玩家。 */
USTRUCT(BlueprintType)
struct GUIYANGMAHJONG_API FMahjongPrivatePlayerState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RoundId = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 TurnId = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SeatIndex = INDEX_NONE;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 LastAcceptedClientSequence = -1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMahjongHand Hand;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 StateSequence = 0;
};

USTRUCT(BlueprintType)
struct GUIYANGMAHJONG_API FMahjongReconnectSnapshot
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMahjongRoomState RoomState;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMahjongPublicTableState TableState;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMahjongPrivatePlayerState PrivateState;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RemainingReconnectSeconds = 0;
};
