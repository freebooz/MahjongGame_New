#include "UI/MahjongResponsiveScaleBox.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

EStretch::Type UMahjongResponsiveScaleBox::ResolveStretchForViewport(const FIntPoint ViewportSize)
{
    const int32 ShortSide = FMath::Max(1, FMath::Min(ViewportSize.X, ViewportSize.Y));
    const int32 LongSide = FMath::Max(ViewportSize.X, ViewportSize.Y);
    const float AspectRatio = static_cast<float>(LongSide) / static_cast<float>(ShortSide);
    return AspectRatio < 1.72f ? EStretch::ScaleToFit : EStretch::Fill;
}

void UMahjongResponsiveScaleBox::SynchronizeProperties()
{
#if PLATFORM_ANDROID
    FVector2D ViewportSize = FVector2D::ZeroVector;
    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->GetViewportSize(ViewportSize);
    }
    if (ViewportSize.X > 0.0f && ViewportSize.Y > 0.0f)
    {
        SetStretch(ResolveStretchForViewport(FIntPoint(
            FMath::RoundToInt(ViewportSize.X), FMath::RoundToInt(ViewportSize.Y))));
    }
#endif
    Super::SynchronizeProperties();
}
