#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MahjongBackgroundMusicSubsystem.generated.h"

class UAudioComponent;

/** 全局背景音乐管理器：跨界面持续播放，并实时响应本地音乐开关与音量。 */
UCLASS()
class GUIYANGMAHJONG_API UMahjongBackgroundMusicSubsystem final : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** 在首个可用世界中启动背景音乐；重复调用不会重复创建播放器。 */
    void EnsurePlaying(const UObject* WorldContextObject);

    /** 从本地设置重新应用音乐开关与音量。 */
    void ApplyLocalSettings();

    virtual void Deinitialize() override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> MusicComponent;
};
