#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MobileCreatingRoomWidget.generated.h"

class UProgressBar;
class UTextBlock;

/** 创建房间与分配 Dedicated Server 期间的全屏反馈页。 */
UCLASS(Abstract, BlueprintType)
class GUIYANGMAHJONG_API UMobileCreatingRoomWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="麻将|UI")
    void SetConnectionStage(const FString& ChineseStatus);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(meta=(BindWidget)) TObjectPtr<UProgressBar> Progress_CreatingRoom;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_CreatingStatus;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_CreatingPercent;

private:
    void RefreshProgress();

    FTimerHandle ProgressTimer;
    double StartedAtSeconds = 0.0;
    bool bConnecting = false;
};
