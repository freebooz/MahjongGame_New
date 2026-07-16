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
    FMahjongActionResult SubmitReaction(int32 SeatIndex, const FMahjongActionRequest& Request);

    const FMahjongPublicTableState& GetPublicState() const { return PublicState; }
    bool GetPrivateState(int32 SeatIndex, FMahjongPrivatePlayerState& OutState) const;
    TArray<FMahjongAction> GetAvailableActions(int32 SeatIndex) const;
    const FGuiyangRuleSnapshot& GetLockedRuleSnapshot() const { return LockedRules; }
#if WITH_DEV_AUTOMATION_TESTS
    bool SetHandForServerTest(int32 SeatIndex, const FMahjongHand& Hand);
#endif

private:
    UPROPERTY(Transient) TObjectPtr<class UMahjongDeckManager> DeckManager;
    UPROPERTY() FGuiyangRuleSnapshot LockedRules;
    UPROPERTY() FMahjongPublicTableState PublicState;
    UPROPERTY() TArray<FMahjongHand> Hands;
    TMap<int32, TArray<FMahjongAction>> AvailableActionsBySeat;
    TMap<int32, FMahjongActionRequest> SubmittedReactions;
    TArray<int32> LastClientSequences;
    int32 LastDiscardSeat = INDEX_NONE;

    bool ValidateRequestCommon(int32 SeatIndex, const FMahjongActionRequest& Request, FString& OutError);
    void OpenReactionWindow(const FMahjongTile& Discard, int32 DiscardSeat);
    void ResolveSubmittedReactions();
    void ResolveHuReactions(const TArray<int32>& HuSeats);
    void ApplyClaim(int32 SeatIndex, EMahjongActionType Type);
    void AdvanceTurnAndDraw();
    void RefreshSeatCounts();
    FMahjongAction BuildReactionAction(int32 SeatIndex, EMahjongActionType Type, const FMahjongTile& Discard) const;
    int32 FindBestReactionSeat() const;
    static int32 GetReactionPriority(EMahjongActionType Type);
    static bool RemoveTilesByRuleIndex(FMahjongHand& Hand, int32 RuleIndex, int32 Count, TArray<FMahjongTile>& OutRemoved);
    FMahjongActionResult Fail(const FString& Message) const;
};
