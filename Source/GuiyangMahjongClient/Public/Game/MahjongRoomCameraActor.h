#pragma once

#include "CoreMinimal.h"
#include "CineCameraActor.h"
#include "MahjongRoomCameraActor.generated.h"

/**
 * Editor-adjustable camera preset for the real Mahjong room level.
 * Place one actor in MahjongRoomMap, pilot it in the editor, and tune its CineCameraComponent.
 */
UCLASS(Blueprintable)
class GUIYANGMAHJONGCLIENT_API AMahjongRoomCameraActor : public ACineCameraActor
{
    GENERATED_BODY()

public:
    AMahjongRoomCameraActor(const FObjectInitializer& ObjectInitializer);

    static const FName RoomCameraTag;

protected:
    virtual void BeginPlay() override;

private:
    void ConfigureStablePostProcess();
};
