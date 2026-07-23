#pragma once

#include "CoreMinimal.h"
#include "Auth/GuiyangLoginTypes.h"
#include "Core/MahjongTypes.h"
#include "Network/MahjongNetworkTypes.h"
#include "GuiyangServerRequestHandler.generated.h"

class AGuiyangMahjongPlayerController;

UINTERFACE(MinimalAPI)
class UGuiyangServerRequestHandler : public UInterface
{
    GENERATED_BODY()
};

class GUIYANGMAHJONG_API IGuiyangServerRequestHandler
{
    GENERATED_BODY()

public:
    virtual void HandleAuthenticateSession(AGuiyangMahjongPlayerController* Controller,
        const FString& PlayerId, const FString& DisplayName, EGuiyangLoginProvider Provider,
        const FString& SessionToken) = 0;
    virtual void HandleCreateRoom(AGuiyangMahjongPlayerController* Controller,
        const FMahjongCreateRoomRequest& Request) = 0;
    virtual void HandleQuickStart(AGuiyangMahjongPlayerController* Controller) = 0;
    virtual void HandleJoinRoom(AGuiyangMahjongPlayerController* Controller,
        const FMahjongJoinRoomRequest& Request) = 0;
    virtual void HandleToggleReady(AGuiyangMahjongPlayerController* Controller) = 0;
    virtual void HandleLeaveRoom(AGuiyangMahjongPlayerController* Controller) = 0;
    virtual void HandleNextRound(AGuiyangMahjongPlayerController* Controller) = 0;
    virtual void HandleLegacyPlayTile(AGuiyangMahjongPlayerController* Controller,
        const FMahjongTile& Tile, int32 ClientSequence) = 0;
    virtual void HandleTableAction(AGuiyangMahjongPlayerController* Controller,
        const FMahjongActionRequest& Request) = 0;
};
