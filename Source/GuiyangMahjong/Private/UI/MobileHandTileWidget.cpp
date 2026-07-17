#include "UI/MobileHandTileWidget.h"
#include "Game/GuiyangMahjongPlayerController.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GuiyangMahjong.h"

void UMobileHandTileWidget::NativeConstruct()
{
    Super::NativeConstruct();
    Btn_Tile->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleTileClicked);
}

void UMobileHandTileWidget::SetTile(const FMahjongTile& Tile, const bool bInteractive)
{
    TileData = Tile;
    SetSelected(false);
    Txt_TileName->SetText(FText::FromString(Tile.ToDebugString()));
    Btn_Tile->SetIsEnabled(bInteractive);
}

void UMobileHandTileWidget::SetSelected(const bool bInSelected)
{
    bSelected = bInSelected;
    SetRenderTranslation(bSelected ? FVector2D(0.0, -18.0) : FVector2D::ZeroVector);
}

void UMobileHandTileWidget::HandleTileClicked()
{
    if (!bSelected)
    {
        SetSelected(true);
        OnTileSelected.Broadcast(this);
        UE_LOG(LogMahjongUI, Log, TEXT("选中手牌：%s"), *TileData.ToDebugString());
        return;
    }
    if (AGuiyangMahjongPlayerController* PC = Cast<AGuiyangMahjongPlayerController>(GetOwningPlayer()))
    {
        PC->RequestTableAction(EMahjongActionType::Play, TileData.UniqueId);
    }
}
