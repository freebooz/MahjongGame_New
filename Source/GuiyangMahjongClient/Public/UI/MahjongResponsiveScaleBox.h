#pragma once

#include "CoreMinimal.h"
#include "Components/ScaleBox.h"
#include "MahjongResponsiveScaleBox.generated.h"

/** 前景始终等比适配；独立背景层负责覆盖手机、平板与桌面全屏。 */
UCLASS()
class GUIYANGMAHJONGCLIENT_API UMahjongResponsiveScaleBox final : public UScaleBox
{
    GENERATED_BODY()

public:
    static EStretch::Type ResolveStretchForViewport(FIntPoint ViewportSize);

protected:
    virtual void SynchronizeProperties() override;
};
