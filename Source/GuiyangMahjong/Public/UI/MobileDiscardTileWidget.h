#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/MahjongTypes.h"
#include "MobileDiscardTileWidget.generated.h"

class UBorder; class UTextBlock;

/** 只读弃牌组件，不响应点击。 */
UCLASS(Abstract, BlueprintType)
class GUIYANGMAHJONG_API UMobileDiscardTileWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UBorder> Border_Tile;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_TileName;
public:
    UFUNCTION(BlueprintCallable, Category="麻将|UI") void SetDiscard(const FMahjongTile& Tile, bool bLatest, float SeatRotation);
};
