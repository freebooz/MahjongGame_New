#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MahjongRoomPresentationActor.generated.h"

class AMahjong3DTableActor;

/**
 * Client-only visual root for the network-neutral MahjongRoomMap.
 *
 * The native class deliberately owns no scene components. BP_MahjongRoomPresentation
 * owns the complete designer-editable component tree (table, camera and lights).
 * The shipping client spawns that Blueprint locally after joining MahjongRoomMap;
 * dedicated servers never build or load this class.
 */
UCLASS(Blueprintable)
class GUIYANGMAHJONGCLIENT_API AMahjongRoomPresentationActor : public AActor
{
    GENERATED_BODY()

public:
    static const FName PresentationTag;

    AMahjongRoomPresentationActor();

    AMahjong3DTableActor* GetTableActor() const;
    AActor* GetRoomCameraActor() const;
};
