#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MobileConfirmDialogWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMahjongConfirmAccepted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMahjongConfirmCancelled);

/** 通用中文确认弹窗，只产生确认/取消事件，不直接执行房间或牌局操作。 */
UCLASS(Abstract, BlueprintType)
class GUIYANGMAHJONGCLIENT_API UMobileConfirmDialogWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_Title;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_Message;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Btn_Confirm;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Btn_Cancel;
    UFUNCTION() void HandleConfirm();
    UFUNCTION() void HandleCancel();

public:
    UPROPERTY(BlueprintAssignable, Category="麻将|UI") FMahjongConfirmAccepted OnConfirmed;
    UPROPERTY(BlueprintAssignable, Category="麻将|UI") FMahjongConfirmCancelled OnCancelled;
    UFUNCTION(BlueprintCallable, Category="麻将|UI") void Configure(const FString& Title, const FString& Message);
};
