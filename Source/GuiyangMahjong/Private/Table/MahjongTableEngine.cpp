#include "Table/MahjongTableEngine.h"

#include "Rules/MahjongChiPengChecker.h"
#include "Rules/MahjongGangChecker.h"
#include "Rules/MahjongHuChecker.h"
#include "Table/MahjongDeckManager.h"

bool UMahjongTableEngine::StartRound(const FGuiyangRuleSnapshot& RuleSnapshot, const TArray<FMahjongSeatInfo>& Seats,
    const int32 DealerSeat, const int32 ShuffleSeed, FString& OutError)
{
    if (!UGuiyangRuleSnapshotLibrary::VerifySnapshot(RuleSnapshot) || Seats.Num() != 4 || DealerSeat < 0 || DealerSeat >= 4)
    {
        OutError = TEXT("规则快照、座位或庄家无效");
        return false;
    }
    for (const FMahjongSeatInfo& Seat : Seats)
    {
        if (!Seat.bOccupied || Seat.SeatIndex < 0 || Seat.SeatIndex >= 4)
        {
            OutError = TEXT("开局必须有四个有效座位");
            return false;
        }
    }

    const int32 NextRoundId = FMath::Max(1, PublicState.RoundId + 1);
    LockedRules = RuleSnapshot;
    DeckManager = NewObject<UMahjongDeckManager>(this);
    DeckManager->InitializeDeck(LockedRules.Config);
    DeckManager->ShuffleDeck(ShuffleSeed);
    if (!DeckManager->DealInitialHands(Hands, DealerSeat))
    {
        OutError = TEXT("服务端发牌失败");
        return false;
    }

    PublicState = FMahjongPublicTableState();
    PublicState.RoundId = NextRoundId;
    PublicState.TurnId = 1;
    PublicState.ServerActionSequence = 1;
    PublicState.Phase = EMahjongTablePhase::PlayerTurn;
    PublicState.CurrentTurnSeat = DealerSeat;
    PublicState.RemainingTileCount = DeckManager->GetRemainingCount();
    PublicState.Seats = Seats;
    PublicState.StateSequence = 1;
    LastClientSequences.Init(-1, 4);
    LastDiscardSeat = INDEX_NONE;
    AvailableActionsBySeat.Reset();
    SubmittedReactions.Reset();
    RefreshSeatCounts();
    OutError.Reset();
    return true;
}

FMahjongActionResult UMahjongTableEngine::SubmitPlayTile(const int32 SeatIndex, const FMahjongActionRequest& Request)
{
    FString Error;
    if (!ValidateRequestCommon(SeatIndex, Request, Error)) return Fail(Error);
    if (Request.Type != EMahjongActionType::Play || PublicState.Phase != EMahjongTablePhase::PlayerTurn
        || PublicState.CurrentTurnSeat != SeatIndex)
    {
        return Fail(TEXT("当前阶段或座位不允许出牌"));
    }
    const int32 TileIndex = Hands[SeatIndex].Tiles.IndexOfByPredicate(
        [&Request](const FMahjongTile& Tile) { return Tile.UniqueId == Request.TargetTileId; });
    if (!Hands[SeatIndex].Tiles.IsValidIndex(TileIndex)) return Fail(TEXT("目标牌不在该玩家手牌中"));

    LastClientSequences[SeatIndex] = Request.ClientSequence;
    const FMahjongTile PlayedTile = Hands[SeatIndex].Tiles[TileIndex];
    Hands[SeatIndex].Tiles.RemoveAt(TileIndex);
    FMahjongDiscardRecord Record;
    Record.SeatIndex = SeatIndex;
    Record.Tile = PlayedTile;
    Record.Sequence = ++PublicState.ServerActionSequence;
    PublicState.Discards.Add(Record);
    PublicState.LastDiscard = PlayedTile;
    LastDiscardSeat = SeatIndex;
    ++PublicState.StateSequence;
    RefreshSeatCounts();
    OpenReactionWindow(PlayedTile, SeatIndex);

    FMahjongActionResult Result;
    Result.bSuccess = true;
    Result.Message = TEXT("服务端已接受出牌");
    Result.Action.Type = EMahjongActionType::Play;
    Result.Action.SourceSeat = SeatIndex;
    Result.Action.TargetTile = PlayedTile;
    return Result;
}

FMahjongActionResult UMahjongTableEngine::SubmitReaction(const int32 SeatIndex, const FMahjongActionRequest& Request)
{
    FString Error;
    if (!ValidateRequestCommon(SeatIndex, Request, Error)) return Fail(Error);
    if (PublicState.Phase != EMahjongTablePhase::WaitingForAction || !AvailableActionsBySeat.Contains(SeatIndex))
        return Fail(TEXT("当前没有该玩家的响应窗口"));
    if (SubmittedReactions.Contains(SeatIndex)) return Fail(TEXT("该响应已经提交"));

    const TArray<FMahjongAction>& Available = AvailableActionsBySeat[SeatIndex];
    const bool bAllowed = Request.Type == EMahjongActionType::Pass
        || Available.ContainsByPredicate([&Request](const FMahjongAction& Action) { return Action.Type == Request.Type; });
    if (!bAllowed) return Fail(TEXT("请求动作不在服务端候选列表中"));

    LastClientSequences[SeatIndex] = Request.ClientSequence;
    SubmittedReactions.Add(SeatIndex, Request);
    ++PublicState.ServerActionSequence;
    if (SubmittedReactions.Num() == AvailableActionsBySeat.Num()) ResolveSubmittedReactions();

    FMahjongActionResult Result;
    Result.bSuccess = true;
    Result.Message = TEXT("响应已记录");
    Result.Action.Type = Request.Type;
    Result.Action.SourceSeat = SeatIndex;
    Result.Action.TargetTile = PublicState.LastDiscard;
    return Result;
}

bool UMahjongTableEngine::GetPrivateState(const int32 SeatIndex, FMahjongPrivatePlayerState& OutState) const
{
    if (!Hands.IsValidIndex(SeatIndex)) return false;
    OutState.RoundId = PublicState.RoundId;
    OutState.TurnId = PublicState.TurnId;
    OutState.SeatIndex = SeatIndex;
    OutState.Hand = Hands[SeatIndex];
    OutState.StateSequence = PublicState.StateSequence;
    return true;
}

TArray<FMahjongAction> UMahjongTableEngine::GetAvailableActions(const int32 SeatIndex) const
{
    const TArray<FMahjongAction>* Actions = AvailableActionsBySeat.Find(SeatIndex);
    return Actions ? *Actions : TArray<FMahjongAction>();
}

#if WITH_DEV_AUTOMATION_TESTS
bool UMahjongTableEngine::SetHandForServerTest(const int32 SeatIndex, const FMahjongHand& Hand)
{
    if (!Hands.IsValidIndex(SeatIndex)) return false;
    Hands[SeatIndex] = Hand;
    Hands[SeatIndex].Sort();
    RefreshSeatCounts();
    return true;
}
#endif

bool UMahjongTableEngine::ValidateRequestCommon(const int32 SeatIndex, const FMahjongActionRequest& Request, FString& OutError)
{
    if (!Hands.IsValidIndex(SeatIndex)) { OutError = TEXT("座位无效"); return false; }
    if (Request.RoundId != PublicState.RoundId || Request.TurnId != PublicState.TurnId)
    { OutError = TEXT("请求所属牌局或回合已过期"); return false; }
    if (Request.ClientSequence <= LastClientSequences[SeatIndex])
    { OutError = TEXT("客户端序号重复或倒退"); return false; }
    return true;
}

void UMahjongTableEngine::OpenReactionWindow(const FMahjongTile& Discard, const int32 DiscardSeat)
{
    AvailableActionsBySeat.Reset();
    SubmittedReactions.Reset();
    for (int32 Seat = 0; Seat < Hands.Num(); ++Seat)
    {
        if (Seat == DiscardSeat) continue;
        TArray<FMahjongAction> Actions;
        FMahjongHand HuHand = Hands[Seat];
        HuHand.Tiles.Add(Discard);
        if (UMahjongHuChecker::CanHu(HuHand, LockedRules.Config.bEnableQiDui))
            Actions.Add(BuildReactionAction(Seat, EMahjongActionType::Hu, Discard));
        if (UMahjongGangChecker::CanMingGang(Hands[Seat], Discard))
            Actions.Add(BuildReactionAction(Seat, EMahjongActionType::MingGang, Discard));
        if (UMahjongChiPengChecker::CanPeng(Hands[Seat], Discard))
            Actions.Add(BuildReactionAction(Seat, EMahjongActionType::Peng, Discard));
        if (!Actions.IsEmpty()) AvailableActionsBySeat.Add(Seat, MoveTemp(Actions));
    }
    if (AvailableActionsBySeat.IsEmpty()) AdvanceTurnAndDraw();
    else PublicState.Phase = EMahjongTablePhase::WaitingForAction;
}

void UMahjongTableEngine::ResolveSubmittedReactions()
{
    TArray<int32> HuSeats;
    for (const TPair<int32, FMahjongActionRequest>& Pair : SubmittedReactions)
    {
        if (Pair.Value.Type == EMahjongActionType::Hu) HuSeats.Add(Pair.Key);
    }
    if (!HuSeats.IsEmpty())
    {
        ResolveHuReactions(HuSeats);
        return;
    }
    const int32 WinnerSeat = FindBestReactionSeat();
    if (WinnerSeat == INDEX_NONE)
    {
        AdvanceTurnAndDraw();
        return;
    }
    ApplyClaim(WinnerSeat, SubmittedReactions[WinnerSeat].Type);
}

void UMahjongTableEngine::ResolveHuReactions(const TArray<int32>& HuSeats)
{
    PublicState.WinningSeats.Reset();
    if (LockedRules.Config.bEnableYiPaoDuoXiang)
    {
        PublicState.WinningSeats = HuSeats;
        PublicState.WinningSeats.Sort([this](const int32 Left, const int32 Right)
        {
            return (Left - LastDiscardSeat + 4) % 4 < (Right - LastDiscardSeat + 4) % 4;
        });
    }
    else
    {
        int32 ClosestSeat = HuSeats[0];
        for (const int32 Seat : HuSeats)
        {
            if ((Seat - LastDiscardSeat + 4) % 4 < (ClosestSeat - LastDiscardSeat + 4) % 4) ClosestSeat = Seat;
        }
        PublicState.WinningSeats.Add(ClosestSeat);
    }
    PublicState.CurrentTurnSeat = PublicState.WinningSeats[0];
    PublicState.Phase = EMahjongTablePhase::Settlement;
    AvailableActionsBySeat.Reset();
    SubmittedReactions.Reset();
    ++PublicState.ServerActionSequence;
    ++PublicState.StateSequence;
}

void UMahjongTableEngine::ApplyClaim(const int32 SeatIndex, const EMahjongActionType Type)
{
    if (Type == EMahjongActionType::Hu)
    {
        PublicState.WinningSeats = { SeatIndex };
        PublicState.Phase = EMahjongTablePhase::Settlement;
        PublicState.CurrentTurnSeat = SeatIndex;
        AvailableActionsBySeat.Reset();
        SubmittedReactions.Reset();
        ++PublicState.StateSequence;
        return;
    }

    const int32 RemoveCount = Type == EMahjongActionType::MingGang ? 3 : 2;
    TArray<FMahjongTile> Removed;
    if (!RemoveTilesByRuleIndex(Hands[SeatIndex], PublicState.LastDiscard.GetRuleIndex(), RemoveCount, Removed))
    {
        AdvanceTurnAndDraw();
        return;
    }
    FMahjongMeld Meld;
    Meld.Type = Type == EMahjongActionType::MingGang ? EMahjongMeldType::MingGang : EMahjongMeldType::Peng;
    Meld.FromSeat = LastDiscardSeat;
    Meld.Tiles = MoveTemp(Removed);
    Meld.Tiles.Add(PublicState.LastDiscard);
    Hands[SeatIndex].Melds.Add(Meld);
    PublicState.PublicMelds.Add(Meld);
    if (!PublicState.Discards.IsEmpty()) PublicState.Discards.Last().bClaimed = true;
    PublicState.CurrentTurnSeat = SeatIndex;
    ++PublicState.TurnId;
    PublicState.Phase = EMahjongTablePhase::PlayerTurn;
    if (Type == EMahjongActionType::MingGang)
    {
        FMahjongTile Replacement;
        if (DeckManager->DrawTile(Replacement)) Hands[SeatIndex].Tiles.Add(Replacement);
        else PublicState.Phase = EMahjongTablePhase::Settlement;
    }
    AvailableActionsBySeat.Reset();
    SubmittedReactions.Reset();
    PublicState.RemainingTileCount = DeckManager->GetRemainingCount();
    ++PublicState.StateSequence;
    RefreshSeatCounts();
}

void UMahjongTableEngine::AdvanceTurnAndDraw()
{
    AvailableActionsBySeat.Reset();
    SubmittedReactions.Reset();
    PublicState.CurrentTurnSeat = (LastDiscardSeat + 1) % 4;
    ++PublicState.TurnId;
    FMahjongTile Drawn;
    if (!DeckManager->DrawTile(Drawn))
    {
        PublicState.Phase = EMahjongTablePhase::Settlement;
    }
    else
    {
        Hands[PublicState.CurrentTurnSeat].Tiles.Add(Drawn);
        Hands[PublicState.CurrentTurnSeat].Sort();
        PublicState.Phase = EMahjongTablePhase::PlayerTurn;
    }
    PublicState.RemainingTileCount = DeckManager->GetRemainingCount();
    ++PublicState.StateSequence;
    RefreshSeatCounts();
}

void UMahjongTableEngine::RefreshSeatCounts()
{
    for (FMahjongSeatInfo& Seat : PublicState.Seats)
    {
        if (Hands.IsValidIndex(Seat.SeatIndex)) Seat.HandTileCount = Hands[Seat.SeatIndex].Tiles.Num();
    }
}

FMahjongAction UMahjongTableEngine::BuildReactionAction(const int32 SeatIndex, const EMahjongActionType Type, const FMahjongTile& Discard) const
{
    FMahjongAction Action;
    Action.Type = Type;
    Action.SourceSeat = SeatIndex;
    Action.TargetSeat = LastDiscardSeat;
    Action.TargetTile = Discard;
    return Action;
}

int32 UMahjongTableEngine::FindBestReactionSeat() const
{
    int32 BestSeat = INDEX_NONE;
    int32 BestPriority = 0;
    int32 BestDistance = 5;
    for (const TPair<int32, FMahjongActionRequest>& Pair : SubmittedReactions)
    {
        const int32 Priority = GetReactionPriority(Pair.Value.Type);
        const int32 Distance = (Pair.Key - LastDiscardSeat + 4) % 4;
        if (Priority > BestPriority || (Priority == BestPriority && Priority > 0 && Distance < BestDistance))
        {
            BestSeat = Pair.Key;
            BestPriority = Priority;
            BestDistance = Distance;
        }
    }
    return BestPriority > 0 ? BestSeat : INDEX_NONE;
}

int32 UMahjongTableEngine::GetReactionPriority(const EMahjongActionType Type)
{
    if (Type == EMahjongActionType::Hu) return 3;
    if (Type == EMahjongActionType::MingGang) return 2;
    if (Type == EMahjongActionType::Peng) return 1;
    return 0;
}

bool UMahjongTableEngine::RemoveTilesByRuleIndex(FMahjongHand& Hand, const int32 RuleIndex, const int32 Count, TArray<FMahjongTile>& OutRemoved)
{
    OutRemoved.Reset();
    for (int32 Index = Hand.Tiles.Num() - 1; Index >= 0 && OutRemoved.Num() < Count; --Index)
    {
        if (Hand.Tiles[Index].GetRuleIndex() == RuleIndex)
        {
            OutRemoved.Add(Hand.Tiles[Index]);
            Hand.Tiles.RemoveAt(Index);
        }
    }
    return OutRemoved.Num() == Count;
}

FMahjongActionResult UMahjongTableEngine::Fail(const FString& Message) const
{
    FMahjongActionResult Result;
    Result.Message = Message;
    return Result;
}
