#include "Game/MahjongRoomPresentationActor.h"

#include "Camera/CameraActor.h"
#include "Components/ChildActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SpotLightComponent.h"
#include "Game/Mahjong3DTableActor.h"
#include "Game/MahjongRoomCameraActor.h"

const FName AMahjongRoomPresentationActor::PresentationTag(TEXT("MahjongRoomPresentation"));

AMahjongRoomPresentationActor::AMahjongRoomPresentationActor()
{
    PrimaryActorTick.bCanEverTick = false;
    Tags.AddUnique(PresentationTag);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    Table = CreateDefaultSubobject<UChildActorComponent>(TEXT("MahjongTable"));
    Table->SetupAttachment(SceneRoot);
    Table->SetChildActorClass(AMahjong3DTableActor::StaticClass());

    Camera = CreateDefaultSubobject<UChildActorComponent>(TEXT("MahjongCamera"));
    Camera->SetupAttachment(SceneRoot);
    Camera->SetChildActorClass(AMahjongRoomCameraActor::StaticClass());
    Camera->SetRelativeLocation(FVector(0.0f, -950.0f, 1320.0f));
    Camera->SetRelativeRotation(FRotator(-54.25f, 90.0f, 0.0f));

    KeyLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("StableKeyLight"));
    KeyLight->SetupAttachment(SceneRoot);
    KeyLight->SetRelativeLocation(FVector(0.0f, 0.0f, 1200.0f));
    KeyLight->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
    KeyLight->SetIntensity(800.0f);
    KeyLight->SetAttenuationRadius(3000.0f);
    KeyLight->SetInnerConeAngle(40.0f);
    KeyLight->SetOuterConeAngle(65.0f);
    KeyLight->SetLightColor(FLinearColor(1.0f, 0.96f, 0.88f));
    KeyLight->SetCastShadows(false);
    KeyLight->SetMobility(EComponentMobility::Movable);

    FillLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("StableFillLight"));
    FillLight->SetupAttachment(SceneRoot);
    FillLight->SetRelativeLocation(FVector(0.0f, -650.0f, 720.0f));
    FillLight->SetRelativeRotation(FRotator(-48.0f, 90.0f, 0.0f));
    FillLight->SetIntensity(260.0f);
    FillLight->SetAttenuationRadius(2200.0f);
    FillLight->SetInnerConeAngle(45.0f);
    FillLight->SetOuterConeAngle(75.0f);
    FillLight->SetLightColor(FLinearColor(0.82f, 0.9f, 1.0f));
    FillLight->SetCastShadows(false);
    FillLight->SetMobility(EComponentMobility::Movable);
}

AMahjong3DTableActor* AMahjongRoomPresentationActor::GetTableActor() const
{
    return Table ? Cast<AMahjong3DTableActor>(Table->GetChildActor()) : nullptr;
}

ACameraActor* AMahjongRoomPresentationActor::GetRoomCameraActor() const
{
    return Camera ? Cast<ACameraActor>(Camera->GetChildActor()) : nullptr;
}
