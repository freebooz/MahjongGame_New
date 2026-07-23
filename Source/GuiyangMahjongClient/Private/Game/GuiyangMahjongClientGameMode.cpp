#include "Game/GuiyangMahjongClientGameMode.h"

#include "Game/GuiyangMahjongPlayerController.h"

AGuiyangMahjongClientGameMode::AGuiyangMahjongClientGameMode()
{
    PlayerControllerClass = AGuiyangMahjongPlayerController::StaticClass();
    DefaultPawnClass = nullptr;
    HUDClass = nullptr;
    bStartPlayersAsSpectators = true;
}
