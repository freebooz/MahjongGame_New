#include "Table/MahjongDeckManager.h"
#include "GuiyangMahjongCore.h"

void UMahjongDeckManager::InitializeDeck(const FMahjongRuleConfig& RuleConfig)
{
    if (RuleConfig.TileSetMode != EMahjongTileSetMode::Suited108)
    {
        UE_LOG(LogMahjongCore, Warning, TEXT("Guiyang rules force Suited108; ignoring legacy tile-set mode"));
    }
    Deck.Reset(108);
    DrawIndex = 0;
    int32 UniqueId = 0;

    for (const EMahjongSuit Suit : { EMahjongSuit::Characters, EMahjongSuit::Bamboo, EMahjongSuit::Dots })
    {
        for (int32 Rank = 1; Rank <= 9; ++Rank)
        {
            for (int32 Copy = 0; Copy < 4; ++Copy)
            {
                FMahjongTile& Tile = Deck.AddDefaulted_GetRef();
                Tile.Suit = Suit;
                Tile.Type = EMahjongTileType::Number;
                Tile.Rank = Rank;
                Tile.UniqueId = UniqueId++;
            }
        }
    }

    UE_LOG(LogMahjongCore, Log, TEXT("牌墙初始化完成，共 %d 张牌"), Deck.Num());
}

void UMahjongDeckManager::InitializeStandardDeck()
{
    InitializeDeck(FMahjongRuleConfig());
}

void UMahjongDeckManager::ShuffleDeck(const int32 Seed)
{
    FRandomStream Random(Seed);
    for (int32 Index = Deck.Num() - 1; Index > 0; --Index)
    {
        const int32 SwapIndex = Random.RandRange(0, Index);
        Deck.Swap(Index, SwapIndex);
    }
    DrawIndex = 0;
    UE_LOG(LogMahjongCore, Log, TEXT("服务端洗牌完成，随机种子=%d"), Seed);
}

bool UMahjongDeckManager::DrawTile(FMahjongTile& OutTile)
{
    if (!Deck.IsValidIndex(DrawIndex))
    {
        UE_LOG(LogMahjongCore, Log, TEXT("牌墙已空，触发流局检测"));
        return false;
    }
    OutTile = Deck[DrawIndex++];
    return true;
}

bool UMahjongDeckManager::DealInitialHands(TArray<FMahjongHand>& OutHands, const int32 DealerSeat)
{
    if (DealerSeat < 0 || DealerSeat >= 4 || GetRemainingCount() < 53)
    {
        UE_LOG(LogMahjongCore, Warning, TEXT("发牌失败：庄家座位或剩余牌数非法，庄家=%d，剩余=%d"), DealerSeat, GetRemainingCount());
        return false;
    }
    OutHands.SetNum(4);
    for (FMahjongHand& Hand : OutHands)
    {
        Hand.Tiles.Reset();
        Hand.Melds.Reset();
    }
    for (int32 Round = 0; Round < 13; ++Round)
    {
        for (int32 Seat = 0; Seat < 4; ++Seat)
        {
            FMahjongTile Tile;
            DrawTile(Tile);
            OutHands[Seat].Tiles.Add(Tile);
        }
    }
    FMahjongTile DealerExtra;
    DrawTile(DealerExtra);
    OutHands[DealerSeat].Tiles.Add(DealerExtra);
    for (FMahjongHand& Hand : OutHands) Hand.Sort();
    UE_LOG(LogMahjongCore, Log, TEXT("初始发牌完成：庄家座位=%d，庄家14张，其余玩家13张"), DealerSeat);
    return true;
}
