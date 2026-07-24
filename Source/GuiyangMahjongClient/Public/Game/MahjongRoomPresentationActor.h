#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MahjongRoomPresentationActor.generated.h"

class ACameraActor;
class AMahjong3DTableActor;
class UChildActorComponent;
class UDirectionalLightComponent;
class USceneComponent;
class USkyLightComponent;
class USpotLightComponent;

/**
 * Client-only visual root for the network-neutral MahjongRoomMap.
 *
 * Place this actor in MahjongRoomVisualPreviewMap to tune the table, camera and
 * lights in the editor. The shipping client spawns the same actor locally after
 * joining MahjongRoomMap; dedicated servers never build or load this class.
 */
UCLASS(Blueprintable)
class GUIYANGMAHJONGCLIENT_API AMahjongRoomPresentationActor : public AActor
{
    GENERATED_BODY()

public:
    static const FName PresentationTag;

    AMahjongRoomPresentationActor();

    AMahjong3DTableActor* GetTableActor() const;
    ACameraActor* GetRoomCameraActor() const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mahjong|Presentation")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mahjong|Presentation")
    TObjectPtr<UChildActorComponent> Table;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mahjong|Presentation")
    TObjectPtr<UChildActorComponent> Camera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mahjong|Lighting")
    TObjectPtr<UDirectionalLightComponent> DirectionalLight;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mahjong|Lighting")
    TObjectPtr<USkyLightComponent> SkyLight;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mahjong|Lighting")
    TObjectPtr<USpotLightComponent> KeyLight;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mahjong|Lighting")
    TObjectPtr<USpotLightComponent> FillLight;
};
