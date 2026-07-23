#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AgonesSubsystem.h"
#include "Server/GuiyangGameServerBridge.h"
#include "GuiyangAgonesLifecycleSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(
    FGuiyangAgonesAllocationReady, const FGuiyangGameServerLaunchConfig&);

/**
 * Optional Agones lifecycle adapter. It is inert unless a dedicated server explicitly selects
 * the Agones orchestrator, so local/WSL Allocator launches retain their existing behavior.
 */
UCLASS()
class GUIYANGMAHJONGSERVER_API UGuiyangAgonesLifecycleSubsystem final : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    static bool IsAgonesRequested(const TCHAR* CommandLine, const FString& EnvironmentValue);
    static bool TryBuildLaunchConfig(const FGameServerResponse& Response,
        const FString& SigningKey, const FString& MatchResultOutboxPath,
        FGuiyangGameServerLaunchConfig& OutConfig, FString& OutError);

    bool IsActive() const { return bActive; }
    bool IsReady() const { return bReady; }
    void NotifyPlayerConnected(const FString& PlayerId);
    void NotifyPlayerDisconnected(const FString& PlayerId);
    void RequestShutdown();
    bool TryGetAllocationConfig(FGuiyangGameServerLaunchConfig& OutConfig) const;
    FGuiyangAgonesAllocationReady& OnAllocationReady() { return AllocationReady; }

private:
    UFUNCTION() void HandleConnected(const FGameServerResponse& Response);
    UFUNCTION() void HandleGameServerUpdated(const FGameServerResponse& Response);
    UFUNCTION() void HandleError(const FAgonesError& Error);
    UFUNCTION() void HandleEmptySuccess(const FEmptyResponse& Response);
    UFUNCTION() void HandlePlayerConnected(const FConnectedResponse& Response);
    UFUNCTION() void HandlePlayerDisconnected(const FDisconnectResponse& Response);

    UPROPERTY(Transient) TObjectPtr<UAgonesSubsystem> Agones;
    FGuiyangAgonesAllocationReady AllocationReady;
    TOptional<FGuiyangGameServerLaunchConfig> AllocationConfig;
    bool bActive = false;
    bool bReady = false;
    bool bShutdownRequested = false;
};
