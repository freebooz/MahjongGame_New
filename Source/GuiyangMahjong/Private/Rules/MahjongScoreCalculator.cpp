#include "Rules/MahjongScoreCalculator.h"
#include "GuiyangMahjong.h"

FMahjongSettlementResult UMahjongScoreCalculator::CalculateWin(const int32 WinnerSeat, const int32 LoserSeat, const bool bSelfDraw,
    const TArray<int32>& JiCounts, const TArray<int32>& GangDeltas, const TArray<int32>& CurrentScores, const FMahjongRuleConfig& Config)
{
    FMahjongSettlementResult Result;
    Result.WinnerSeat = WinnerSeat;
    Result.LoserSeat = bSelfDraw ? INDEX_NONE : LoserSeat;
    Result.bSelfDraw = bSelfDraw;
    Result.PlayerResults.SetNum(4);

    if (WinnerSeat < 0 || WinnerSeat >= 4 || (!bSelfDraw && (LoserSeat < 0 || LoserSeat >= 4 || LoserSeat == WinnerSeat)))
    {
        Result.bDrawGame = true;
        UE_LOG(LogMahjongScore, Warning, TEXT("结算拒绝：赢家或放炮座位非法，赢家=%d，放炮=%d"), WinnerSeat, LoserSeat);
        return Result;
    }

    for (int32 Seat = 0; Seat < 4; ++Seat)
    {
        Result.PlayerResults[Seat].SeatIndex = Seat;
        Result.PlayerResults[Seat].JiScoreDelta = 0;
        Result.PlayerResults[Seat].GangScoreDelta = GangDeltas.IsValidIndex(Seat) ? GangDeltas[Seat] : 0;
    }

    const int32 Unit = Config.BaseScore * (bSelfDraw ? Config.ZiMoMultiplier : Config.DianPaoMultiplier);
    if (bSelfDraw)
    {
        for (int32 Seat = 0; Seat < 4; ++Seat)
        {
            if (Seat == WinnerSeat) continue;
            Result.PlayerResults[Seat].BaseScoreDelta -= Unit;
            Result.PlayerResults[WinnerSeat].BaseScoreDelta += Unit;
        }
    }
    else
    {
        Result.PlayerResults[LoserSeat].BaseScoreDelta -= Unit * 3;
        Result.PlayerResults[WinnerSeat].BaseScoreDelta += Unit * 3;
    }

    // 鸡分采用玩家间差额结算：每位玩家相对其余三家的鸡数差形成严格零和结果。
    for (int32 A = 0; A < 4; ++A)
    {
        for (int32 B = A + 1; B < 4; ++B)
        {
            const int32 ACount = JiCounts.IsValidIndex(A) ? JiCounts[A] : 0;
            const int32 BCount = JiCounts.IsValidIndex(B) ? JiCounts[B] : 0;
            const int32 Delta = (ACount - BCount) * Config.JiScore;
            Result.PlayerResults[A].JiScoreDelta += Delta;
            Result.PlayerResults[B].JiScoreDelta -= Delta;
        }
    }

    for (int32 Seat = 0; Seat < 4; ++Seat)
    {
        FMahjongPlayerScoreResult& Player = Result.PlayerResults[Seat];
        Player.TotalDelta = Player.BaseScoreDelta + Player.JiScoreDelta + Player.GangScoreDelta;
        Player.TotalScore = (CurrentScores.IsValidIndex(Seat) ? CurrentScores[Seat] : 0) + Player.TotalDelta;
    }
    UE_LOG(LogMahjongScore, Log, TEXT("单局结算完成：%s"), *Result.ToDebugString());
    return Result;
}
