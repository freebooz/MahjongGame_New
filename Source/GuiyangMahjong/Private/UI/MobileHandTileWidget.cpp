#include "UI/MobileHandTileWidget.h"
#include "UI/MahjongTileVisualLibrary.h"
#include "UI/MahjongUISoundLibrary.h"
#include "Game/GuiyangMahjongPlayerController.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
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
    if (UTexture2D* FaceTexture = UMahjongTileVisualLibrary::LoadFaceTexture(Tile))
    {
        auto MakeFaceBrush = [FaceTexture](const FLinearColor& Tint)
        {
            FSlateBrush Brush;
            Brush.SetResourceObject(FaceTexture);
            Brush.ImageSize = FVector2D(FaceTexture->GetSizeX(), FaceTexture->GetSizeY());
            Brush.DrawAs = ESlateBrushDrawType::Image;
            Brush.TintColor = FSlateColor(Tint);
            return Brush;
        };
        FButtonStyle Style = Btn_Tile->GetStyle();
        Style.SetNormal(MakeFaceBrush(FLinearColor::White));
        Style.SetHovered(MakeFaceBrush(FLinearColor(1.0f, 0.96f, 0.74f, 1.0f)));
        Style.SetPressed(MakeFaceBrush(FLinearColor(0.86f, 0.76f, 0.52f, 1.0f)));
        Style.SetDisabled(MakeFaceBrush(FLinearColor(0.48f, 0.52f, 0.50f, 0.88f)));
        Btn_Tile->SetStyle(Style);
        Txt_TileName->SetVisibility(ESlateVisibility::Collapsed);
    }
    else
    {
        Txt_TileName->SetText(FText::FromString(Tile.ToDebugString()));
        Txt_TileName->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
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
        UMahjongUISoundLibrary::PlayUISound(this, EMahjongUISound::TileSelect);
        SetSelected(true);
        OnTileSelected.Broadcast(this);
        UE_LOG(LogMahjongUI, Log, TEXT("选中手牌：%s"), *TileData.ToDebugString());
        return;
    }
    if (AGuiyangMahjongPlayerController* PC = Cast<AGuiyangMahjongPlayerController>(GetOwningPlayer()))
    {
        UMahjongUISoundLibrary::PlayUISound(this, EMahjongUISound::TilePlay);
        PC->RequestTableAction(EMahjongActionType::Play, TileData.UniqueId);
    }
}
