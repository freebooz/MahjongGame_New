#include "Game/MahjongRoomPresentationActor.h"

#include "Camera/CameraComponent.h"
#include "Components/ChildActorComponent.h"
#include "Game/Mahjong3DTableActor.h"

const FName AMahjongRoomPresentationActor::PresentationTag(TEXT("MahjongRoomPresentation"));

AMahjongRoomPresentationActor::AMahjongRoomPresentationActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SetReplicates(false);
    SetCanBeDamaged(false);
    Tags.AddUnique(PresentationTag);
}

AMahjong3DTableActor* AMahjongRoomPresentationActor::GetTableActor() const
{
    TInlineComponentArray<UChildActorComponent*> ChildActorComponents(this);
    for (const UChildActorComponent* Component : ChildActorComponents)
    {
        if (AMahjong3DTableActor* TableActor = Cast<AMahjong3DTableActor>(Component->GetChildActor()))
        {
            return TableActor;
        }
    }
    return nullptr;
}

AActor* AMahjongRoomPresentationActor::GetRoomCameraActor() const
{
    return FindComponentByClass<UCameraComponent>() ? const_cast<AMahjongRoomPresentationActor*>(this) : nullptr;
}
