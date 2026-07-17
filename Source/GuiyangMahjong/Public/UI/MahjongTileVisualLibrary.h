#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Core/MahjongTypes.h"
#include "MahjongTileVisualLibrary.generated.h"

class UTexture2D;

/** 麻将牌视觉资源映射。规则索引只用于规则，视觉路径按花色和点数生成，避免条/筒顺序混淆。 */
UCLASS()
class GUIYANGMAHJONG_API UMahjongTileVisualLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** 返回数牌牌面资源路径；字牌或非法牌返回空字符串，由控件回退为文字。 */
    UFUNCTION(BlueprintPure, Category="麻将|UI")
    static FString GetFaceTexturePath(const FMahjongTile& Tile);

    /** 同步读取一张小型 UI 牌面纹理，资源首次读取后由 UObject 系统缓存。 */
    static UTexture2D* LoadFaceTexture(const FMahjongTile& Tile);
};
