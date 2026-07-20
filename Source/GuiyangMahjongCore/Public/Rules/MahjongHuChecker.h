#pragma once

#include "CoreMinimal.h"
#include "Core/MahjongTypes.h"
#include "UObject/Object.h"
#include "MahjongHuChecker.generated.h"

/** 无状态胡牌检查器。只读取手牌，不修改牌局或触发同步。 */
UCLASS()
class GUIYANGMAHJONGCORE_API UMahjongHuChecker : public UObject
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, Category="麻将|规则") static bool CanHu(const FMahjongHand& Hand, bool bEnableQiDui);
    UFUNCTION(BlueprintPure, Category="麻将|规则") static bool IsQiDui(const FMahjongHand& Hand);
private:
    static bool CanFormMelds(TArray<int32>& Counts, int32 RequiredMelds);
};
