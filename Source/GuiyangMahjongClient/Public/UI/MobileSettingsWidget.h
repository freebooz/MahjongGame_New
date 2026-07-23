#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MobileSettingsWidget.generated.h"

class UButton;
class UCheckBox;
class USlider;
class UTextBlock;

/** 大厅本地设置弹窗，设置即时生效并写入设备本地配置。 */
UCLASS(Abstract, BlueprintType)
class GUIYANGMAHJONGCLIENT_API UMobileSettingsWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta=(BindWidget)) TObjectPtr<UCheckBox> Chk_MusicEnabled;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UCheckBox> Chk_SoundEnabled;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UCheckBox> Chk_VibrationEnabled;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<USlider> Slider_MusicVolume;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<USlider> Slider_SoundVolume;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_MusicVolumeValue;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_SoundVolumeValue;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Btn_Reset;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Btn_ExitGame;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Btn_Close;

    UFUNCTION() void HandleToggleChanged(bool bIsChecked);
    UFUNCTION() void HandleMusicVolumeChanged(float Value);
    UFUNCTION() void HandleSoundVolumeChanged(float Value);
    UFUNCTION() void HandleReset();
    UFUNCTION() void HandleExitGame();
    UFUNCTION() void HandleClose();

private:
    bool bUpdatingControls = false;
    void ApplySettingsToControls();
    void SaveSettingsFromControls();
    void UpdateVolumeLabels();
};
