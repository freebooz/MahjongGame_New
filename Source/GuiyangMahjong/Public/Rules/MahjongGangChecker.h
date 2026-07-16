#pragma once

#include "CoreMinimal.h"
#include "Core/MahjongTypes.h"
#include "UObject/Object.h"
#include "MahjongGangChecker.generated.h"

/** 杠牌候选检测器。只返回服务端可进一步校验的候选牌型。 */
UCLASS()
class GUIYANGMAHJONG_API UMahjongGangChecker : public UObject
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, Category="麻将|规则") static bool CanMingGang(const FMahjongHand& Hand, const FMahjongTile& Discard);
    UFUNCTION(BlueprintPure, Category="麻将|规则") static TArray<int32> FindAnGangRuleIndices(const FMahjongHand& Hand);
    UFUNCTION(BlueprintPure, Category="麻将|规则") static TArray<int32> FindBuGangRuleIndices(const FMahjongHand& Hand);
};
