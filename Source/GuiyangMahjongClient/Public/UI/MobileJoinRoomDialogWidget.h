#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MobileJoinRoomDialogWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;

/** 加入房间弹窗。房间号和可选密码只进入 Client->Server 请求，不写入公共状态。 */
UCLASS(Abstract, BlueprintType)
class GUIYANGMAHJONGCLIENT_API UMobileJoinRoomDialogWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta=(BindWidget)) TObjectPtr<UEditableTextBox> Txt_RoomCode;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UEditableTextBox> Txt_Password;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_Status;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Btn_Join;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Btn_Cancel;

    UFUNCTION() void HandleJoin();
    UFUNCTION() void HandleCancel();
};
