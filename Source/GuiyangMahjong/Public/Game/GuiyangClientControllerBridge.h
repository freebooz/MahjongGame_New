#pragma once

#include "CoreMinimal.h"
#include "Lobby/GuiyangLobbyTypes.h"
#include "Network/MahjongNetworkTypes.h"
#include "GuiyangClientControllerBridge.generated.h"

class AGuiyangMahjongPlayerController;

UINTERFACE(MinimalAPI)
class UGuiyangClientControllerBridge : public UInterface
{
    GENERATED_BODY()
};

class GUIYANGMAHJONG_API IGuiyangClientControllerBridge
{
    GENERATED_BODY()

public:
    virtual void InitializeClient(AGuiyangMahjongPlayerController& Controller) = 0;
    virtual AActor* EnsureRoomPresentation() = 0;
    virtual void ConnectToServer(const FString& ServerIP, int32 Port, const FString& PlayerName) = 0;
    virtual void ConnectToAllocatedServer(const FGuiyangGameServerRoute& Route) = 0;
    virtual void RetryLastConnection() = 0;
    virtual void ReturnToConnectScreen() = 0;
    virtual void ReturnToLobby() = 0;
    virtual void ShowCreatingRoomLoading() = 0;
    virtual void RequestCreateRoomWithLoading(const FMahjongCreateRoomRequest& Request) = 0;
    virtual void CompleteRemoteReturnToLobby() = 0;
    virtual void NotifyReconnectRestored(const FMahjongReconnectSnapshot& Snapshot) = 0;
    virtual void NotifyFinalSettlement(const FMahjongFinalSettlementResult& Result) = 0;
    virtual void HandleIntegrationPrivateState(const FMahjongPrivatePlayerState& PrivateState) = 0;
};

using FGuiyangClientBridgeFactory = TFunction<UObject*(AGuiyangMahjongPlayerController&)>;

class GUIYANGMAHJONG_API FGuiyangClientBridgeRegistry
{
public:
    static void Register(FGuiyangClientBridgeFactory Factory);
    static void Unregister();
    static UObject* Create(AGuiyangMahjongPlayerController& Controller);
};
