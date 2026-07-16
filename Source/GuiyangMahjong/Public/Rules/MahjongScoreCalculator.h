#pragma once

#include "CoreMinimal.h"
#include "Core/MahjongTypes.h"
#include "UObject/Object.h"
#include "MahjongScoreCalculator.generated.h"

/** 单局零和结算器。所有结果由服务端计算，再复制给客户端展示。 */
UCLASS()
class GUIYANGMAHJONG_API UMahjongScoreCalculator : public UObject
{
    GENERATED_BODY()
public:
    static FMahjongSettlementResult CalculateWins(const TArray<int32>& WinningSeats, int32 LoserSeat, bool bSelfDraw,
        const TArray<int32>& JiCounts, const TArray<int32>& GangDeltas, const TArray<int32>& CurrentScores,
        const FMahjongRuleConfig& Config);
    static FMahjongSettlementResult CalculateWinsWithSpecialJi(const TArray<int32>& WinningSeats, int32 LoserSeat,
        bool bSelfDraw, const TArray<int32>& JiCounts, const TArray<int32>& SpecialJiDeltas,
        const TArray<int32>& GangDeltas, const TArray<int32>& CurrentScores, const FMahjongRuleConfig& Config);
    static FMahjongSettlementResult CalculateWin(int32 WinnerSeat, int32 LoserSeat, bool bSelfDraw,
        const TArray<int32>& JiCounts, const TArray<int32>& GangDeltas, const TArray<int32>& CurrentScores,
        const FMahjongRuleConfig& Config);
};
