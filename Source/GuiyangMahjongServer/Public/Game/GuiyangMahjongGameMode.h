#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Game/GuiyangServerRequestHandler.h"
#include "Auth/GuiyangLoginTypes.h"
#include "Network/MahjongNetworkTypes.h"
#include "Server/GuiyangGameServerBridge.h"
#include "GuiyangMahjongGameMode.generated.h"

struct FGuiyangManagedRoomDefinition;

/** Dedicated Server 权威入口；为牌桌复制指定 GameState 和玩家请求入口。 */
UCLASS()
class GUIYANGMAHJONGSERVER_API AGuiyangMahjongGameMode : public AGameModeBase, public IGuiyangServerRequestHandler
{
    GENERATED_BODY()

public:
    AGuiyangMahjongGameMode();
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    virtual void BeginPlay() override;
    virtual void PreLogin(const FString& Options, const FString& Address,
        const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
    virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
        const FString& Options, const FString& Portal = TEXT("")) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    bool InitializeManagedRoomAuthority(const FGuiyangManagedRoomDefinition& Definition, FString& OutError);

    virtual void HandleCreateRoom(class AGuiyangMahjongPlayerController* Controller, const FMahjongCreateRoomRequest& Request) override;
    virtual void HandleQuickStart(class AGuiyangMahjongPlayerController* Controller) override;
    void HandleAuthenticateSession(class AGuiyangMahjongPlayerController* Controller, const FString& PlayerId,
        const FString& DisplayName, EGuiyangLoginProvider Provider, const FString& SessionToken) override;
    virtual void HandleJoinRoom(class AGuiyangMahjongPlayerController* Controller, const FMahjongJoinRoomRequest& Request) override;
    virtual void HandleToggleReady(class AGuiyangMahjongPlayerController* Controller) override;
    virtual void HandleNextRound(class AGuiyangMahjongPlayerController* Controller) override;
    virtual void HandleLeaveRoom(class AGuiyangMahjongPlayerController* Controller) override;
    virtual void HandleTableAction(class AGuiyangMahjongPlayerController* Controller, const FMahjongActionRequest& Request) override;
    virtual void HandleLegacyPlayTile(class AGuiyangMahjongPlayerController* Controller, const FMahjongTile& Tile, int32 ClientSequence) override;

private:
    UPROPERTY(Transient) TObjectPtr<class UGuiyangRoomManager> RoomManager;
    UPROPERTY(Transient) TObjectPtr<class UMahjongTableEngine> TableEngine;
    UPROPERTY(Transient) TObjectPtr<class UGuiyangGameServerBridge> GameServerBridge;
    int32 LastPublishedSettlementSequence = INDEX_NONE;
    int32 LastFinalizedSettlementSequence = INDEX_NONE;
    int32 LastPublishedFinalRoomSequence = INDEX_NONE;
    FString ActiveRoomCode;
    FString ManagedRoomCode;
    TMap<FString, FString> SessionTokenDigestsByPlayer;
    TMap<FString, FString> PendingAuthorizedPlayersByTicketDigest;
    TMap<FString, int64> PendingTicketExpiryByDigest;
    TMap<TObjectPtr<APlayerController>, FString> AuthorizedPlayerIdsByController;
    bool bManagedGameServer = false;
    bool bAgonesGameServer = false;
    bool bManagedWorldReady = false;
    bool bHasPendingManagedConfig = false;
    FGuiyangGameServerLaunchConfig PendingManagedConfig;
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
    void PublishFinalSettlement(const FMahjongRoomState& RoomState);
    static FString HashSessionToken(const FString& SessionToken);
    static FString HashJoinTicket(const FString& JoinTicket);
    static bool ConstantTimeDigestEquals(const FString& Left, const FString& Right);
    static FString ErrorToMessage(EMahjongRoomError Error);
    void InitializeManagedBridge(const FGuiyangGameServerLaunchConfig& Config);
    void TryInitializeManagedBridgeAfterListen();
    void HandleAgonesAllocationReady(const FGuiyangGameServerLaunchConfig& Config);
};
