#pragma once

#include "CoreMinimal.h"
#include "MahjongTypes.generated.h"

UENUM(BlueprintType)
enum class EMahjongSuit : uint8
{
    Characters UMETA(DisplayName="万"),
    Bamboo UMETA(DisplayName="条"),
    Dots UMETA(DisplayName="筒"),
    Winds UMETA(DisplayName="风"),
    Dragons UMETA(DisplayName="箭牌")
};

UENUM(BlueprintType)
enum class EMahjongTileType : uint8
{
    Number,
    East, South, West, North,
    RedDragon, GreenDragon, WhiteDragon,
    Invalid
};

UENUM(BlueprintType)
enum class EMahjongActionType : uint8
{
    Draw, Play, Peng, MingGang, AnGang, BuGang, Hu, Pass
};

UENUM(BlueprintType)
enum class EMahjongTablePhase : uint8
{
    WaitingForPlayers,
    PreparingGame,
    Dealing,
    PlayerTurn,
    WaitingForAction,
    ResolvingAction,
    Settlement,
    GameOver,
    Restarting
};

UENUM(BlueprintType)
enum class EMahjongMeldType : uint8
{
    Chi, Peng, MingGang, AnGang, BuGang
};

UENUM(BlueprintType)
enum class EMahjongJiCountingScope : uint8
{
    HandOnly,
    HandAndMeld,
    HandAndDiscard,
    HandMeldAndDiscard
};

UENUM(BlueprintType)
enum class EMahjongJiEventType : uint8
{
    ChongFeng,
    ZeRen
};

/** 牌墙组成。贵阳主流规则默认只使用万、条、筒三门 108 张牌。 */
UENUM(BlueprintType)
enum class EMahjongTileSetMode : uint8
{
    Suited108 UMETA(DisplayName="三门数牌（108 张）"),
    Standard136 UMETA(DisplayName="标准牌（136 张，含风牌和箭牌）")
};

/** 一张麻将牌。UniqueId 在单局牌墙中稳定且唯一，规则判断使用 Suit/Rank 而不是显示文本。 */
USTRUCT(BlueprintType)
struct GUIYANGMAHJONG_API FMahjongTile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) EMahjongSuit Suit = EMahjongSuit::Characters;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EMahjongTileType Type = EMahjongTileType::Invalid;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="9")) int32 Rank = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 UniqueId = INDEX_NONE;

    bool IsValid() const { return UniqueId >= 0 && Type != EMahjongTileType::Invalid; }
    int32 GetRuleIndex() const;
    FString ToDebugString() const;
    bool operator==(const FMahjongTile& Other) const { return UniqueId == Other.UniqueId; }
};

USTRUCT(BlueprintType)
struct GUIYANGMAHJONG_API FMahjongMeld
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EMahjongMeldType Type = EMahjongMeldType::Peng;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FMahjongTile> Tiles;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 OwnerSeat = INDEX_NONE;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 FromSeat = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct GUIYANGMAHJONG_API FMahjongHand
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FMahjongTile> Tiles;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FMahjongMeld> Melds;
    FString ToDebugString() const;
    void Sort();
};

USTRUCT(BlueprintType)
struct GUIYANGMAHJONG_API FMahjongDiscardRecord
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SeatIndex = INDEX_NONE;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMahjongTile Tile;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Sequence = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bClaimed = false;
};

USTRUCT(BlueprintType)
struct GUIYANGMAHJONG_API FMahjongAction
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EMahjongActionType Type = EMahjongActionType::Pass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SourceSeat = INDEX_NONE;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 TargetSeat = INDEX_NONE;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMahjongTile TargetTile;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FMahjongTile> ConsumedTiles;
    FString ToDebugString() const;
};

USTRUCT(BlueprintType)
struct GUIYANGMAHJONG_API FMahjongActionRequest
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EMahjongActionType Type = EMahjongActionType::Pass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RoundId = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 TurnId = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 TargetTileId = INDEX_NONE;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<int32> ConsumedTileIds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ClientSequence = 0;
};

USTRUCT(BlueprintType)
struct GUIYANGMAHJONG_API FMahjongActionResult
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bSuccess = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Message;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMahjongAction Action;
};

USTRUCT(BlueprintType)
struct GUIYANGMAHJONG_API FMahjongPlayerScoreResult
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SeatIndex = INDEX_NONE;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 BaseScoreDelta = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 JiScoreDelta = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SpecialJiScoreDelta = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 GangScoreDelta = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 TotalDelta = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 TotalScore = 0;
};

USTRUCT(BlueprintType)
struct GUIYANGMAHJONG_API FMahjongJiEvent
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EMahjongJiEventType Type = EMahjongJiEventType::ChongFeng;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMahjongTile Tile;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ActorSeat = INDEX_NONE;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 TargetSeat = INDEX_NONE;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ValueUnits = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 DiscardSequence = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct GUIYANGMAHJONG_API FMahjongSettlementResult
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bDrawGame = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bSelfDraw = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 WinnerSeat = INDEX_NONE;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<int32> WinningSeats;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 LoserSeat = INDEX_NONE;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMahjongTile WinningTile;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMahjongTile FlippedJiTile;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<int32> PlayerJiCounts;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FMahjongJiEvent> JiEvents;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FMahjongTile> JiTiles;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FMahjongPlayerScoreResult> PlayerResults;
    FString ToDebugString() const;
};

USTRUCT(BlueprintType)
struct GUIYANGMAHJONG_API FMahjongRuleConfig
{
    GENERATED_BODY()
    /** 规则族标识和版本会写入房间规则快照，开局后不得修改。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RuleId = TEXT("GuiyangMainstreamV1");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) int32 RuleVersion = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EMahjongTileSetMode TileSetMode = EMahjongTileSetMode::Suited108;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEnableChongFengJi = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEnableZeRenJi = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEnableWuGuJi = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bWuGuCanChongFeng = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bWuGuCanZeRen = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEnableQiangGangHu = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEnableYiPaoDuoXiang = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEnableQiDui = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bDrawGameDealerContinues = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEnableTimeoutAutoPlay = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 BaseScore = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 JiScore = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 BasicJiValue = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 FlippedJiValue = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 WuGuJiValue = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ChongFengJiValue = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 WuGuChongFengJiValue = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ZeRenJiValue = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 WuGuZeRenJiValue = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EMahjongJiCountingScope JiCountingScope = EMahjongJiCountingScope::HandAndMeld;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 GangScore = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ZiMoMultiplier = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 DianPaoMultiplier = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ReconnectTimeoutSeconds = 120;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 TurnTimeoutSeconds = 15;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ReactionTimeoutSeconds = 8;
};
