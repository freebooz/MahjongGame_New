#pragma once

#include "CoreMinimal.h"
#include "Game/GuiyangClientControllerBridge.h"
#include "GuiyangClientControllerBridgeImpl.generated.h"

class ACameraActor;
class AGuiyangMahjongPlayerController;
class AMahjong3DTableActor;
class UMobileRootHUDWidget;

UCLASS(Transient)
class UGuiyangClientControllerBridgeImpl final : public UObject, public IGuiyangClientControllerBridge
{
    GENERATED_BODY()

public:
    virtual UWorld* GetWorld() const override;
    virtual void InitializeClient(AGuiyangMahjongPlayerController& InController) override;
    virtual AActor* EnsureRoomPresentation() override;
    virtual void ConnectToServer(const FString& ServerIP, int32 Port, const FString& PlayerName) override;
    virtual void ConnectToAllocatedServer(const FGuiyangGameServerRoute& Route) override;
    virtual void RetryLastConnection() override;
    virtual void ReturnToConnectScreen() override;
    virtual void ReturnToLobby() override;
    virtual void ShowCreatingRoomLoading() override;
    virtual void RequestCreateRoomWithLoading(const FMahjongCreateRoomRequest& Request) override;
    virtual void CompleteRemoteReturnToLobby() override;
    virtual void NotifyReconnectRestored(const FMahjongReconnectSnapshot& Snapshot) override;
    virtual void NotifyFinalSettlement(const FMahjongFinalSettlementResult& Result) override;
    virtual void HandleIntegrationPrivateState(const FMahjongPrivatePlayerState& PrivateState) override;

private:
    UPROPERTY(Transient) TObjectPtr<AGuiyangMahjongPlayerController> Controller;
    UPROPERTY(Transient) TObjectPtr<UMobileRootHUDWidget> RootHUDInstance;
    UPROPERTY(Transient) TObjectPtr<AMahjong3DTableActor> RoomTableActor;
    UPROPERTY(Transient) TObjectPtr<ACameraActor> RoomCameraActor;
    UPROPERTY(Transient) FGuiyangGameServerRoute PendingAllocatedRoute;
    FTimerHandle CreatingRoomTravelDelayTimer;
    double CreatingRoomLoadingShownAtSeconds = 0.0;

    void CompleteDelayedAllocatedServerConnection();
    void TravelToAllocatedServer(FGuiyangGameServerRoute Route);
};
