#include "UI/MobileDiscardTileWidget.h"
#include "UI/MahjongTileVisualLibrary.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

void UMobileDiscardTileWidget::SetDiscard(const FMahjongTile& Tile, const bool bLatest)
{
    if (UTexture2D* FaceTexture = UMahjongTileVisualLibrary::LoadFaceTexture(Tile))
    {
        Border_Tile->SetBrushFromTexture(FaceTexture);
        Border_Tile->SetBrushColor(bLatest
            ? FLinearColor(1.0f, 0.88f, 0.58f, 1.0f)
            : FLinearColor::White);
        Txt_TileName->SetVisibility(ESlateVisibility::Collapsed);
    }
    else
    {
        Border_Tile->SetBrushColor(bLatest ? FLinearColor(1.0f, 0.72f, 0.16f, 1.0f) : FLinearColor::White);
        Txt_TileName->SetText(FText::FromString(Tile.IsValid() ? Tile.ToDebugString() : TEXT("暗牌")));
        Txt_TileName->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    // 四方牌池的位置已经表达座位方向。旋转整个组件会连中文牌名一起旋转，降低可读性。
    SetRenderTransformAngle(0.0f);
}
