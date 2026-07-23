#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MahjongRoomPresentationActor.generated.h"

class ACameraActor;
class AMahjong3DTableActor;
class UChildActorComponent;
class USceneComponent;
class USpotLightComponent;

/**
 * Client-only visual root for the network-neutral MahjongRoomMap.
 *
 * Place this actor in MahjongRoomVisualPreviewMap to tune the table, camera and
 * lights in the editor. The shipping client spawns the same actor locally after
 * joining MahjongRoomMap; dedicated servers never build or load this class.
 */
UCLASS()
class GUIYANGMAHJONGCLIENT_API AMahjongRoomPresentationActor final : public AActor
{
    GENERATED_BODY()

public:
    static const FName PresentationTag;

    AMahjongRoomPresentationActor();

    AMahjong3DTableActor* GetTableActor() const;
    ACameraActor* GetRoomCameraActor() const;

private:
    UPROPERTY(VisibleAnywhere, Category="Mahjong|Presentation")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, Category="Mahjong|Presentation")
    TObjectPtr<UChildActorComponent> Table;

    UPROPERTY(VisibleAnywhere, Category="Mahjong|Presentation")
    TObjectPtr<UChildActorComponent> Camera;

    UPROPERTY(VisibleAnywhere, Category="Mahjong|Lighting")
    TObjectPtr<USpotLightComponent> KeyLight;

    UPROPERTY(VisibleAnywhere, Category="Mahjong|Lighting")
    TObjectPtr<USpotLightComponent> FillLight;
};
