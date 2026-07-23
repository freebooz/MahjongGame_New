#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MahjongUISoundLibrary.generated.h"

/** 麻将桌 UI 与操作反馈音效类型。 */
UENUM(BlueprintType)
enum class EMahjongUISound : uint8
{
    ButtonClick UMETA(DisplayName="按钮点击"),
    TileSelect UMETA(DisplayName="选牌"),
    TilePlay UMETA(DisplayName="出牌"),
    Peng UMETA(DisplayName="碰"),
    Gang UMETA(DisplayName="杠"),
    Hu UMETA(DisplayName="胡"),
    Pass UMETA(DisplayName="过")
};

/** 集中管理 UI 音效资源路径，资源缺失时安全跳过，不阻断交互。 */
UCLASS()
class GUIYANGMAHJONGCLIENT_API UMahjongUISoundLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="麻将|音效", meta=(WorldContext="WorldContextObject"))
    static bool PlayUISound(const UObject* WorldContextObject, EMahjongUISound SoundType);
};
