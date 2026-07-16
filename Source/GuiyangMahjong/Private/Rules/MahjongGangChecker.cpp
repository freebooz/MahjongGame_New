#include "Rules/MahjongGangChecker.h"

bool UMahjongGangChecker::CanMingGang(const FMahjongHand& Hand, const FMahjongTile& Discard)
{
    const int32 Target = Discard.GetRuleIndex();
    if (Target == INDEX_NONE) return false;
    int32 MatchCount = 0;
    for (const FMahjongTile& Tile : Hand.Tiles) if (Tile.GetRuleIndex() == Target) ++MatchCount;
    return MatchCount >= 3;
}

TArray<int32> UMahjongGangChecker::FindAnGangRuleIndices(const FMahjongHand& Hand)
{
    int32 Counts[34] = {};
    for (const FMahjongTile& Tile : Hand.Tiles)
    {
        const int32 Index = Tile.GetRuleIndex();
        if (Index != INDEX_NONE) ++Counts[Index];
    }
    TArray<int32> Result;
    for (int32 Index = 0; Index < 34; ++Index) if (Counts[Index] == 4) Result.Add(Index);
    return Result;
}

TArray<int32> UMahjongGangChecker::FindBuGangRuleIndices(const FMahjongHand& Hand)
{
    TSet<int32> PengIndices;
    for (const FMahjongMeld& Meld : Hand.Melds)
    {
        if (Meld.Type == EMahjongMeldType::Peng && !Meld.Tiles.IsEmpty()) PengIndices.Add(Meld.Tiles[0].GetRuleIndex());
    }
    TArray<int32> Result;
    for (const FMahjongTile& Tile : Hand.Tiles)
    {
        const int32 Index = Tile.GetRuleIndex();
        if (PengIndices.Contains(Index)) Result.AddUnique(Index);
    }
    return Result;
}
