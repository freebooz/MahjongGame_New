#include "UI/MahjongTileVisualLibrary.h"

#include "Engine/Texture2D.h"

FString UMahjongTileVisualLibrary::GetFaceTexturePath(const FMahjongTile& Tile)
{
    if (Tile.Type != EMahjongTileType::Number || Tile.Rank < 1 || Tile.Rank > 9)
    {
        return FString();
    }

    const TCHAR* SuitName = nullptr;
    switch (Tile.Suit)
    {
    case EMahjongSuit::Characters: SuitName = TEXT("Wan"); break;
    case EMahjongSuit::Bamboo: SuitName = TEXT("Tiao"); break;
    case EMahjongSuit::Dots: SuitName = TEXT("Tong"); break;
    default: return FString();
    }

    const FString AssetName = FString::Printf(TEXT("T_Tile_%s_%02d"), SuitName, Tile.Rank);
    return FString::Printf(TEXT("/Game/UI/Textures/Tiles/%s.%s"), *AssetName, *AssetName);
}

UTexture2D* UMahjongTileVisualLibrary::LoadFaceTexture(const FMahjongTile& Tile)
{
    const FString Path = GetFaceTexturePath(Tile);
    return Path.IsEmpty() ? nullptr : LoadObject<UTexture2D>(nullptr, *Path);
}
