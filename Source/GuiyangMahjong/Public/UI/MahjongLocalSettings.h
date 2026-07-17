#pragma once

#include "CoreMinimal.h"

/** 客户端本地视听与触感设置，不参与任何服务端权威状态。 */
struct GUIYANGMAHJONG_API FMahjongLocalSettings
{
    bool bMusicEnabled = true;
    bool bSoundEnabled = true;
    bool bVibrationEnabled = true;
    float MusicVolume = 0.70f;
    float SoundVolume = 0.85f;

    static FMahjongLocalSettings Load();
    void Save() const;
    void Sanitize();
};
