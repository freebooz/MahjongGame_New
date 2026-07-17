#include "UI/MahjongUISoundLibrary.h"

#include "GuiyangMahjong.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace
{
const TCHAR* ResolveSoundPath(const EMahjongUISound SoundType)
{
    switch (SoundType)
    {
    case EMahjongUISound::ButtonClick: return TEXT("/Game/UI/Audio/SFX_UI_Click.SFX_UI_Click");
    case EMahjongUISound::TileSelect: return TEXT("/Game/UI/Audio/SFX_Tile_Select.SFX_Tile_Select");
    case EMahjongUISound::TilePlay: return TEXT("/Game/UI/Audio/SFX_Tile_Play.SFX_Tile_Play");
    case EMahjongUISound::Peng: return TEXT("/Game/UI/Audio/SFX_Peng.SFX_Peng");
    case EMahjongUISound::Gang: return TEXT("/Game/UI/Audio/SFX_Gang.SFX_Gang");
    case EMahjongUISound::Hu: return TEXT("/Game/UI/Audio/SFX_Hu.SFX_Hu");
    case EMahjongUISound::Pass: return TEXT("/Game/UI/Audio/SFX_Pass.SFX_Pass");
    default: return nullptr;
    }
}
}

bool UMahjongUISoundLibrary::PlayUISound(const UObject* WorldContextObject, const EMahjongUISound SoundType)
{
    const TCHAR* AssetPath = ResolveSoundPath(SoundType);
    if (!WorldContextObject || !AssetPath)
    {
        return false;
    }

    USoundBase* Sound = LoadObject<USoundBase>(nullptr, AssetPath);
    if (!Sound)
    {
        UE_LOG(LogMahjongUI, Warning, TEXT("UI 音效资源缺失，已安全跳过：%s"), AssetPath);
        return false;
    }

    UGameplayStatics::PlaySound2D(WorldContextObject, Sound);
    return true;
}
