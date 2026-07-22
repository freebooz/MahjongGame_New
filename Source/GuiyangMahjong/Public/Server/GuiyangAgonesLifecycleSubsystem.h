#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AgonesSubsystem.h"
#include "GuiyangAgonesLifecycleSubsystem.generated.h"

/**
 * Optional Agones lifecycle adapter. It is inert unless a dedicated server explicitly selects
 * the Agones orchestrator, so local/WSL Allocator launches retain their existing behavior.
 */
UCLASS()
class GUIYANGMAHJONG_API UGuiyangAgonesLifecycleSubsystem final : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    static bool IsAgonesRequested(const TCHAR* CommandLine, const FString& EnvironmentValue);

    bool IsActive() const { return bActive; }
    bool IsReady() const { return bReady; }
    void NotifyPlayerConnected(const FString& PlayerId);
    void NotifyPlayerDisconnected(const FString& PlayerId);
    void RequestShutdown();

private:
    UFUNCTION() void HandleConnected(const FGameServerResponse& Response);
    UFUNCTION() void HandleError(const FAgonesError& Error);
    UFUNCTION() void HandleEmptySuccess(const FEmptyResponse& Response);
    UFUNCTION() void HandlePlayerConnected(const FConnectedResponse& Response);
    UFUNCTION() void HandlePlayerDisconnected(const FDisconnectResponse& Response);

    UPROPERTY(Transient) TObjectPtr<UAgonesSubsystem> Agones;
    bool bActive = false;
    bool bReady = false;
    bool bShutdownRequested = false;
};

