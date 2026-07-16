#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Auth/GuiyangLoginTypes.h"
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

    UFUNCTION() void HandleLoginStateChanged(EGuiyangLoginState State, const FGuiyangLoginProfile& Profile);
    UFUNCTION() void HandleLoginFailed(const FString& ChineseReason);

public:
    UFUNCTION(BlueprintCallable, Category="麻将|UI") void ShowLogin();
    UFUNCTION(BlueprintCallable, Category="麻将|UI") void ShowLobby();
    UFUNCTION(BlueprintCallable, Category="麻将|UI") void ShowChineseError(const FString& ChineseReason);

private:
    void ShowScreenByClassPath(const TCHAR* ClassPath);
};
