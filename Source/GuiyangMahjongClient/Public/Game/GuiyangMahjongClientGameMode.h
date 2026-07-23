#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GuiyangMahjongClientGameMode.generated.h"

/**
 * Lightweight standalone bootstrap GameMode used only by client targets.
 *
 * The engine Entry map has no project-specific WorldSettings override, so it
 * needs this class to create the shared Mahjong PlayerController and start the
 * client UI bridge. Dedicated servers use their own server-only GameMode.
 */
UCLASS(Config=Game)
class GUIYANGMAHJONGCLIENT_API AGuiyangMahjongClientGameMode final : public AGameModeBase
{
    GENERATED_BODY()

public:
    AGuiyangMahjongClientGameMode();
};
