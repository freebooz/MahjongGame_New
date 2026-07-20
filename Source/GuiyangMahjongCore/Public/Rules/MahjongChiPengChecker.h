#pragma once

#include "CoreMinimal.h"
#include "Core/MahjongTypes.h"
#include "UObject/Object.h"
#include "MahjongChiPengChecker.generated.h"

/** 吃碰检测器。贵阳 MVP 使用碰；吃牌接口保留为可配置地区规则能力。 */
UCLASS()
class GUIYANGMAHJONGCORE_API UMahjongChiPengChecker : public UObject
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, Category="麻将|规则") static bool CanPeng(const FMahjongHand& Hand, const FMahjongTile& Discard);
    UFUNCTION(BlueprintPure, Category="麻将|规则") static bool CanChi(const FMahjongHand& Hand, const FMahjongTile& Discard);
};
