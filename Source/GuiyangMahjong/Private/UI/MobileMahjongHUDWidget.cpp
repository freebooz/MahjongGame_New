#include "UI/MobileMahjongHUDWidget.h"
#include "UI/MobileActionButtonPanel.h"
#include "UI/MobileDiscardTileWidget.h"
#include "UI/MobileErrorToastWidget.h"
#include "UI/MobileHandTileWidget.h"
#include "UI/MobileSettlementWidget.h"
#include "Game/GuiyangMahjongGameState.h"
#include "Game/GuiyangMahjongPlayerController.h"
#include "Game/GuiyangMahjongPlayerState.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/WrapBox.h"
#include "GuiyangMahjong.h"

namespace
{
    FString MeldTypeText(const EMahjongMeldType Type)
    {
        switch (Type)
        {
        case EMahjongMeldType::Chi: return TEXT("吃");
        case EMahjongMeldType::Peng: return TEXT("碰");
        case EMahjongMeldType::MingGang: return TEXT("明杠");
        case EMahjongMeldType::AnGang: return TEXT("暗杠");
        case EMahjongMeldType::BuGang: return TEXT("补杠");
        default: return TEXT("副露");
        }
    }
}

void UMobileMahjongHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (AGuiyangMahjongPlayerController* PC = Cast<AGuiyangMahjongPlayerController>(GetOwningPlayer()))
    {
        PC->OnPrivateHandUpdated.AddUniqueDynamic(this, &ThisClass::HandlePrivateHand);
        PC->OnAvailableActionsUpdated.AddUniqueDynamic(this, &ThisClass::HandleAvailableActions);
        PC->OnSettlementShown.AddUniqueDynamic(this, &ThisClass::HandleSettlement);
        PC->OnFinalSettlementShown.AddUniqueDynamic(this, &ThisClass::HandleFinalSettlement);
        PC->OnErrorShown.AddUniqueDynamic(this, &ThisClass::HandleError);
    }
    if (AGuiyangMahjongGameState* GS = GetWorld()->GetGameState<AGuiyangMahjongGameState>())
    {
        GS->OnPublicTableStateUpdated.AddUniqueDynamic(this, &ThisClass::HandlePublicTableState);
        RefreshTableState(GS->PublicTableState);
    }
    UE_LOG(LogMahjongUI, Log, TEXT("牌局 HUD 创建并绑定私有手牌与操作事件"));
}

void UMobileMahjongHUDWidget::NativeDestruct()
{
    if (AGuiyangMahjongPlayerController* PC = Cast<AGuiyangMahjongPlayerController>(GetOwningPlayer()))
    {
        PC->OnPrivateHandUpdated.RemoveDynamic(this, &ThisClass::HandlePrivateHand);
        PC->OnAvailableActionsUpdated.RemoveDynamic(this, &ThisClass::HandleAvailableActions);
        PC->OnSettlementShown.RemoveDynamic(this, &ThisClass::HandleSettlement);
        PC->OnFinalSettlementShown.RemoveDynamic(this, &ThisClass::HandleFinalSettlement);
        PC->OnErrorShown.RemoveDynamic(this, &ThisClass::HandleError);
    }
    if (AGuiyangMahjongGameState* GS = GetWorld()->GetGameState<AGuiyangMahjongGameState>())
    {
        GS->OnPublicTableStateUpdated.RemoveDynamic(this, &ThisClass::HandlePublicTableState);
    }
    Super::NativeDestruct();
}

void UMobileMahjongHUDWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (bVisualReviewMode)
    {
        Txt_Countdown->SetText(FText::AsNumber(12));
        return;
    }
    const AGuiyangMahjongGameState* GS = GetWorld() ? GetWorld()->GetGameState<AGuiyangMahjongGameState>() : nullptr;
    if (!GS || GS->PublicTableState.ActionDeadlineServerTimeSeconds <= 0.0)
    {
        Txt_Countdown->SetText(FText::FromString(TEXT("--")));
        return;
    }
    const int32 RemainingSeconds = FMath::Max(0, FMath::CeilToInt(
        GS->PublicTableState.ActionDeadlineServerTimeSeconds - GS->GetServerWorldTimeSeconds()));
    Txt_Countdown->SetText(FText::AsNumber(RemainingSeconds));
}

void UMobileMahjongHUDWidget::RefreshTableState(const FMahjongPublicTableState& State)
{
    CachedPublicState = State;
    const int32 LocalSeat = ResolveLocalSeat();
    if (const AGuiyangMahjongGameState* GS = GetWorld() ? GetWorld()->GetGameState<AGuiyangMahjongGameState>() : nullptr)
    {
        Txt_RoomId->SetText(FText::FromString(FString::Printf(TEXT("房间：%s"), *GS->RoomState.RoomInfo.RoomId)));
    }
    Txt_RemainingTileCount->SetText(FText::FromString(FString::Printf(TEXT("剩余：%d"), State.RemainingTileCount)));
    Txt_CurrentPhase->SetText(FText::FromString(FString::Printf(TEXT("阶段：%s"), *GetPhaseDisplayText(State.Phase))));

    const FMahjongSeatInfo* TurnSeat = State.Seats.FindByPredicate([&State](const FMahjongSeatInfo& Seat)
    {
        return Seat.SeatIndex == State.CurrentTurnSeat;
    });
    Txt_CurrentTurnPlayer->SetText(FText::FromString(TurnSeat
        ? FString::Printf(TEXT("当前：%s"), *TurnSeat->PlayerName)
        : TEXT("当前：--")));

    UTextBlock* SeatWidgets[] = {Seat_Self, Seat_Right, Seat_Top, Seat_Left};
    for (UTextBlock* SeatWidget : SeatWidgets)
    {
        SeatWidget->SetText(FText::FromString(TEXT("等待玩家")));
    }
    for (const FMahjongSeatInfo& Seat : State.Seats)
    {
        const int32 RelativeSeat = GetRelativeSeatIndex(Seat.SeatIndex, LocalSeat);
        if (RelativeSeat == INDEX_NONE) continue;
        const FString OnlineText = Seat.bOnline ? TEXT("在线") : TEXT("离线");
        const FString TurnMark = Seat.SeatIndex == State.CurrentTurnSeat ? TEXT("▶ ") : TEXT("");
        SeatWidgets[RelativeSeat]->SetText(FText::FromString(FString::Printf(
            TEXT("%s%s\n手牌 %d\n%d 分 · %s"), *TurnMark, *Seat.PlayerName,
            Seat.HandTileCount, Seat.Score, *OnlineText)));
    }
    RefreshDiscards(LocalSeat);
    RefreshMelds(LocalSeat);
    RefreshJiDisplay();
    if (bHasPrivateState) RebuildPrivateHand();
    UE_LOG(LogMahjongUI, Verbose, TEXT("公共牌桌 UI 刷新：序号=%d"), State.StateSequence);
}

void UMobileMahjongHUDWidget::RefreshPrivateHand(const FMahjongPrivatePlayerState& State)
{
    const int32 PreviousLocalSeat = ResolveLocalSeat();
    CachedPrivateState = State;
    bHasPrivateState = State.SeatIndex != INDEX_NONE;
    if (ResolveLocalSeat() != PreviousLocalSeat)
    {
        RefreshTableState(CachedPublicState);
    }
    else
    {
        RebuildPrivateHand();
    }
}

void UMobileMahjongHUDWidget::ApplyVisualReviewState(const FMahjongPublicTableState& PublicState,
    const FMahjongPrivatePlayerState& PrivateState, const TArray<FMahjongAction>& Actions)
{
#if !UE_BUILD_SHIPPING
    bVisualReviewMode = true;
    RefreshPrivateHand(PrivateState);
    RefreshTableState(PublicState);
    ActionButtonPanel->ShowActions(Actions);
    Txt_Countdown->SetText(FText::AsNumber(12));
#endif
}

int32 UMobileMahjongHUDWidget::GetRelativeSeatIndex(const int32 AbsoluteSeat, const int32 LocalSeat)
{
    if (AbsoluteSeat < 0 || AbsoluteSeat >= 4 || LocalSeat < 0 || LocalSeat >= 4)
    {
        return INDEX_NONE;
    }
    return (AbsoluteSeat - LocalSeat + 4) % 4;
}

FString UMobileMahjongHUDWidget::GetPhaseDisplayText(const EMahjongTablePhase Phase)
{
    switch (Phase)
    {
    case EMahjongTablePhase::WaitingForPlayers: return TEXT("等待玩家");
    case EMahjongTablePhase::PreparingGame: return TEXT("准备开局");
    case EMahjongTablePhase::Dealing: return TEXT("发牌");
    case EMahjongTablePhase::PlayerTurn: return TEXT("玩家回合");
    case EMahjongTablePhase::WaitingForAction: return TEXT("等待碰杠胡");
    case EMahjongTablePhase::ResolvingAction: return TEXT("结算操作");
    case EMahjongTablePhase::Settlement: return TEXT("单局结算");
    case EMahjongTablePhase::GameOver: return TEXT("牌局结束");
    case EMahjongTablePhase::Restarting: return TEXT("下一局准备");
    default: return TEXT("未知阶段");
    }
}

int32 UMobileMahjongHUDWidget::ResolveLocalSeat() const
{
    if (bHasPrivateState && CachedPrivateState.SeatIndex >= 0 && CachedPrivateState.SeatIndex < 4)
    {
        return CachedPrivateState.SeatIndex;
    }
    if (const AGuiyangMahjongPlayerState* PlayerState = GetOwningPlayer()
        ? GetOwningPlayer()->GetPlayerState<AGuiyangMahjongPlayerState>() : nullptr)
    {
        if (PlayerState->SeatIndex >= 0 && PlayerState->SeatIndex < 4)
        {
            return PlayerState->SeatIndex;
        }
    }
    return 0;
}

void UMobileMahjongHUDWidget::RebuildPrivateHand()
{
    Panel_SelfHandTiles->ClearChildren();
    SelectedHandTile = nullptr;
    if (!bHasPrivateState) return;
    UClass* TileWidgetClass = LoadClass<UMobileHandTileWidget>(nullptr, TEXT("/Game/UI/Components/WBP_HandTile.WBP_HandTile_C"));
    if (!TileWidgetClass)
    {
        UE_LOG(LogMahjongUI, Warning, TEXT("尚未找到 WBP_HandTile，私有手牌暂不生成可视组件"));
        return;
    }
    const bool bCanPlay = CachedPublicState.Phase == EMahjongTablePhase::PlayerTurn
        && CachedPublicState.CurrentTurnSeat == CachedPrivateState.SeatIndex;
    for (const FMahjongTile& Tile : CachedPrivateState.Hand.Tiles)
    {
        if (UMobileHandTileWidget* TileWidget = CreateWidget<UMobileHandTileWidget>(GetOwningPlayer(), TileWidgetClass))
        {
            TileWidget->SetTile(Tile, bCanPlay);
            TileWidget->OnTileSelected.AddUniqueDynamic(this, &ThisClass::HandleTileSelected);
            Panel_SelfHandTiles->AddChildToHorizontalBox(TileWidget);
        }
    }
    UE_LOG(LogMahjongUI, Log, TEXT("私有手牌 UI 刷新：%d 张，可出牌=%s"),
        CachedPrivateState.Hand.Tiles.Num(), bCanPlay ? TEXT("是") : TEXT("否"));
}

void UMobileMahjongHUDWidget::RefreshDiscards(const int32 LocalSeat)
{
    UWrapBox* DiscardPanels[] = {Panel_SelfDiscards, Panel_RightDiscards, Panel_TopDiscards, Panel_LeftDiscards};
    for (UWrapBox* Panel : DiscardPanels) Panel->ClearChildren();

    UClass* DiscardClass = LoadClass<UMobileDiscardTileWidget>(nullptr,
        TEXT("/Game/UI/Components/WBP_DiscardTile.WBP_DiscardTile_C"));
    if (!DiscardClass) return;

    int32 LatestSequence = INDEX_NONE;
    for (const FMahjongDiscardRecord& Record : CachedPublicState.Discards)
    {
        if (!Record.bClaimed) LatestSequence = FMath::Max(LatestSequence, Record.Sequence);
    }
    for (const FMahjongDiscardRecord& Record : CachedPublicState.Discards)
    {
        if (Record.bClaimed) continue;
        const int32 RelativeSeat = GetRelativeSeatIndex(Record.SeatIndex, LocalSeat);
        if (RelativeSeat == INDEX_NONE) continue;
        if (UMobileDiscardTileWidget* TileWidget = CreateWidget<UMobileDiscardTileWidget>(GetOwningPlayer(), DiscardClass))
        {
            TileWidget->SetDiscard(Record.Tile, Record.Sequence == LatestSequence);
            DiscardPanels[RelativeSeat]->AddChildToWrapBox(TileWidget);
        }
    }
}

void UMobileMahjongHUDWidget::RefreshMelds(const int32 LocalSeat)
{
    UVerticalBox* MeldPanels[] = {Panel_SelfMelds, Panel_RightMelds, Panel_TopMelds, Panel_LeftMelds};
    for (UVerticalBox* Panel : MeldPanels) Panel->ClearChildren();

    UClass* TileClass = LoadClass<UMobileDiscardTileWidget>(nullptr,
        TEXT("/Game/UI/Components/WBP_DiscardTile.WBP_DiscardTile_C"));
    if (!TileClass) return;

    for (const FMahjongMeld& Meld : CachedPublicState.PublicMelds)
    {
        const int32 RelativeSeat = GetRelativeSeatIndex(Meld.OwnerSeat, LocalSeat);
        if (RelativeSeat == INDEX_NONE) continue;
        UHorizontalBox* Row = NewObject<UHorizontalBox>(this);
        UTextBlock* TypeLabel = NewObject<UTextBlock>(this);
        TypeLabel->SetText(FText::FromString(MeldTypeText(Meld.Type)));
        Row->AddChildToHorizontalBox(TypeLabel);
        for (const FMahjongTile& Tile : Meld.Tiles)
        {
            if (UMobileDiscardTileWidget* TileWidget = CreateWidget<UMobileDiscardTileWidget>(GetOwningPlayer(), TileClass))
            {
                TileWidget->SetDiscard(Tile, false);
                Row->AddChildToHorizontalBox(TileWidget);
            }
        }
        MeldPanels[RelativeSeat]->AddChildToVerticalBox(Row);
    }
}

void UMobileMahjongHUDWidget::RefreshJiDisplay()
{
    Txt_FlippedJiTile->SetText(FText::FromString(CachedPublicState.FlippedJiTile.IsValid()
        ? FString::Printf(TEXT("翻鸡：%s"), *CachedPublicState.FlippedJiTile.ToDebugString())
        : TEXT("翻鸡：尚未翻牌")));
    if (CachedPublicState.JiEvents.IsEmpty())
    {
        Txt_JiEvents->SetText(FText::FromString(TEXT("特殊鸡事件：无")));
        return;
    }
    TArray<FString> Lines;
    for (const FMahjongJiEvent& Event : CachedPublicState.JiEvents)
    {
        Lines.Add(Event.Type == EMahjongJiEventType::ChongFeng
            ? FString::Printf(TEXT("冲锋鸡：座位%d · %s · %d单位"),
                Event.ActorSeat, *Event.Tile.ToDebugString(), Event.ValueUnits)
            : FString::Printf(TEXT("责任鸡：座位%d → 座位%d · %d单位"),
                Event.ActorSeat, Event.TargetSeat, Event.ValueUnits));
    }
    Txt_JiEvents->SetText(FText::FromString(FString::Join(Lines, TEXT("\n"))));
}

void UMobileMahjongHUDWidget::HandleTileSelected(UMobileHandTileWidget* TileWidget)
{
    SelectedHandTile = TileWidget;
    for (int32 ChildIndex = 0; ChildIndex < Panel_SelfHandTiles->GetChildrenCount(); ++ChildIndex)
    {
        if (UMobileHandTileWidget* Child = Cast<UMobileHandTileWidget>(Panel_SelfHandTiles->GetChildAt(ChildIndex)))
        {
            Child->SetSelected(Child == TileWidget);
        }
    }
}

void UMobileMahjongHUDWidget::HandlePublicTableState(const FMahjongPublicTableState& State){ RefreshTableState(State); }
void UMobileMahjongHUDWidget::HandlePrivateHand(const FMahjongPrivatePlayerState& State){ RefreshPrivateHand(State); }
void UMobileMahjongHUDWidget::HandleAvailableActions(const TArray<FMahjongAction>& Actions){ ActionButtonPanel->ShowActions(Actions); }

void UMobileMahjongHUDWidget::HandleSettlement(const FMahjongSettlementResult& Result)
{
    if (!SettlementInstance)
    {
        UClass* SettlementClass = LoadClass<UMobileSettlementWidget>(nullptr, TEXT("/Game/UI/Dialogs/WBP_Settlement.WBP_Settlement_C"));
        if (SettlementClass)
        {
            SettlementInstance = CreateWidget<UMobileSettlementWidget>(GetOwningPlayer(), SettlementClass);
            PopupLayer->AddChildToOverlay(SettlementInstance);
        }
    }
    if (SettlementInstance)
    {
        SettlementInstance->SetVisibility(ESlateVisibility::Visible);
        SettlementInstance->SetSettlementResult(Result);
    }
}

void UMobileMahjongHUDWidget::HandleFinalSettlement(const FMahjongFinalSettlementResult& Result)
{
    if (!SettlementInstance)
    {
        UClass* SettlementClass = LoadClass<UMobileSettlementWidget>(nullptr,
            TEXT("/Game/UI/Dialogs/WBP_Settlement.WBP_Settlement_C"));
        if (SettlementClass)
        {
            SettlementInstance = CreateWidget<UMobileSettlementWidget>(GetOwningPlayer(), SettlementClass);
            PopupLayer->AddChildToOverlay(SettlementInstance);
        }
    }
    if (SettlementInstance)
    {
        SettlementInstance->SetVisibility(ESlateVisibility::Visible);
        SettlementInstance->SetFinalSettlementResult(Result);
    }
}

void UMobileMahjongHUDWidget::HandleError(const FString& Message)
{
    if (!ErrorToastInstance)
    {
        UClass* ErrorClass = LoadClass<UMobileErrorToastWidget>(nullptr, TEXT("/Game/UI/Components/WBP_ErrorToast.WBP_ErrorToast_C"));
        if (ErrorClass)
        {
            ErrorToastInstance = CreateWidget<UMobileErrorToastWidget>(GetOwningPlayer(), ErrorClass);
            PopupLayer->AddChildToOverlay(ErrorToastInstance);
        }
    }
    if (ErrorToastInstance) ErrorToastInstance->ShowToast(Message);
}
