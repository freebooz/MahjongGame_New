#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Auth/GuiyangLoginTypes.h"
#include "Network/MahjongNetworkTypes.h"
#include "MobileRootHUDWidget.generated.h"

class UMobileErrorToastWidget;
class UOverlay;

/** 全局 UI 路由层，负责页面切换和弹层，不持有任何牌局权威状态。 */
UCLASS(Abstract, BlueprintType)
class GUIYANGMAHJONG_API UMobileRootHUDWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(meta=(BindWidget)) TObjectPtr<UOverlay> ScreenLayer;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UOverlay> PopupLayer;
    UPROPERTY(Transient) TObjectPtr<UUserWidget> CurrentScreen;
    UPROPERTY(Transient) TObjectPtr<UMobileErrorToastWidget> ErrorToastInstance;
    FString CurrentScreenClassPath;

    UFUNCTION() void HandleLoginStateChanged(EGuiyangLoginState State, const FGuiyangLoginProfile& Profile);
    UFUNCTION() void HandleLoginFailed(const FString& ChineseReason);
    UFUNCTION() void HandleRoomStateUpdated(const FMahjongRoomState& State);
    UFUNCTION() void HandleReconnectRestored(const FMahjongReconnectSnapshot& Snapshot);

public:
    UFUNCTION(BlueprintCallable, Category="麻将|UI") void ShowLogin();
    UFUNCTION(BlueprintCallable, Category="麻将|UI") void ShowLobby();
    UFUNCTION(BlueprintCallable, Category="麻将|UI") void ShowRoom(const FMahjongRoomState& State, int32 LocalSeat);
    UFUNCTION(BlueprintCallable, Category="麻将|UI") void ShowGameHUD();
    UFUNCTION(BlueprintCallable, Category="麻将|UI") void ShowChineseError(const FString& ChineseReason);

private:
    UUserWidget* ShowScreenByClassPath(const TCHAR* ClassPath);
    void RouteFromRoomState(const FMahjongRoomState& State);
    int32 FindLocalSeat(const FMahjongRoomState& State) const;
};
