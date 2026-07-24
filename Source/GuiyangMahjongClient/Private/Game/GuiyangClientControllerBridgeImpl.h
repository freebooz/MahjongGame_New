#pragma once

#include "CoreMinimal.h"
#include "Game/GuiyangClientControllerBridge.h"
#include "GuiyangClientControllerBridgeImpl.generated.h"

class ACameraActor;
class AGuiyangMahjongPlayerController;
class AMahjong3DTableActor;
class AMahjongRoomPresentationActor;
struct FStreamableHandle;
class UMobileRootHUDWidget;

UCLASS(Transient)
class UGuiyangClientControllerBridgeImpl final : public UObject, public IGuiyangClientControllerBridge
{
    GENERATED_BODY()

public:
    virtual void BeginDestroy() override;
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
    UPROPERTY(Transient) TObjectPtr<AMahjongRoomPresentationActor> RoomPresentationActor;
    UPROPERTY(Transient) TObjectPtr<AMahjong3DTableActor> RoomTableActor;
    UPROPERTY(Transient) TObjectPtr<AActor> RoomCameraActor;
    UPROPERTY(Transient) FGuiyangGameServerRoute PendingAllocatedRoute;
    FTimerHandle CreatingRoomTravelDelayTimer;
    TSharedPtr<FStreamableHandle> PresentationLoadHandle;
    double CreatingRoomLoadingShownAtSeconds = 0.0;
    bool bPresentationLoadFailed = false;

    void RequestRoomPresentationClassLoad();
    void HandleRoomPresentationClassLoaded();
    AMahjongRoomPresentationActor* SpawnRoomPresentation(UClass& PresentationClass);
    void ApplyRoomPresentationViewTarget();
    void CompleteDelayedAllocatedServerConnection();
    void TravelToAllocatedServer(FGuiyangGameServerRoute Route);
};
