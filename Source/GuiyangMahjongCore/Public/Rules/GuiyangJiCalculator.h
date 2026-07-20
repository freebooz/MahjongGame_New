#pragma once

#include "CoreMinimal.h"
#include "Core/MahjongTypes.h"
#include "UObject/Object.h"
#include "GuiyangJiCalculator.generated.h"

/** 贵阳捉鸡 MVP 计算器：支持幺鸡和翻鸡；复杂冲锋鸡、责任鸡、乌骨鸡以独立配置保留。 */
UCLASS()
class GUIYANGMAHJONGCORE_API UGuiyangJiCalculator : public UObject
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, Category="麻将|捉鸡") static bool IsBasicJi(const FMahjongTile& Tile);
    UFUNCTION(BlueprintPure, Category="麻将|捉鸡") static int32 GetFlippedJiRuleIndex(const FMahjongTile& FlippedTile);
    UFUNCTION(BlueprintPure, Category="麻将|捉鸡") static int32 CountJi(const FMahjongHand& Hand, const FMahjongTile& FlippedTile);
    static bool IsWuGuJi(const FMahjongTile& Tile);
    static int32 CountTileJiUnits(const FMahjongTile& Tile, const FMahjongTile& FlippedTile,
        const FMahjongRuleConfig& Config);
    static int32 CountJiUnits(const FMahjongHand& Hand, const FMahjongTile& FlippedTile,
        const FMahjongRuleConfig& Config);
};
