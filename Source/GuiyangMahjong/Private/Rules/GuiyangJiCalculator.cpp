#include "Rules/GuiyangJiCalculator.h"

bool UGuiyangJiCalculator::IsBasicJi(const FMahjongTile& Tile)
{
    return Tile.Type == EMahjongTileType::Number && Tile.Suit == EMahjongSuit::Bamboo && Tile.Rank == 1;
}

int32 UGuiyangJiCalculator::GetFlippedJiRuleIndex(const FMahjongTile& FlippedTile)
{
    const int32 Index = FlippedTile.GetRuleIndex();
    if (Index < 0) return INDEX_NONE;
    if (Index < 27) return (Index / 9) * 9 + (Index + 1) % 9;
    if (Index <= 30) return 27 + (Index - 27 + 1) % 4;
    return 31 + (Index - 31 + 1) % 3;
}

int32 UGuiyangJiCalculator::CountJi(const FMahjongHand& Hand, const FMahjongTile& FlippedTile)
{
    const int32 FlippedJiIndex = GetFlippedJiRuleIndex(FlippedTile);
    int32 Count = 0;
    auto CountTile = [&Count, FlippedJiIndex](const FMahjongTile& Tile)
    {
        if (IsBasicJi(Tile) || Tile.GetRuleIndex() == FlippedJiIndex) ++Count;
    };
    for (const FMahjongTile& Tile : Hand.Tiles) CountTile(Tile);
    for (const FMahjongMeld& Meld : Hand.Melds) for (const FMahjongTile& Tile : Meld.Tiles) CountTile(Tile);
    return Count;
}
