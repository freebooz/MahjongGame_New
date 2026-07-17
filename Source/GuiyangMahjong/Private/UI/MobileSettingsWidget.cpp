#include "UI/MobileSettingsWidget.h"

#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GuiyangMahjong.h"
#include "UI/MahjongLocalSettings.h"
#include "UI/MahjongBackgroundMusicSubsystem.h"

void UMobileSettingsWidget::NativeConstruct()
{
    Super::NativeConstruct();
    Chk_MusicEnabled->OnCheckStateChanged.AddUniqueDynamic(this, &ThisClass::HandleToggleChanged);
    Chk_SoundEnabled->OnCheckStateChanged.AddUniqueDynamic(this, &ThisClass::HandleToggleChanged);
    Chk_VibrationEnabled->OnCheckStateChanged.AddUniqueDynamic(this, &ThisClass::HandleToggleChanged);
    Slider_MusicVolume->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleMusicVolumeChanged);
    Slider_SoundVolume->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleSoundVolumeChanged);
    Btn_Reset->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleReset);
    Btn_ExitGame->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleExitGame);
    Btn_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClose);
    ApplySettingsToControls();
    UE_LOG(LogMahjongUI, Log, TEXT("本地设置界面已打开"));
}

void UMobileSettingsWidget::ApplySettingsToControls()
{
    const FMahjongLocalSettings Settings = FMahjongLocalSettings::Load();
    bUpdatingControls = true;
    Chk_MusicEnabled->SetIsChecked(Settings.bMusicEnabled);
    Chk_SoundEnabled->SetIsChecked(Settings.bSoundEnabled);
    Chk_VibrationEnabled->SetIsChecked(Settings.bVibrationEnabled);
    Slider_MusicVolume->SetValue(Settings.MusicVolume);
    Slider_SoundVolume->SetValue(Settings.SoundVolume);
    Slider_MusicVolume->SetIsEnabled(Settings.bMusicEnabled);
    Slider_SoundVolume->SetIsEnabled(Settings.bSoundEnabled);
    bUpdatingControls = false;
    UpdateVolumeLabels();
}

void UMobileSettingsWidget::SaveSettingsFromControls()
{
    if (bUpdatingControls)
    {
        return;
    }

    FMahjongLocalSettings Settings;
    Settings.bMusicEnabled = Chk_MusicEnabled->IsChecked();
    Settings.bSoundEnabled = Chk_SoundEnabled->IsChecked();
    Settings.bVibrationEnabled = Chk_VibrationEnabled->IsChecked();
    Settings.MusicVolume = Slider_MusicVolume->GetValue();
    Settings.SoundVolume = Slider_SoundVolume->GetValue();
    Settings.Save();
    if (UMahjongBackgroundMusicSubsystem* Music =
        GetGameInstance()->GetSubsystem<UMahjongBackgroundMusicSubsystem>())
    {
        Music->ApplyLocalSettings();
    }
    Slider_MusicVolume->SetIsEnabled(Settings.bMusicEnabled);
    Slider_SoundVolume->SetIsEnabled(Settings.bSoundEnabled);
    UpdateVolumeLabels();
}

void UMobileSettingsWidget::UpdateVolumeLabels()
{
    Txt_MusicVolumeValue->SetText(FText::AsNumber(FMath::RoundToInt(Slider_MusicVolume->GetValue() * 100.0f)));
    Txt_SoundVolumeValue->SetText(FText::AsNumber(FMath::RoundToInt(Slider_SoundVolume->GetValue() * 100.0f)));
}

void UMobileSettingsWidget::HandleToggleChanged(const bool bIsChecked)
{
    SaveSettingsFromControls();
}

void UMobileSettingsWidget::HandleMusicVolumeChanged(const float Value)
{
    SaveSettingsFromControls();
}

void UMobileSettingsWidget::HandleSoundVolumeChanged(const float Value)
{
    SaveSettingsFromControls();
}

void UMobileSettingsWidget::HandleReset()
{
    FMahjongLocalSettings().Save();
    ApplySettingsToControls();
    if (UMahjongBackgroundMusicSubsystem* Music =
        GetGameInstance()->GetSubsystem<UMahjongBackgroundMusicSubsystem>())
    {
        Music->ApplyLocalSettings();
    }
}

void UMobileSettingsWidget::HandleExitGame()
{
    UE_LOG(LogMahjongUI, Log, TEXT("玩家从设置界面退出游戏"));
    UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UMobileSettingsWidget::HandleClose()
{
    RemoveFromParent();
    UE_LOG(LogMahjongUI, Log, TEXT("本地设置界面已关闭"));
}
