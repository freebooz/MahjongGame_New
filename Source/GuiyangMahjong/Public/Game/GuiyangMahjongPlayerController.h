#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Core/MahjongTypes.h"
#include "Auth/GuiyangLoginTypes.h"
#include "Network/MahjongNetworkTypes.h"
#include "Lobby/GuiyangLobbyTypes.h"
#include "GuiyangMahjongPlayerController.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMahjongPrivateHandUpdated, const FMahjongPrivatePlayerState&, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMahjongAvailableActionsUpdated, const TArray<FMahjongAction>&, Actions);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMahjongSettlementShown, const FMahjongSettlementResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMahjongErrorShown, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMahjongReconnectRestored, const FMahjongReconnectSnapshot&, Snapshot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMahjongFinalSettlementShown, const FMahjongFinalSettlementResult&, Result);

class IGuiyangClientControllerBridge;
class IGuiyangServerRequestHandler;

/** Shared replicated controller. Client presentation and server authority are supplied by target-specific modules. */
UCLASS()
class GUIYANGMAHJONG_API AGuiyangMahjongPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AActor* EnsureMahjongRoomPresentation();

    UFUNCTION(BlueprintCallable, Category="Mahjong|Network")
    void ConnectToAllocatedServer(const FGuiyangGameServerRoute& Route);

    UPROPERTY(BlueprintAssignable, Category="Mahjong|UI") FMahjongPrivateHandUpdated OnPrivateHandUpdated;
    UPROPERTY(BlueprintAssignable, Category="Mahjong|UI") FMahjongAvailableActionsUpdated OnAvailableActionsUpdated;
    UPROPERTY(BlueprintAssignable, Category="Mahjong|UI") FMahjongSettlementShown OnSettlementShown;
    UPROPERTY(BlueprintAssignable, Category="Mahjong|UI") FMahjongErrorShown OnErrorShown;
    UPROPERTY(BlueprintAssignable, Category="Mahjong|UI") FMahjongReconnectRestored OnReconnectRestored;
    UPROPERTY(BlueprintAssignable, Category="Mahjong|UI") FMahjongFinalSettlementShown OnFinalSettlementShown;

    UFUNCTION(BlueprintCallable, Category="Mahjong|Network")
    void ConnectToServer(const FString& ServerIP, int32 Port, const FString& PlayerName);
    UFUNCTION(BlueprintCallable, Category="Mahjong|Network") void RetryLastConnection();
    UFUNCTION(BlueprintCallable, Category="Mahjong|Network") void ReturnToConnectScreen();
    UFUNCTION(BlueprintCallable, Category="Mahjong|Network") void ReturnToLobby();
    UFUNCTION(BlueprintCallable, Category="Mahjong|Lobby")
    void RequestCreateRoomWithLoading(const FMahjongCreateRoomRequest& Request);
    UFUNCTION(BlueprintCallable, Category="Mahjong|UI") void ShowCreatingRoomLoading();
    void CompleteRemoteReturnToLobby();
    UFUNCTION(BlueprintCallable, Category="Mahjong|Table")
    void RequestTableAction(EMahjongActionType Type, int32 TargetTileId);

    UFUNCTION(Server, Reliable) void Server_RequestCreateRoom();
    UFUNCTION(Server, Reliable) void Server_RequestQuickStart();
    UFUNCTION(Server, Reliable) void Server_AuthenticateSession(const FString& PlayerId, const FString& DisplayName,
        EGuiyangLoginProvider Provider, const FString& SessionToken);
    UFUNCTION(Server, Reliable) void Server_RequestCreateRoomWithConfig(const FMahjongCreateRoomRequest& Request);
    UFUNCTION(Server, Reliable) void Server_RequestJoinRoom(const FString& PlayerName);
    UFUNCTION(Server, Reliable) void Server_RequestJoinRoomByCode(const FMahjongJoinRoomRequest& Request);
    UFUNCTION(Server, Reliable) void Server_RequestReady();
    UFUNCTION(Server, Reliable) void Server_RequestLeaveRoom();
    UFUNCTION(Server, Reliable) void Server_RequestNextRound();
    UFUNCTION(Server, Reliable) void Server_RequestPlayTile(FMahjongTile Tile);
    UFUNCTION(Server, Reliable) void Server_RequestAction(FMahjongActionRequest Request);
    UFUNCTION(Server, Reliable) void Server_RequestIntegrationDisconnect();

    UFUNCTION(Client, Reliable) void Client_UpdatePrivateHand(const FMahjongPrivatePlayerState& PrivateState);
    UFUNCTION(Client, Reliable) void Client_ShowAvailableActions(const TArray<FMahjongAction>& Actions);
    UFUNCTION(Client, Reliable) void Client_ShowSettlement(const FMahjongSettlementResult& Result);
    UFUNCTION(Client, Reliable) void Client_ShowErrorMessage(const FString& Message);
    UFUNCTION(Client, Reliable) void Client_RestoreReconnectSnapshot(
        const FMahjongReconnectSnapshot& Snapshot, const TArray<FMahjongAction>& AvailableActions);
    UFUNCTION(Client, Reliable) void Client_ShowFinalSettlement(const FMahjongFinalSettlementResult& Result);

    UFUNCTION(BlueprintPure, Category="Mahjong|Network")
    const FString& GetPendingPlayerName() const { return PendingPlayerName; }
    void SetPendingPlayerName(const FString& PlayerName) { PendingPlayerName = PlayerName; }

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(Transient) TObjectPtr<UObject> ClientBridge;
    UPROPERTY() FString PendingPlayerName;
    int32 LastClientActionSequence = -1;

    IGuiyangClientControllerBridge* GetClientBridge() const;
    IGuiyangServerRequestHandler* GetServerRequestHandler() const;
};
