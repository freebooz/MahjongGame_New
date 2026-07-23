#include "UI/MahjongBackgroundMusicSubsystem.h"

#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UI/MahjongLocalSettings.h"

namespace
{
    constexpr TCHAR BackgroundMusicPath[] =
        TEXT("/Game/UI/Audio/BGM_FirstLightParticles.BGM_FirstLightParticles");
}

void UMahjongBackgroundMusicSubsystem::EnsurePlaying(const UObject* WorldContextObject)
{
    if (!IsValid(MusicComponent))
    {
        USoundBase* Music = LoadObject<USoundBase>(nullptr, BackgroundMusicPath);
        if (!Music || !WorldContextObject)
        {
            return;
        }

        MusicComponent = UGameplayStatics::SpawnSound2D(
            WorldContextObject, Music, 0.0f, 1.0f, 0.0f, nullptr, true, false);
    }

    ApplyLocalSettings();
}

void UMahjongBackgroundMusicSubsystem::ApplyLocalSettings()
{
    if (!IsValid(MusicComponent))
    {
        return;
    }

    const FMahjongLocalSettings Settings = FMahjongLocalSettings::Load();
    MusicComponent->SetVolumeMultiplier(Settings.bMusicEnabled ? Settings.MusicVolume : 0.0f);
}

void UMahjongBackgroundMusicSubsystem::Deinitialize()
{
    if (IsValid(MusicComponent))
    {
        MusicComponent->Stop();
        MusicComponent = nullptr;
    }
    Super::Deinitialize();
}
