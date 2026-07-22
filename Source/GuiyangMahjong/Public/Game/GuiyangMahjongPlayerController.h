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

class UMobileRootHUDWidget;
class ACameraActor;
class AMahjong3DTableActor;

/**
 * 客户端输入与 UI 生命周期入口。UI 只能调用本类请求函数，所有牌局修改均由 Server RPC 校验后执行。
 */
UCLASS()
class GUIYANGMAHJONG_API AGuiyangMahjongPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    /** Finds the editor-authored room table/camera, spawning a safe runtime fallback when absent. */
    AMahjong3DTableActor* EnsureMahjongRoomPresentation();

    UFUNCTION(BlueprintCallable, Category="Mahjong|Network")
    void ConnectToAllocatedServer(const FGuiyangGameServerRoute& Route);

    UPROPERTY(BlueprintAssignable, Category="麻将|UI") FMahjongPrivateHandUpdated OnPrivateHandUpdated;
    UPROPERTY(BlueprintAssignable, Category="麻将|UI") FMahjongAvailableActionsUpdated OnAvailableActionsUpdated;
    UPROPERTY(BlueprintAssignable, Category="麻将|UI") FMahjongSettlementShown OnSettlementShown;
    UPROPERTY(BlueprintAssignable, Category="麻将|UI") FMahjongErrorShown OnErrorShown;
    UPROPERTY(BlueprintAssignable, Category="麻将|UI") FMahjongReconnectRestored OnReconnectRestored;
    UPROPERTY(BlueprintAssignable, Category="麻将|UI") FMahjongFinalSettlementShown OnFinalSettlementShown;

    /** 校验地址后执行 ClientTravel；不直接修改房间或牌局状态。 */
    UFUNCTION(BlueprintCallable, Category="麻将|网络") void ConnectToServer(const FString& ServerIP, int32 Port, const FString& PlayerName);
    UFUNCTION(BlueprintCallable, Category="麻将|网络") void RetryLastConnection();
    UFUNCTION(BlueprintCallable, Category="麻将|网络") void ReturnToConnectScreen();
    UFUNCTION(BlueprintCallable, Category="麻将|网络") void ReturnToLobby();
    UFUNCTION(BlueprintCallable, Category="麻将|UI") void ShowCreatingRoomLoading();
    void CompleteRemoteReturnToLobby();
    UFUNCTION(BlueprintCallable, Category="麻将|牌桌") void RequestTableAction(EMahjongActionType Type, int32 TargetTileId);

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
    /** 仅在非 Shipping 且服务端显式启用集成钩子时关闭本测试连接。 */
    UFUNCTION(Server, Reliable) void Server_RequestIntegrationDisconnect();

    UFUNCTION(Client, Reliable) void Client_UpdatePrivateHand(const FMahjongPrivatePlayerState& PrivateState);
    UFUNCTION(Client, Reliable) void Client_ShowAvailableActions(const TArray<FMahjongAction>& Actions);
    UFUNCTION(Client, Reliable) void Client_ShowSettlement(const FMahjongSettlementResult& Result);
    UFUNCTION(Client, Reliable) void Client_ShowErrorMessage(const FString& Message);
    UFUNCTION(Client, Reliable) void Client_RestoreReconnectSnapshot(
        const FMahjongReconnectSnapshot& Snapshot, const TArray<FMahjongAction>& AvailableActions);
    UFUNCTION(Client, Reliable) void Client_ShowFinalSettlement(const FMahjongFinalSettlementResult& Result);

    UFUNCTION(BlueprintPure, Category="麻将|网络") const FString& GetPendingPlayerName() const { return PendingPlayerName; }

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(Transient) TObjectPtr<UMobileRootHUDWidget> RootHUDInstance;
    UPROPERTY(Transient) TObjectPtr<AMahjong3DTableActor> RoomTableActor;
    UPROPERTY(Transient) TObjectPtr<ACameraActor> RoomCameraActor;
    UPROPERTY() FString PendingPlayerName;
    int32 LastClientActionSequence = -1;
    int32 IntegrationClientIndex = INDEX_NONE;
    bool bIntegrationQuickStartRequested = false;
    bool bIntegrationReadyRequested = false;
    bool bIntegrationRetryRequested = false;
    double IntegrationReconnectObservedAtSeconds = 0.0;
    double CreatingRoomLoadingShownAtSeconds = 0.0;
    UPROPERTY(Transient) FGuiyangGameServerRoute PendingAllocatedRoute;
    FTimerHandle IntegrationPollTimer;
    FTimerHandle CreatingRoomTravelDelayTimer;

    void CompleteDelayedAllocatedServerConnection();
    void TravelToAllocatedServer(FGuiyangGameServerRoute Route);
    void InitializeIntegrationClient();
    void PollIntegrationClient();
    void HandleIntegrationPrivateState(const FMahjongPrivatePlayerState& PrivateState);
};
