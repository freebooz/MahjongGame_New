#include "UI/MahjongLocalSettings.h"

#include "Misc/ConfigCacheIni.h"

namespace
{
const TCHAR* SettingsSection = TEXT("/Script/GuiyangMahjong.MahjongLocalSettings");
}

FMahjongLocalSettings FMahjongLocalSettings::Load()
{
    FMahjongLocalSettings Settings;
    if (GConfig)
    {
        GConfig->GetBool(SettingsSection, TEXT("MusicEnabled"), Settings.bMusicEnabled, GGameUserSettingsIni);
        GConfig->GetBool(SettingsSection, TEXT("SoundEnabled"), Settings.bSoundEnabled, GGameUserSettingsIni);
        GConfig->GetBool(SettingsSection, TEXT("VibrationEnabled"), Settings.bVibrationEnabled, GGameUserSettingsIni);
        GConfig->GetFloat(SettingsSection, TEXT("MusicVolume"), Settings.MusicVolume, GGameUserSettingsIni);
        GConfig->GetFloat(SettingsSection, TEXT("SoundVolume"), Settings.SoundVolume, GGameUserSettingsIni);
    }
    Settings.Sanitize();
    return Settings;
}

void FMahjongLocalSettings::Save() const
{
    if (!GConfig)
    {
        return;
    }

    FMahjongLocalSettings Settings = *this;
    Settings.Sanitize();
    GConfig->SetBool(SettingsSection, TEXT("MusicEnabled"), Settings.bMusicEnabled, GGameUserSettingsIni);
    GConfig->SetBool(SettingsSection, TEXT("SoundEnabled"), Settings.bSoundEnabled, GGameUserSettingsIni);
    GConfig->SetBool(SettingsSection, TEXT("VibrationEnabled"), Settings.bVibrationEnabled, GGameUserSettingsIni);
    GConfig->SetFloat(SettingsSection, TEXT("MusicVolume"), Settings.MusicVolume, GGameUserSettingsIni);
    GConfig->SetFloat(SettingsSection, TEXT("SoundVolume"), Settings.SoundVolume, GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni);
}

void FMahjongLocalSettings::Sanitize()
{
    MusicVolume = FMath::Clamp(MusicVolume, 0.0f, 1.0f);
    SoundVolume = FMath::Clamp(SoundVolume, 0.0f, 1.0f);
}
