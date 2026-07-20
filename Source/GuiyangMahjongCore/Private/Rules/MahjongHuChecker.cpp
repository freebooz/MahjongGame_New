#include "Rules/MahjongHuChecker.h"


bool UMahjongHuChecker::IsQiDui(const FMahjongHand& Hand)
{
    if (Hand.Melds.Num() != 0 || Hand.Tiles.Num() != 14) return false;
    int32 Counts[34] = {};
    for (const FMahjongTile& Tile : Hand.Tiles)
    {
        const int32 Index = Tile.GetRuleIndex();
        if (Index == INDEX_NONE || ++Counts[Index] > 4) return false;
    }
    int32 PairCount = 0;
    for (const int32 Count : Counts) PairCount += Count / 2;
    return PairCount == 7;
}

bool UMahjongHuChecker::CanHu(const FMahjongHand& Hand, const bool bEnableQiDui)
{
    if (bEnableQiDui && IsQiDui(Hand)) return true;
    const int32 RequiredMelds = 4 - Hand.Melds.Num();
    if (RequiredMelds < 0 || Hand.Tiles.Num() != RequiredMelds * 3 + 2) return false;

    TArray<int32> Counts;
    Counts.Init(0, 34);
    for (const FMahjongTile& Tile : Hand.Tiles)
    {
        const int32 Index = Tile.GetRuleIndex();
        if (Index == INDEX_NONE || ++Counts[Index] > 4) return false;
    }

    // 依次尝试每一种可能的将牌，再递归拆解剩余刻子和顺子。
    for (int32 PairIndex = 0; PairIndex < Counts.Num(); ++PairIndex)
    {
        if (Counts[PairIndex] < 2) continue;
        Counts[PairIndex] -= 2;
        if (CanFormMelds(Counts, RequiredMelds)) return true;
        Counts[PairIndex] += 2;
    }
    return false;
}

bool UMahjongHuChecker::CanFormMelds(TArray<int32>& Counts, const int32 RequiredMelds)
{
    if (RequiredMelds == 0)
    {
        for (const int32 Count : Counts) if (Count != 0) return false;
        return true;
    }
    int32 First = INDEX_NONE;
    for (int32 Index = 0; Index < Counts.Num(); ++Index)
    {
        if (Counts[Index] > 0) { First = Index; break; }
    }
    if (First == INDEX_NONE) return false;

    if (Counts[First] >= 3)
    {
        Counts[First] -= 3;
        if (CanFormMelds(Counts, RequiredMelds - 1)) return true;
        Counts[First] += 3;
    }
    // 0..26 为三门数字牌，且顺子不能跨越花色边界。
    if (First < 27 && First % 9 <= 6 && Counts[First + 1] > 0 && Counts[First + 2] > 0)
    {
        --Counts[First]; --Counts[First + 1]; --Counts[First + 2];
        if (CanFormMelds(Counts, RequiredMelds - 1)) return true;
        ++Counts[First]; ++Counts[First + 1]; ++Counts[First + 2];
    }
    return false;
}
