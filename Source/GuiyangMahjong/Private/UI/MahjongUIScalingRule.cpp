#include "UI/MahjongUIScalingRule.h"

float UMahjongUIScalingRule::GetDPIScaleBasedOnSize(const FIntPoint Size) const
{
    const int32 ShortSide = FMath::Max(1, FMath::Min(Size.X, Size.Y));
    const int32 LongSide = FMath::Max(Size.X, Size.Y);
    const float AspectRatio = static_cast<float>(LongSide) / static_cast<float>(ShortSide);

    // 18:9、20:9 等手机保持用户确认的 1.5 倍可读尺寸。
    if (AspectRatio >= 1.72f)
    {
        return 1.50f;
    }

    // 4:3、3:2、16:10 平板使用更温和的缩放，避免固定 1.5 倍挤压内容。
    if (ShortSide <= 900)
    {
        return 1.25f;
    }
    if (ShortSide <= 1600)
    {
        return FMath::GetMappedRangeValueClamped(FVector2D(900.0f, 1600.0f),
            FVector2D(1.25f, 1.30f), static_cast<float>(ShortSide));
    }
    if (ShortSide <= 2200)
    {
        return FMath::GetMappedRangeValueClamped(FVector2D(1600.0f, 2200.0f),
            FVector2D(1.30f, 1.15f), static_cast<float>(ShortSide));
    }
    return 1.10f;
}
