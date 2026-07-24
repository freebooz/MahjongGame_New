#include "UI/MahjongResponsiveScaleBox.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

EStretch::Type UMahjongResponsiveScaleBox::ResolveStretchForViewport(const FIntPoint ViewportSize)
{
    // Foreground controls must keep the authored 16:9 geometry on phones, tablets and desktop.
    // Full-screen coverage is handled by the independent background layer; stretching the
    // foreground changes hit targets and pushes edge controls outside the visible viewport.
    return EStretch::ScaleToFit;
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
