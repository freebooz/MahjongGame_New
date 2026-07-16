#pragma once

#include "CoreMinimal.h"
#include "Network/MahjongNetworkTypes.h"
#include "UObject/Object.h"
#include "MahjongTableEngine.generated.h"

/** 单桌 Dedicated Server 的权威牌局状态机。客户端只能提交意图，不能指定牌墙或结算结果。 */
UCLASS()
class GUIYANGMAHJONG_API UMahjongTableEngine : public UObject
{
    GENERATED_BODY()

public:
    bool StartRound(const FGuiyangRuleSnapshot& RuleSnapshot, const TArray<FMahjongSeatInfo>& Seats,
        int32 DealerSeat, int32 ShuffleSeed, FString& OutError);
    FMahjongActionResult SubmitPlayTile(int32 SeatIndex, const FMahjongActionRequest& Request);
    FMahjongActionResult SubmitTurnAction(int32 SeatIndex, const FMahjongActionRequest& Request);
    FMahjongActionResult SubmitReaction(int32 SeatIndex, const FMahjongActionRequest& Request);
    FMahjongActionResult ResolveActionTimeout(int32 ExpectedRoundId, int32 ExpectedTurnId,
        EMahjongTablePhase ExpectedPhase);
    void SetActionDeadlineForServer(double DeadlineServerTimeSeconds, int32 TimeoutSeconds);

    const FMahjongPublicTableState& GetPublicState() const { return PublicState; }
    bool GetPrivateState(int32 SeatIndex, FMahjongPrivatePlayerState& OutState) const;
    TArray<FMahjongAction> GetAvailableActions(int32 SeatIndex) const;
    const FGuiyangRuleSnapshot& GetLockedRuleSnapshot() const { return LockedRules; }
    bool GetSettlementResult(FMahjongSettlementResult& OutResult) const;
#if WITH_DEV_AUTOMATION_TESTS
    bool SetHandForServerTest(int32 SeatIndex, const FMahjongHand& Hand);
#endif

private:
    UPROPERTY(Transient) TObjectPtr<class UMahjongDeckManager> DeckManager;
    UPROPERTY() FGuiyangRuleSnapshot LockedRules;
    UPROPERTY() FMahjongPublicTableState PublicState;
    UPROPERTY() TArray<FMahjongHand> Hands;
    UPROPERTY() FMahjongSettlementResult SettlementResult;
    TMap<int32, TArray<FMahjongAction>> AvailableActionsBySeat;
    TMap<int32, FMahjongActionRequest> SubmittedReactions;
    TArray<int32> LastClientSequences;
    TArray<int32> CurrentScores;
    TArray<int32> GangDeltas;
    TArray<int32> SpecialJiDeltas;
    int32 LastDiscardSeat = INDEX_NONE;
    int32 FirstSpecialJiDiscardSequence = INDEX_NONE;
    FMahjongTile LastDrawnTile;
    bool bQiangGangWindow = false;
    int32 PendingBuGangSeat = INDEX_NONE;
    int32 PendingBuGangTileId = INDEX_NONE;
    FMahjongTile PendingBuGangTile;

    bool ValidateRequestCommon(int32 SeatIndex, const FMahjongActionRequest& Request, FString& OutError);
    void OpenReactionWindow(const FMahjongTile& Discard, int32 DiscardSeat);
    void BeginBuGang(int32 SeatIndex, const FMahjongTile& Tile);
    void CompleteBuGang();
    void ResolveQiangGangReactions(const TArray<int32>& HuSeats);
    void ResolveSubmittedReactions();
    void ResolveHuReactions(const TArray<int32>& HuSeats);
    void ApplyClaim(int32 SeatIndex, EMahjongActionType Type);
    void AdvanceTurnAndDraw();
    void RebuildTurnActions();
    void SettleWin(const TArray<int32>& WinningSeats, int32 LoserSeat, bool bSelfDraw, const FMahjongTile& WinningTile);
    void SettleDrawGame();
    void ApplyGangScore(int32 GangSeat);
    void RecordSpecialJiDiscard(int32 SeatIndex, const FMahjongDiscardRecord& Record);
    void RecordZeRenJiClaim(int32 ClaimSeat, EMahjongActionType ClaimType);
    bool IsSpecialJiTarget(const FMahjongTile& Tile, bool bForZeRen) const;
    TArray<int32> CountJiForSettlement(const FMahjongTile& FlippedJiTile, const TArray<int32>& WinningSeats,
        bool bSelfDraw, const FMahjongTile& WinningTile) const;
    void RefreshSeatCounts();
    FMahjongAction BuildReactionAction(int32 SeatIndex, EMahjongActionType Type, const FMahjongTile& Discard) const;
    int32 FindBestReactionSeat() const;
    static int32 GetReactionPriority(EMahjongActionType Type);
    static bool RemoveTilesByRuleIndex(FMahjongHand& Hand, int32 RuleIndex, int32 Count, TArray<FMahjongTile>& OutRemoved);
    FMahjongActionResult Fail(const FString& Message) const;
};
