#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MobileConnectServerWidget.generated.h"

class UButton; class UCheckBox; class UEditableTextBox; class UTextBlock;

/** 服务器连接页 C++ 基类。动态文本全部由 TextBlock/EditableTextBox 渲染。 */
UCLASS(Abstract, BlueprintType)
class GUIYANGMAHJONGCLIENT_API UMobileConnectServerWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual void NativeConstruct() override;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UEditableTextBox> Txt_ServerIP;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UEditableTextBox> Txt_ServerPort;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UEditableTextBox> Txt_PlayerName;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UCheckBox> Chk_RememberAddress;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Btn_Connect;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_ConnectButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_Version;
    UFUNCTION() void HandleConnectClicked();
public:
    UFUNCTION(BlueprintCallable, Category="麻将|UI") void SetConnecting(bool bConnecting);
};
