#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MobileErrorToastWidget.generated.h"

class UBorder; class UTextBlock;

/** 中文错误 Toast。重复提示会刷新文字并重新开始两秒计时。 */
UCLASS(Abstract, BlueprintType)
class GUIYANGMAHJONG_API UMobileErrorToastWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UBorder> Border_Toast;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_Message;
    FTimerHandle HideTimer;
    UFUNCTION() void HideToast();
public:
    UFUNCTION(BlueprintCallable, Category="麻将|UI") void ShowToast(const FString& Message, float DurationSeconds = 2.0f);
};
