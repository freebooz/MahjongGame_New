#include "Core/MahjongTypes.h"

int32 FMahjongTile::GetRuleIndex() const
{
    if (Type == EMahjongTileType::Number)
    {
        const int32 SuitOffset = Suit == EMahjongSuit::Characters ? 0 : Suit == EMahjongSuit::Bamboo ? 9 : 18;
        return Rank >= 1 && Rank <= 9 ? SuitOffset + Rank - 1 : INDEX_NONE;
    }
    switch (Type)
    {
    case EMahjongTileType::East: return 27;
    case EMahjongTileType::South: return 28;
    case EMahjongTileType::West: return 29;
    case EMahjongTileType::North: return 30;
    case EMahjongTileType::RedDragon: return 31;
    case EMahjongTileType::GreenDragon: return 32;
    case EMahjongTileType::WhiteDragon: return 33;
    default: return INDEX_NONE;
    }
}

FString FMahjongTile::ToDebugString() const
{
    if (Type == EMahjongTileType::Number)
    {
        static const TCHAR* Digits[] = { TEXT("零"), TEXT("一"), TEXT("二"), TEXT("三"), TEXT("四"), TEXT("五"), TEXT("六"), TEXT("七"), TEXT("八"), TEXT("九") };
        const TCHAR* SuitText = Suit == EMahjongSuit::Characters ? TEXT("万") : Suit == EMahjongSuit::Bamboo ? TEXT("条") : TEXT("筒");
        return FString::Printf(TEXT("%s%s"), Rank >= 0 && Rank <= 9 ? Digits[Rank] : TEXT("?"), SuitText);
    }
    switch (Type)
    {
    case EMahjongTileType::East: return TEXT("东风");
    case EMahjongTileType::South: return TEXT("南风");
    case EMahjongTileType::West: return TEXT("西风");
    case EMahjongTileType::North: return TEXT("北风");
    case EMahjongTileType::RedDragon: return TEXT("红中");
    case EMahjongTileType::GreenDragon: return TEXT("发财");
    case EMahjongTileType::WhiteDragon: return TEXT("白板");
    default: return TEXT("无效牌");
    }
}

void FMahjongHand::Sort()
{
    Tiles.Sort([](const FMahjongTile& A, const FMahjongTile& B)
    {
        const int32 AI = A.GetRuleIndex();
        const int32 BI = B.GetRuleIndex();
        return AI == BI ? A.UniqueId < B.UniqueId : AI < BI;
    });
}

FString FMahjongHand::ToDebugString() const
{
    TArray<FString> Names;
    Algo::Transform(Tiles, Names, [](const FMahjongTile& Tile) { return Tile.ToDebugString(); });
    return FString::Join(Names, TEXT("、"));
}

FString FMahjongAction::ToDebugString() const
{
    const TCHAR* ActionText = TEXT("过");
    switch (Type)
    {
    case EMahjongActionType::Draw: ActionText = TEXT("摸牌"); break;
    case EMahjongActionType::Play: ActionText = TEXT("出牌"); break;
    case EMahjongActionType::Peng: ActionText = TEXT("碰"); break;
    case EMahjongActionType::MingGang: ActionText = TEXT("明杠"); break;
    case EMahjongActionType::AnGang: ActionText = TEXT("暗杠"); break;
    case EMahjongActionType::BuGang: ActionText = TEXT("补杠"); break;
    case EMahjongActionType::Hu: ActionText = TEXT("胡"); break;
    default: break;
    }
    return FString::Printf(TEXT("操作：%s，目标牌=%s，来源座位=%d"), ActionText, *TargetTile.ToDebugString(), SourceSeat);
}

FString FMahjongSettlementResult::ToDebugString() const
{
    return bDrawGame
        ? TEXT("结算：流局")
        : FString::Printf(TEXT("结算：赢家=%d，放炮=%d，自摸=%s，鸡牌=%d张"), WinnerSeat, LoserSeat, bSelfDraw ? TEXT("是") : TEXT("否"), JiTiles.Num());
}
