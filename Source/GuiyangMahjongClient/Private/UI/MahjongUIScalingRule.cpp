#include "UI/MahjongUIScalingRule.h"

float UMahjongUIScalingRule::GetDPIScaleBasedOnSize(const FIntPoint Size) const
{
    // 手机和平板统一使用 1.0 DPI，防止任何控件因额外倍率被挤出屏幕。
    // 不同宽高比仍由 UMahjongResponsiveScaleBox 负责：手机铺满、平板等比适配。
    return 1.0f;
}
