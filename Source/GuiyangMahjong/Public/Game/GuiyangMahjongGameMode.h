#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Network/MahjongNetworkTypes.h"
#include "GuiyangMahjongGameMode.generated.h"

/** Dedicated Server 权威入口；为牌桌复制指定 GameState 和玩家请求入口。 */
UCLASS()
class GUIYANGMAHJONG_API AGuiyangMahjongGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AGuiyangMahjongGameMode();
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;

    void HandleCreateRoom(class AGuiyangMahjongPlayerController* Controller, const FMahjongCreateRoomRequest& Request);
    void HandleJoinRoom(class AGuiyangMahjongPlayerController* Controller, const FMahjongJoinRoomRequest& Request);
    void HandleToggleReady(class AGuiyangMahjongPlayerController* Controller);
    void HandleLeaveRoom(class AGuiyangMahjongPlayerController* Controller);
    void HandleTableAction(class AGuiyangMahjongPlayerController* Controller, const FMahjongActionRequest& Request);
    void HandleLegacyPlayTile(class AGuiyangMahjongPlayerController* Controller, const FMahjongTile& Tile, int32 ClientSequence);

private:
    UPROPERTY(Transient) TObjectPtr<class UGuiyangRoomManager> RoomManager;
    UPROPERTY(Transient) TObjectPtr<class UMahjongTableEngine> TableEngine;
    int32 LastPublishedSettlementSequence = INDEX_NONE;
    bool ResolvePlayer(class AGuiyangMahjongPlayerController* Controller, class AGuiyangMahjongPlayerState*& OutPlayerState) const;
    void PublishRoomState(const FMahjongRoomState& State);
    void TryStartTable(const FMahjongRoomState& StartingRoomState);
    void PublishTableSnapshots();
    static FString ErrorToMessage(EMahjongRoomError Error);
};
