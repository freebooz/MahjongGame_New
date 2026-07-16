#include "UI/MobileMahjongHUDWidget.h"
#include "UI/MobileActionButtonPanel.h"
#include "UI/MobileErrorToastWidget.h"
#include "UI/MobileHandTileWidget.h"
#include "UI/MobileSettlementWidget.h"
#include "Game/GuiyangMahjongGameState.h"
#include "Game/GuiyangMahjongPlayerController.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Components/WrapBox.h"
#include "GuiyangMahjong.h"

void UMobileMahjongHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (AGuiyangMahjongPlayerController* PC = Cast<AGuiyangMahjongPlayerController>(GetOwningPlayer()))
    {
        PC->OnPrivateHandUpdated.AddUniqueDynamic(this, &ThisClass::HandlePrivateHand);
        PC->OnAvailableActionsUpdated.AddUniqueDynamic(this, &ThisClass::HandleAvailableActions);
        PC->OnSettlementShown.AddUniqueDynamic(this, &ThisClass::HandleSettlement);
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
    Txt_RemainingTileCount->SetText(FText::FromString(FString::Printf(TEXT("剩余：%d"), State.RemainingTileCount)));
    Txt_CurrentPhase->SetText(FText::FromString(FString::Printf(TEXT("阶段：%d"), static_cast<int32>(State.Phase))));
    Txt_CurrentTurnPlayer->SetText(FText::FromString(FString::Printf(TEXT("当前座位：%d"), State.CurrentTurnSeat)));
    UTextBlock* SeatWidgets[] = {Seat_Self, Seat_Right, Seat_Top, Seat_Left};
    for (int32 Index = 0; Index < 4; ++Index)
    {
        if (!State.Seats.IsValidIndex(Index)) continue;
        const FMahjongSeatInfo& Seat = State.Seats[Index];
        SeatWidgets[Index]->SetText(FText::FromString(FString::Printf(TEXT("%s\n手牌 %d\n%d 分"), *Seat.PlayerName, Seat.HandTileCount, Seat.Score)));
    }
    UE_LOG(LogMahjongUI, Verbose, TEXT("公共牌桌 UI 刷新：序号=%d"), State.StateSequence);
}

void UMobileMahjongHUDWidget::RefreshPrivateHand(const FMahjongPrivatePlayerState& State)
{
    Panel_SelfHandTiles->ClearChildren();
    UClass* TileWidgetClass = LoadClass<UMobileHandTileWidget>(nullptr, TEXT("/Game/UI/Components/WBP_HandTile.WBP_HandTile_C"));
    if (!TileWidgetClass)
    {
        UE_LOG(LogMahjongUI, Warning, TEXT("尚未找到 WBP_HandTile，私有手牌暂不生成可视组件"));
        return;
    }
    for (const FMahjongTile& Tile : State.Hand.Tiles)
    {
        if (UMobileHandTileWidget* TileWidget = CreateWidget<UMobileHandTileWidget>(GetOwningPlayer(), TileWidgetClass))
        {
            TileWidget->SetTile(Tile, true);
            Panel_SelfHandTiles->AddChildToHorizontalBox(TileWidget);
        }
    }
    UE_LOG(LogMahjongUI, Log, TEXT("私有手牌 UI 刷新：%d 张"), State.Hand.Tiles.Num());
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
