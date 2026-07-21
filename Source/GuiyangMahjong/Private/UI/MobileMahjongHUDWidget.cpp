#include "UI/MobileMahjongHUDWidget.h"
#include "UI/MobileActionButtonPanel.h"
#include "UI/MobileDiscardTileWidget.h"
#include "UI/MobileErrorToastWidget.h"
#include "UI/MobileHandTileWidget.h"
#include "UI/MobileSettlementWidget.h"
#include "Game/Mahjong3DTableActor.h"
#include "Game/GuiyangMahjongGameState.h"
#include "Game/GuiyangMahjongPlayerController.h"
#include "Game/GuiyangMahjongPlayerState.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Viewport.h"
#include "Components/WrapBox.h"
#include "Engine/Texture2D.h"
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
    if (Table3DViewport)
    {
        Table3DViewport->SetVisibility(ESlateVisibility::HitTestInvisible);
        Table3DViewport->SetEnableAdvancedFeatures(true);
        Table3DViewport->SetBackgroundColor(FLinearColor(0.01f, 0.055f, 0.045f, 1.0f));
        // PBR 麻将材质不需要高强度预览灯；较低的主光和天空光可避免白色树脂过曝。
        Table3DViewport->SetLightIntensity(1.7f);
        Table3DViewport->SetSkyIntensity(0.55f);
        // 本家固定在南侧（屏幕底部），相机从南向北俯视；缩短距离让牌桌填满主要视口。
        // Frame the 1060 x 770 cm tabletop as the room background, with only a narrow margin
        // around its outer rails instead of viewing it as a small object in the distance.
        const FVector CameraLocation(0.0f, -530.0f, 430.0f);
        Table3DViewport->SetViewLocation(CameraLocation);
        Table3DViewport->SetViewRotation((FVector(0.0f, -45.0f, 5.0f) - CameraLocation).Rotation());
        Table3DActor = Cast<AMahjong3DTableActor>(Table3DViewport->Spawn(AMahjong3DTableActor::StaticClass()));

        // 旧二维牌面仅保留本家透明点击层，其余牌区全部由三维模型表现。
        Panel_SelfHandTiles->SetRenderOpacity(0.0f);
        Panel_TopHandTiles->SetVisibility(ESlateVisibility::Collapsed);
        Panel_LeftHandTiles->SetVisibility(ESlateVisibility::Collapsed);
        Panel_RightHandTiles->SetVisibility(ESlateVisibility::Collapsed);
        Panel_SelfDiscards->SetVisibility(ESlateVisibility::Collapsed);
        Panel_TopDiscards->SetVisibility(ESlateVisibility::Collapsed);
        Panel_LeftDiscards->SetVisibility(ESlateVisibility::Collapsed);
        Panel_RightDiscards->SetVisibility(ESlateVisibility::Collapsed);
        Panel_SelfMelds->SetVisibility(ESlateVisibility::Collapsed);
        Panel_TopMelds->SetVisibility(ESlateVisibility::Collapsed);
        Panel_LeftMelds->SetVisibility(ESlateVisibility::Collapsed);
        Panel_RightMelds->SetVisibility(ESlateVisibility::Collapsed);
    }
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
    if (Table3DActor)
    {
        Table3DActor->Destroy();
        Table3DActor = nullptr;
    }
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
    const TCHAR* SeatDirections[] = {TEXT("南"), TEXT("东"), TEXT("北"), TEXT("西")};
    for (int32 RelativeSeat = 0; RelativeSeat < 4; ++RelativeSeat)
    {
        SeatWidgets[RelativeSeat]->SetText(FText::FromString(
            FString::Printf(TEXT("%s · 等待玩家"), SeatDirections[RelativeSeat])));
    }
    for (const FMahjongSeatInfo& Seat : State.Seats)
    {
        const int32 RelativeSeat = GetRelativeSeatIndex(Seat.SeatIndex, LocalSeat);
        if (RelativeSeat == INDEX_NONE) continue;
        const FString OnlineText = Seat.bOnline ? TEXT("在线") : TEXT("离线");
        const FString TurnMark = Seat.SeatIndex == State.CurrentTurnSeat ? TEXT("▶ ") : TEXT("");
        SeatWidgets[RelativeSeat]->SetText(FText::FromString(FString::Printf(
            TEXT("%s%s · %s%s\n手牌 %d\n%d 分 · %s"), SeatDirections[RelativeSeat],
            RelativeSeat == 0 ? TEXT("【我】") : TEXT(""), *TurnMark, *Seat.PlayerName,
            Seat.HandTileCount, Seat.Score, *OnlineText)));
    }
    RefreshOpponentHands(LocalSeat);
    RefreshDiscards(LocalSeat);
    RefreshMelds(LocalSeat);
    RefreshJiDisplay();
    if (bHasPrivateState) RebuildPrivateHand();
    Refresh3DTable();
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
    Refresh3DTable();
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
    for (int32 TileIndex = 0; TileIndex < CachedPrivateState.Hand.Tiles.Num(); ++TileIndex)
    {
        const FMahjongTile& Tile = CachedPrivateState.Hand.Tiles[TileIndex];
        if (UMobileHandTileWidget* TileWidget = CreateWidget<UMobileHandTileWidget>(GetOwningPlayer(), TileWidgetClass))
        {
            TileWidget->SetTile(Tile, bCanPlay);
            TileWidget->OnTileSelected.AddUniqueDynamic(this, &ThisClass::HandleTileSelected);
            if (UHorizontalBoxSlot* HandSlot = Panel_SelfHandTiles->AddChildToHorizontalBox(TileWidget))
            {
                // 十四张时将最后一张视作摸牌区，参照桌面麻将常见布局留出可辨识间隔。
                if (CachedPrivateState.Hand.Tiles.Num() == 14 && TileIndex == 13)
                {
                    HandSlot->SetPadding(FMargin(20.0f, 0.0f, 0.0f, 0.0f));
                }
            }
        }
    }
    UE_LOG(LogMahjongUI, Log, TEXT("私有手牌 UI 刷新：%d 张，可出牌=%s"),
        CachedPrivateState.Hand.Tiles.Num(), bCanPlay ? TEXT("是") : TEXT("否"));
}

void UMobileMahjongHUDWidget::RefreshOpponentHands(const int32 LocalSeat)
{
    Panel_TopHandTiles->ClearChildren();
    Panel_LeftHandTiles->ClearChildren();
    Panel_RightHandTiles->ClearChildren();

    UTexture2D* BackTexture = LoadObject<UTexture2D>(nullptr,
        TEXT("/Game/UI/Textures/Tiles/T_Tile_Back.T_Tile_Back"));
    if (!BackTexture)
    {
        UE_LOG(LogMahjongUI, Warning, TEXT("未找到对手牌背纹理，跳过暗手展示"));
        return;
    }

    int32 HandCounts[4] = {};
    for (const FMahjongSeatInfo& Seat : CachedPublicState.Seats)
    {
        const int32 RelativeSeat = GetRelativeSeatIndex(Seat.SeatIndex, LocalSeat);
        if (RelativeSeat != INDEX_NONE)
        {
            HandCounts[RelativeSeat] = FMath::Clamp(Seat.HandTileCount, 0, 14);
        }
    }

    FSlateBrush BackBrush;
    BackBrush.SetResourceObject(BackTexture);
    BackBrush.ImageSize = FVector2D(BackTexture->GetSizeX(), BackTexture->GetSizeY());
    BackBrush.DrawAs = ESlateBrushDrawType::Image;

    for (int32 Index = 0; Index < HandCounts[2]; ++Index)
    {
        UImage* TileBack = NewObject<UImage>(this);
        TileBack->SetBrush(BackBrush);
        TileBack->SetDesiredSizeOverride(FVector2D(44.0f, 60.0f));
        if (UHorizontalBoxSlot* HandSlot = Panel_TopHandTiles->AddChildToHorizontalBox(TileBack))
        {
            HandSlot->SetPadding(FMargin(0.0f, 0.0f, -14.0f, 0.0f));
        }
    }

    auto FillVerticalHand = [this, &BackBrush](UVerticalBox* Panel, const int32 Count)
    {
        for (int32 Index = 0; Index < Count; ++Index)
        {
            UImage* TileBack = NewObject<UImage>(this);
            TileBack->SetBrush(BackBrush);
            TileBack->SetDesiredSizeOverride(FVector2D(44.0f, 60.0f));
            if (UVerticalBoxSlot* HandSlot = Panel->AddChildToVerticalBox(TileBack))
            {
                HandSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, -34.0f));
            }
        }
    };
    FillVerticalHand(Panel_RightHandTiles, HandCounts[1]);
    FillVerticalHand(Panel_LeftHandTiles, HandCounts[3]);
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
    if (Table3DActor && TileWidget)
    {
        Table3DActor->SetSelectedTile(TileWidget->GetTileData().UniqueId);
    }
}

void UMobileMahjongHUDWidget::Refresh3DTable()
{
    if (Table3DActor)
    {
        Table3DActor->UpdateLayout(CachedPublicState, CachedPrivateState,
            bHasPrivateState, ResolveLocalSeat());
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
