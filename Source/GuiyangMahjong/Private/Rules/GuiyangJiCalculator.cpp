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

bool UGuiyangJiCalculator::IsWuGuJi(const FMahjongTile& Tile)
{
    return Tile.Type == EMahjongTileType::Number && Tile.Suit == EMahjongSuit::Dots && Tile.Rank == 8;
}

int32 UGuiyangJiCalculator::CountTileJiUnits(const FMahjongTile& Tile, const FMahjongTile& FlippedTile,
    const FMahjongRuleConfig& Config)
{
    int32 Units = 0;
    if (IsBasicJi(Tile)) Units = FMath::Max(Units, Config.BasicJiValue);
    if (Config.bEnableWuGuJi && IsWuGuJi(Tile)) Units = FMath::Max(Units, Config.WuGuJiValue);
    if (Tile.GetRuleIndex() == GetFlippedJiRuleIndex(FlippedTile))
        Units = FMath::Max(Units, Config.FlippedJiValue);
    return Units;
}

int32 UGuiyangJiCalculator::CountJiUnits(const FMahjongHand& Hand, const FMahjongTile& FlippedTile,
    const FMahjongRuleConfig& Config)
{
    int32 Units = 0;
    for (const FMahjongTile& Tile : Hand.Tiles) Units += CountTileJiUnits(Tile, FlippedTile, Config);
    const bool bCountMelds = Config.JiCountingScope == EMahjongJiCountingScope::HandAndMeld
        || Config.JiCountingScope == EMahjongJiCountingScope::HandMeldAndDiscard;
    if (bCountMelds)
    {
        for (const FMahjongMeld& Meld : Hand.Melds)
            for (const FMahjongTile& Tile : Meld.Tiles) Units += CountTileJiUnits(Tile, FlippedTile, Config);
    }
    return Units;
}
