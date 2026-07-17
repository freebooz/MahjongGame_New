#include "Game/Mahjong3DTableActor.h"

#include "UI/MahjongTileVisualLibrary.h"
#include "Components/Image.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Widgets/Images/SImage.h"

namespace
{
    constexpr float TileWidth = 44.0f;
    constexpr float TileHeight = 62.0f;
    constexpr float TileDepth = 30.0f;

    FVector RotateAroundTable(const FVector& Position, const int32 RelativeSeat)
    {
        switch (RelativeSeat)
        {
        case 1: return FVector(-Position.Y, Position.X, Position.Z);
        case 2: return FVector(-Position.X, -Position.Y, Position.Z);
        case 3: return FVector(Position.Y, -Position.X, Position.Z);
        default: return Position;
        }
    }

    FRotator RotateAroundTable(const FRotator& Rotation, const int32 RelativeSeat)
    {
        return FRotator(Rotation.Pitch, Rotation.Yaw + 90.0f * RelativeSeat, Rotation.Roll);
    }
}

void UMahjong3DTileFaceWidget::SetTileFace(const FMahjongTile* Tile, const bool bFaceUp)
{
    FaceTexture = bFaceUp && Tile
        ? UMahjongTileVisualLibrary::LoadFaceTexture(*Tile)
        : LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/Textures/Tiles/T_Tile_Back.T_Tile_Back"));
    FaceBrush = FSlateBrush();
    FaceBrush.DrawAs = ESlateBrushDrawType::Image;
    FaceBrush.ImageSize = FVector2D(64.0f, 88.0f);
    FaceBrush.TintColor = FSlateColor(FLinearColor::White);
    FaceBrush.SetResourceObject(FaceTexture);
    InvalidateLayoutAndVolatility();
}

TSharedRef<SWidget> UMahjong3DTileFaceWidget::RebuildWidget()
{
    return SNew(SImage).Image(&FaceBrush);
}

AMahjong3DTableActor::AMahjong3DTableActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
    CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    TileMesh = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Game/Art/Mahjong/Tiles/SM_MahjongTile.SM_MahjongTile"));
    BasicMaterial = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
}

void AMahjong3DTableActor::UpdateLayout(const FMahjongPublicTableState& PublicState,
    const FMahjongPrivatePlayerState& PrivateState, const bool bHasPrivateState, const int32 LocalSeat)
{
    CachedPublicState = PublicState;
    CachedPrivateState = PrivateState;
    bCachedPrivateState = bHasPrivateState;
    CachedLocalSeat = FMath::Clamp(LocalSeat, 0, 3);
    RebuildLayout();
}

void AMahjong3DTableActor::SetSelectedTile(const int32 UniqueId)
{
    if (SelectedTileId == UniqueId) return;
    SelectedTileId = UniqueId;
    RebuildLayout();
}

void AMahjong3DTableActor::RebuildLayout()
{
    ClearRuntimeComponents();
    AddTableAndFrame();
    AddRemainingWall();
    AddHands();
    AddDiscards();
    AddMelds();
}

void AMahjong3DTableActor::ClearRuntimeComponents()
{
    for (UActorComponent* Component : RuntimeComponents)
    {
        if (IsValid(Component)) Component->DestroyComponent();
    }
    RuntimeComponents.Reset();
}

UStaticMeshComponent* AMahjong3DTableActor::AddBox(const FVector& Location, const FVector& Size,
    const FRotator& Rotation, const FLinearColor& Color)
{
    if (!CubeMesh) return nullptr;
    UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
    Component->SetStaticMesh(CubeMesh);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetCastShadow(true);
    Component->SetRelativeLocation(Location);
    Component->SetRelativeRotation(Rotation);
    Component->SetRelativeScale3D(Size / 100.0f);
    Component->SetupAttachment(SceneRoot);
    AddInstanceComponent(Component);
    Component->RegisterComponent();
    if (BasicMaterial)
    {
        UMaterialInstanceDynamic* DynamicMaterial = Component->CreateDynamicMaterialInstance(0, BasicMaterial);
        if (DynamicMaterial)
        {
            DynamicMaterial->SetVectorParameterValue(TEXT("Color"), Color);
            DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), Color);
        }
    }
    RuntimeComponents.Add(Component);
    return Component;
}

void AMahjong3DTableActor::AddTile(const FMahjongTile* Tile, const bool bFaceUp, const bool bUpright,
    const FVector& Location, const FRotator& Rotation, const bool bSelected)
{
    FVector TileLocation = Location;
    if (bSelected) TileLocation.Z += 16.0f;
    if (TileMesh)
    {
        UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
        Component->SetStaticMesh(TileMesh);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetCastShadow(true);
        FRotator MeshRotation = Rotation;
        if (!bUpright)
        {
            // 正面朝上的弃牌使用 -90°，牌墙/暗牌使用 +90° 让绿色牌背朝上。
            MeshRotation.Roll += bFaceUp ? -90.0f : 90.0f;
        }
        else
        {
            // Blender 模型枢轴在底部中心；现有布局坐标以牌体中心为准。
            TileLocation.Z -= TileHeight * 0.5f;
        }
        Component->SetRelativeLocation(TileLocation);
        Component->SetRelativeRotation(MeshRotation);
        Component->SetRelativeScale3D(FVector(TileWidth / 3.2f));
        Component->SetupAttachment(SceneRoot);
        AddInstanceComponent(Component);
        Component->RegisterComponent();
        RuntimeComponents.Add(Component);
    }
    else
    {
        const FVector Size = bUpright
            ? FVector(TileWidth, TileDepth, TileHeight)
            : FVector(TileHeight, TileWidth, TileDepth);
        AddBox(TileLocation, Size, Rotation,
            bSelected ? FLinearColor(0.95f, 0.68f, 0.16f) : FLinearColor(0.92f, 0.88f, 0.72f));
    }
    if (bFaceUp || bUpright)
    {
        AddTileFace(Tile, bFaceUp, bUpright, TileLocation, Rotation);
    }
}

void AMahjong3DTableActor::AddTileFace(const FMahjongTile* Tile, const bool bFaceUp, const bool bUpright,
    const FVector& Location, const FRotator& Rotation)
{
    UWidgetComponent* Face = NewObject<UWidgetComponent>(this);
    Face->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Face->SetWidgetSpace(EWidgetSpace::World);
    Face->SetDrawSize(FVector2D(64.0f, 88.0f));
    Face->SetPivot(FVector2D(0.5f, 0.5f));
    Face->SetTwoSided(true);
    Face->SetBlendMode(EWidgetBlendMode::Transparent);
    Face->SetRelativeScale3D(FVector(0.62f));
    Face->SetupAttachment(SceneRoot);

    if (bUpright)
    {
        const FVector Forward = Rotation.RotateVector(FVector(0.0f, -1.0f, 0.0f));
        Face->SetRelativeLocation(Location + Forward * (TileDepth * 0.55f));
        Face->SetRelativeRotation(FRotator(0.0f, Rotation.Yaw - 90.0f, 0.0f));
    }
    else
    {
        Face->SetRelativeLocation(Location + FVector(0.0f, 0.0f, TileDepth * 0.56f));
        Face->SetRelativeRotation(FRotator(90.0f, Rotation.Yaw, 0.0f));
    }

    UMahjong3DTileFaceWidget* FaceWidget = NewObject<UMahjong3DTileFaceWidget>(Face);
    FaceWidget->SetTileFace(Tile, bFaceUp);
    Face->SetWidget(FaceWidget);
    AddInstanceComponent(Face);
    Face->RegisterComponent();
    RuntimeComponents.Add(Face);
}

void AMahjong3DTableActor::AddTableAndFrame()
{
    AddBox(FVector(0.0f, 0.0f, -18.0f), FVector(1040.0f, 760.0f, 36.0f), FRotator::ZeroRotator,
        FLinearColor(0.05f, 0.18f, 0.11f));
    AddBox(FVector(0.0f, 0.0f, 1.0f), FVector(970.0f, 690.0f, 8.0f), FRotator::ZeroRotator,
        FLinearColor(0.015f, 0.32f, 0.25f));
    const FLinearColor Wood(0.24f, 0.10f, 0.035f);
    AddBox(FVector(0.0f, -372.0f, 8.0f), FVector(1060.0f, 26.0f, 34.0f), FRotator::ZeroRotator, Wood);
    AddBox(FVector(0.0f, 372.0f, 8.0f), FVector(1060.0f, 26.0f, 34.0f), FRotator::ZeroRotator, Wood);
    AddBox(FVector(-522.0f, 0.0f, 8.0f), FVector(26.0f, 770.0f, 34.0f), FRotator::ZeroRotator, Wood);
    AddBox(FVector(522.0f, 0.0f, 8.0f), FVector(26.0f, 770.0f, 34.0f), FRotator::ZeroRotator, Wood);
    AddBox(FVector(0.0f, 0.0f, 7.0f), FVector(150.0f, 150.0f, 10.0f), FRotator::ZeroRotator,
        FLinearColor(0.02f, 0.08f, 0.07f));
}

void AMahjong3DTableActor::AddRemainingWall()
{
    const int32 Remaining = FMath::Clamp(CachedPublicState.RemainingTileCount, 0, 108);
    int32 Added = 0;
    for (int32 Seat = 0; Seat < 4 && Added < Remaining; ++Seat)
    {
        const int32 SideCount = FMath::Min(28, Remaining - Added);
        for (int32 Index = 0; Index < SideCount; ++Index)
        {
            const int32 Column = Index / 2;
            const int32 Level = Index % 2;
            const FVector Base(-390.0f + Column * 58.0f, -245.0f, 15.0f + Level * 13.0f);
            AddTile(nullptr, false, false, RotateAroundTable(Base, Seat),
                FRotator(0.0f, 90.0f * Seat, 0.0f));
            ++Added;
        }
    }
}

void AMahjong3DTableActor::AddHands()
{
    if (bCachedPrivateState)
    {
        const TArray<FMahjongTile>& Tiles = CachedPrivateState.Hand.Tiles;
        const float StartX = -0.5f * (Tiles.Num() - 1) * 49.0f;
        for (int32 Index = 0; Index < Tiles.Num(); ++Index)
        {
            const float DrawGap = Tiles.Num() == 14 && Index == 13 ? 16.0f : 0.0f;
            AddTile(&Tiles[Index], true, true,
                FVector(StartX + Index * 49.0f + DrawGap, -316.0f, TileHeight * 0.5f + 9.0f),
                FRotator::ZeroRotator, Tiles[Index].UniqueId == SelectedTileId);
        }
    }

    for (const FMahjongSeatInfo& Seat : CachedPublicState.Seats)
    {
        const int32 RelativeSeat = GetRelativeSeat(Seat.SeatIndex);
        if (RelativeSeat == 0) continue;
        const int32 Count = FMath::Clamp(Seat.HandTileCount, 0, 14);
        const float StartX = -0.5f * (Count - 1) * 43.0f;
        for (int32 Index = 0; Index < Count; ++Index)
        {
            const FVector Base(StartX + Index * 43.0f, -316.0f, TileHeight * 0.5f + 9.0f);
            AddTile(nullptr, false, true, RotateAroundTable(Base, RelativeSeat),
                RotateAroundTable(FRotator::ZeroRotator, RelativeSeat));
        }
    }
}

void AMahjong3DTableActor::AddDiscards()
{
    for (const FMahjongDiscardRecord& Record : CachedPublicState.Discards)
    {
        if (Record.bClaimed) continue;
        const int32 RelativeSeat = GetRelativeSeat(Record.SeatIndex);
        if (RelativeSeat == INDEX_NONE) continue;
        int32 SeatSequence = 0;
        for (const FMahjongDiscardRecord& Previous : CachedPublicState.Discards)
        {
            if (&Previous == &Record) break;
            if (!Previous.bClaimed && GetRelativeSeat(Previous.SeatIndex) == RelativeSeat) ++SeatSequence;
        }
        const int32 Column = SeatSequence % 6;
        const int32 Row = SeatSequence / 6;
        const FVector Base(-145.0f + Column * 58.0f, -126.0f - Row * 47.0f, 14.0f);
        AddTile(&Record.Tile, true, false, RotateAroundTable(Base, RelativeSeat),
            RotateAroundTable(FRotator::ZeroRotator, RelativeSeat),
            Record.Sequence == CachedPublicState.Discards.Last().Sequence);
    }
}

void AMahjong3DTableActor::AddMelds()
{
    int32 MeldIndexBySeat[4] = {};
    for (const FMahjongMeld& Meld : CachedPublicState.PublicMelds)
    {
        const int32 RelativeSeat = GetRelativeSeat(Meld.OwnerSeat);
        if (RelativeSeat == INDEX_NONE) continue;
        const int32 MeldIndex = MeldIndexBySeat[RelativeSeat]++;
        for (int32 TileIndex = 0; TileIndex < Meld.Tiles.Num(); ++TileIndex)
        {
            const FMahjongTile& Tile = Meld.Tiles[TileIndex];
            const FVector Base(-420.0f + MeldIndex * 205.0f + TileIndex * 51.0f, -267.0f, 14.0f);
            AddTile(Tile.IsValid() ? &Tile : nullptr, Tile.IsValid(), false,
                RotateAroundTable(Base, RelativeSeat), RotateAroundTable(FRotator::ZeroRotator, RelativeSeat));
        }
    }
}

int32 AMahjong3DTableActor::GetRelativeSeat(const int32 AbsoluteSeat) const
{
    if (AbsoluteSeat < 0 || AbsoluteSeat >= 4) return INDEX_NONE;
    return (AbsoluteSeat - CachedLocalSeat + 4) % 4;
}
