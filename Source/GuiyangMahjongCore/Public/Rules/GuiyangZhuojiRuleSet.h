#pragma once

#include "CoreMinimal.h"
#include "Core/MahjongTypes.h"
#include "UObject/Object.h"
#include "GuiyangZhuojiRuleSet.generated.h"

/** 项目默认贵阳捉鸡 MVP 规则集。地区复杂规则通过 Config 显式开关，不伪装为完整地方规则。 */
UCLASS(BlueprintType)
class GUIYANGMAHJONGCORE_API UGuiyangZhuojiRuleSet : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="麻将|规则") FMahjongRuleConfig Config;
    UFUNCTION(BlueprintPure, Category="麻将|规则") bool CanHu(const FMahjongHand& Hand) const;
};
