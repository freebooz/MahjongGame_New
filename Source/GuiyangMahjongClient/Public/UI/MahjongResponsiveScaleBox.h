#pragma once

#include "CoreMinimal.h"
#include "Components/ScaleBox.h"
#include "MahjongResponsiveScaleBox.generated.h"

/** 手机宽屏铺满，平板保持前景设计比例并由背景继续填满屏幕。 */
UCLASS()
class GUIYANGMAHJONGCLIENT_API UMahjongResponsiveScaleBox final : public UScaleBox
{
    GENERATED_BODY()

public:
    static EStretch::Type ResolveStretchForViewport(FIntPoint ViewportSize);

protected:
    virtual void SynchronizeProperties() override;
};
