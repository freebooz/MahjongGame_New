#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/MahjongTypes.h"
#include "MobileCreateRoomDialogWidget.generated.h"

class UButton;
class UCheckBox;
class UEditableTextBox;
class UTextBlock;
class UMobileRuleConfigWidget;
class UMobileRuleSummaryWidget;

/** 创建房间弹窗。收集基础房间参数，并通过 PlayerController 提交权威创建请求。 */
UCLASS(Abstract, BlueprintType)
class GUIYANGMAHJONGCLIENT_API UMobileCreateRoomDialogWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta=(BindWidget)) TObjectPtr<UEditableTextBox> Txt_RoundCount;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UCheckBox> Chk_PublicRoom;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UCheckBox> Chk_EnablePassword;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UEditableTextBox> Txt_Password;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_Status;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UMobileRuleConfigWidget> RuleConfig;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UMobileRuleSummaryWidget> RuleSummary;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Btn_Create;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Btn_Cancel;

    UFUNCTION() void HandleCreate();
    UFUNCTION() void HandleCancel();
    UFUNCTION() void HandleRuleConfigChanged(FMahjongRuleConfig Config);
    UFUNCTION() void HandleOptionCheckChanged(bool bChecked);
    UFUNCTION() void HandleOptionTextChanged(const FText& Text);

private:
    void RefreshRuleSummary();
};
