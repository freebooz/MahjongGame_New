#include "Game/MahjongRoomCameraActor.h"

#include "CineCameraComponent.h"
#include "Engine/Scene.h"

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
    ConfigureStablePostProcess();
}

void AMahjongRoomCameraActor::BeginPlay()
{
    Super::BeginPlay();
    // Older placed map instances may contain serialized camera defaults. Reapply this preset at
    // runtime so eye adaptation and bloom cannot return when the client enters the room.
    ConfigureStablePostProcess();
}

void AMahjongRoomCameraActor::ConfigureStablePostProcess()
{
    UCineCameraComponent* Camera = GetCineCameraComponent();
    if (!Camera) return;

    FPostProcessSettings& Settings = Camera->PostProcessSettings;
    Settings.bOverride_AutoExposureMethod = true;
    Settings.AutoExposureMethod = AEM_Manual;
    Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    Settings.AutoExposureApplyPhysicalCameraExposure = false;
    Settings.bOverride_AutoExposureBias = true;
    Settings.AutoExposureBias = -0.7f;
    Settings.bOverride_BloomIntensity = true;
    Settings.BloomIntensity = 0.0f;
    Settings.bOverride_LensFlareIntensity = true;
    Settings.LensFlareIntensity = 0.0f;
    Settings.bOverride_MotionBlurAmount = true;
    Settings.MotionBlurAmount = 0.0f;
    Settings.bOverride_Sharpen = true;
    Settings.Sharpen = 0.5f;
    Camera->PostProcessBlendWeight = 1.0f;
}
