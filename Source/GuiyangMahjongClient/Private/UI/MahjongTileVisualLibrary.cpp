#include "UI/MahjongTileVisualLibrary.h"

#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"

namespace
{
    constexpr TCHAR FaceAtlasPath[] =
        TEXT("/Game/Art/Mahjong/Mahjong50/Textures/T_Mahjong50_FaceAtlas_BaseColor.T_Mahjong50_FaceAtlas_BaseColor");
    constexpr float AtlasWidth = 8192.0f;
    constexpr float AtlasHeight = 4096.0f;
    constexpr float FaceWidth = 704.0f;
    constexpr float FaceHeight = 1024.0f;
    constexpr float FirstFaceX = 160.0f;
    constexpr float ColumnPitch = 896.0f;
}

FString UMahjongTileVisualLibrary::GetFaceTexturePath(const FMahjongTile& Tile)
{
    int32 Column = 0;
    int32 Row = 0;
    return GetFaceAtlasCell(Tile, Column, Row) ? FString(FaceAtlasPath) : FString();
}

bool UMahjongTileVisualLibrary::GetFaceAtlasCell(const FMahjongTile& Tile,
    int32& OutColumn, int32& OutRowFromBottom)
{
    const int32 RuleIndex = Tile.GetRuleIndex();
    if (RuleIndex >= 0 && RuleIndex < 27)
    {
        if (Tile.Type != EMahjongTileType::Number
            || (Tile.Suit != EMahjongSuit::Characters
                && Tile.Suit != EMahjongSuit::Bamboo
                && Tile.Suit != EMahjongSuit::Dots))
        {
            return false;
        }
        OutColumn = RuleIndex % 9;
        OutRowFromBottom = 3 - RuleIndex / 9;
        return true;
    }

    // The atlas honor row is North, White, South, Red, Green, East, West.
    switch (Tile.Type)
    {
    case EMahjongTileType::North: OutColumn = 0; break;
    case EMahjongTileType::WhiteDragon: OutColumn = 1; break;
    case EMahjongTileType::South: OutColumn = 2; break;
    case EMahjongTileType::RedDragon: OutColumn = 3; break;
    case EMahjongTileType::GreenDragon: OutColumn = 4; break;
    case EMahjongTileType::East: OutColumn = 5; break;
    case EMahjongTileType::West: OutColumn = 6; break;
    default: return false;
    }
    OutRowFromBottom = 0;
    return true;
}

bool UMahjongTileVisualLibrary::ConfigureFaceBrush(const FMahjongTile& Tile,
    FSlateBrush& OutBrush, const FLinearColor& Tint)
{
    int32 Column = 0;
    int32 RowFromBottom = 0;
    UTexture2D* Atlas = LoadFaceTexture(Tile);
    if (!Atlas || !GetFaceAtlasCell(Tile, Column, RowFromBottom))
    {
        return false;
    }

    const FVector2f UVMin(
        (FirstFaceX + Column * ColumnPitch) / AtlasWidth,
        RowFromBottom * FaceHeight / AtlasHeight);
    const FVector2f UVMax(
        UVMin.X + FaceWidth / AtlasWidth,
        UVMin.Y + FaceHeight / AtlasHeight);
    OutBrush = FSlateBrush();
    OutBrush.SetResourceObject(Atlas);
    OutBrush.ImageSize = FVector2D(FaceWidth, FaceHeight);
    OutBrush.DrawAs = ESlateBrushDrawType::Image;
    OutBrush.TintColor = FSlateColor(Tint);
    OutBrush.SetUVRegion(FBox2f(UVMin, UVMax));
    return true;
}

UTexture2D* UMahjongTileVisualLibrary::LoadFaceTexture(const FMahjongTile& Tile)
{
    const FString Path = GetFaceTexturePath(Tile);
    return Path.IsEmpty() ? nullptr : LoadObject<UTexture2D>(nullptr, *Path);
}
