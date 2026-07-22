#include "Game/MahjongRoomCameraActor.h"

#include "CineCameraComponent.h"

const FName AMahjongRoomCameraActor::RoomCameraTag(TEXT("MahjongRoomCamera"));

AMahjongRoomCameraActor::AMahjongRoomCameraActor(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = false;
    Tags.AddUnique(RoomCameraTag);

    if (UCineCameraComponent* Camera = GetCineCameraComponent())
    {
        FCameraFilmbackSettings Filmback;
        Filmback.SensorWidth = 36.0f;
        Filmback.SensorHeight = 20.25f;
        Filmback.RecalcSensorAspectRatio();
        Camera->SetFilmback(Filmback);
        Camera->SetCurrentFocalLength(45.0f);
        Camera->SetConstraintAspectRatio(false);
    }
}
