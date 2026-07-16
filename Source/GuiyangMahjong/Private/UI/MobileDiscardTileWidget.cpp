#include "UI/MobileDiscardTileWidget.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"

void UMobileDiscardTileWidget::SetDiscard(const FMahjongTile& Tile, const bool bLatest, const float SeatRotation)
{
    Txt_TileName->SetText(FText::FromString(Tile.ToDebugString()));
    Border_Tile->SetBrushColor(bLatest ? FLinearColor(1.0f, 0.72f, 0.16f, 1.0f) : FLinearColor::White);
    SetRenderTransformAngle(SeatRotation);
}
