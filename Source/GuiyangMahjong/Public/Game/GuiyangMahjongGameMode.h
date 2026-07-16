#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Auth/GuiyangLoginTypes.h"
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
    void HandleAuthenticateSession(class AGuiyangMahjongPlayerController* Controller, const FString& PlayerId,
        const FString& DisplayName, EGuiyangLoginProvider Provider, const FString& SessionToken);
    void HandleJoinRoom(class AGuiyangMahjongPlayerController* Controller, const FMahjongJoinRoomRequest& Request);
    void HandleToggleReady(class AGuiyangMahjongPlayerController* Controller);
    void HandleNextRound(class AGuiyangMahjongPlayerController* Controller);
    void HandleLeaveRoom(class AGuiyangMahjongPlayerController* Controller);
    void HandleTableAction(class AGuiyangMahjongPlayerController* Controller, const FMahjongActionRequest& Request);
    void HandleLegacyPlayTile(class AGuiyangMahjongPlayerController* Controller, const FMahjongTile& Tile, int32 ClientSequence);

private:
    UPROPERTY(Transient) TObjectPtr<class UGuiyangRoomManager> RoomManager;
    UPROPERTY(Transient) TObjectPtr<class UMahjongTableEngine> TableEngine;
    int32 LastPublishedSettlementSequence = INDEX_NONE;
    int32 LastFinalizedSettlementSequence = INDEX_NONE;
    FString ActiveRoomCode;
    TMap<FString, FString> SessionTokenDigestsByPlayer;
    FTimerHandle ActionTimeoutHandle;
    int32 ArmedTimeoutRoundId = INDEX_NONE;
    int32 ArmedTimeoutTurnId = INDEX_NONE;
    EMahjongTablePhase ArmedTimeoutPhase = EMahjongTablePhase::WaitingForPlayers;
    bool ResolvePlayer(class AGuiyangMahjongPlayerController* Controller, class AGuiyangMahjongPlayerState*& OutPlayerState) const;
    void PublishRoomState(const FMahjongRoomState& State);
    void TryStartTable(const FMahjongRoomState& StartingRoomState);
    void PublishTableSnapshots();
    void FinalizeRoundIfNeeded();
    void RefreshActionTimeoutTimer();
    void HandleActionTimeout(int32 ExpectedRoundId, int32 ExpectedTurnId, EMahjongTablePhase ExpectedPhase);
    void PublishReconnectSnapshot(class AGuiyangMahjongPlayerController* Controller,
        const FMahjongRoomState& RoomState, int32 RemainingReconnectSeconds);
    static FString HashSessionToken(const FString& SessionToken);
    static bool ConstantTimeDigestEquals(const FString& Left, const FString& Right);
    static FString ErrorToMessage(EMahjongRoomError Error);
};
