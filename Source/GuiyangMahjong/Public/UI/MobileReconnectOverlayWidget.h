#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MobileReconnectOverlayWidget.generated.h"

class UButton; class UTextBlock;

/** 断线重连遮罩。仅发起重连/返回请求，快照恢复由 PlayerController 与 GameState 完成。 */
UCLASS(Abstract, BlueprintType)
class GUIYANGMAHJONG_API UMobileReconnectOverlayWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual void NativeConstruct() override;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_ReconnectStatus;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_RemainingTime;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Btn_Reconnect;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Btn_BackConnect;
    UFUNCTION() void HandleReconnect(); UFUNCTION() void HandleBackConnect();
public:
    UFUNCTION(BlueprintCallable, Category="麻将|UI") void RefreshReconnectState(const FString& Status, int32 RemainingSeconds, bool bCanRetry);
};
