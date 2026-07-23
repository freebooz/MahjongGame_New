#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Network/MahjongNetworkTypes.h"
#include "Mahjong3DTableActor.generated.h"

class UActorComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;

/**
 * UMG Viewport 中的三维牌桌表现层。
 * 只消费客户端已有的公开/私有快照，不拥有规则状态，也不发送网络请求。
 */
UCLASS()
class GUIYANGMAHJONGCLIENT_API AMahjong3DTableActor final : public AActor
{
    GENERATED_BODY()

public:
    AMahjong3DTableActor();

    void UpdateLayout(const FMahjongPublicTableState& PublicState,
        const FMahjongPrivatePlayerState& PrivateState, bool bHasPrivateState, int32 LocalSeat);
    void SetSelectedTile(int32 UniqueId);
    /** 将服务端绝对牌墙方位转换为当前客户端以自己为南方的相对方位。 */
    static int32 GetRelativeWallSide(int32 AbsoluteWallSide, int32 LocalSeat);

private:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, Category="Mahjong|Presentation") TObjectPtr<USceneComponent> SceneRoot;
    /** Persistent editor-visible table mesh; runtime tile components are rebuilt around it. */
    UPROPERTY(VisibleAnywhere, Category="Mahjong|Presentation") TObjectPtr<UStaticMeshComponent> TableComponent;
    UPROPERTY(Transient) TArray<TObjectPtr<UActorComponent>> RuntimeComponents;
    UPROPERTY(Transient) TObjectPtr<UStaticMesh> TableMesh;
    UPROPERTY(Transient) TObjectPtr<UStaticMesh> CubeMesh;
    UPROPERTY(Transient) TObjectPtr<UStaticMesh> DefaultTileMesh;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMesh>> TileMeshes;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> BasicMaterial;
    UPROPERTY() FMahjongPublicTableState CachedPublicState;
    UPROPERTY() FMahjongPrivatePlayerState CachedPrivateState;
    bool bCachedPrivateState = false;
    bool bLayoutInitialized = false;
    int32 CachedLocalSeat = 0;
    int32 SelectedTileId = INDEX_NONE;

    void InitializePresentationAssets();
    void RebuildLayout();
    void ClearRuntimeComponents();
    class UStaticMeshComponent* AddBox(const FVector& Location, const FVector& Size,
        const FRotator& Rotation, const FLinearColor& Color);
    UStaticMesh* ResolveTileMesh(const FMahjongTile* Tile, bool bFaceUp) const;
    void AddTile(const FMahjongTile* Tile, bool bFaceUp, bool bUpright,
        const FVector& Location, const FRotator& Rotation, bool bSelected = false);
    void AddTableAndFrame();
    void AddRemainingWall();
    void AddHands();
    void AddDiscards();
    void AddMelds();
    int32 GetRelativeSeat(int32 AbsoluteSeat) const;
};
