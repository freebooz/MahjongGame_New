#pragma once

#include "CoreMinimal.h"
#include "Engine/DPICustomScalingRule.h"
#include "MahjongUIScalingRule.generated.h"

/** 手机和平板共用的响应式 DPI 规则。 */
UCLASS()
class GUIYANGMAHJONG_API UMahjongUIScalingRule final : public UDPICustomScalingRule
{
    GENERATED_BODY()

public:
    virtual float GetDPIScaleBasedOnSize(FIntPoint Size) const override;
};
