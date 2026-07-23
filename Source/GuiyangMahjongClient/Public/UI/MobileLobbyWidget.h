#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MobileLobbyWidget.generated.h"

class UButton; class UTextBlock;
class UMobileCreateRoomDialogWidget;
class UMobileJoinRoomDialogWidget;
class UMobileSettingsWidget;

/** 大厅页 C++ 基类。只发起房间请求，不保存或修改权威房间状态。 */
UCLASS(Abstract, BlueprintType)
class GUIYANGMAHJONGCLIENT_API UMobileLobbyWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Btn_QuickStart;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Btn_CreateRoom;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Btn_JoinRoom;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Btn_Setting;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_PlayerName;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_PlayerId;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_OnlineCount;
    UPROPERTY(Transient) TObjectPtr<UMobileCreateRoomDialogWidget> CreateRoomDialogInstance;
    UPROPERTY(Transient) TObjectPtr<UMobileJoinRoomDialogWidget> JoinRoomDialogInstance;
    UPROPERTY(Transient) TObjectPtr<UMobileSettingsWidget> SettingsDialogInstance;
    FTimerHandle PresenceRefreshTimer;
    UFUNCTION() void HandleQuickStart();
    UFUNCTION() void HandleCreateRoom();
    UFUNCTION() void HandleJoinRoom();
    UFUNCTION() void HandleSetting();
    bool EnsureCreateRoomDialog();
    void RefreshOnlinePresence();
public:
    UFUNCTION(BlueprintCallable, Category="麻将|UI") void RefreshPlayerInfo(const FString& PlayerName, const FString& PlayerId, int32 OnlineCount);
};
