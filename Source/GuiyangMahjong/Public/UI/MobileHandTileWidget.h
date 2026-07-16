#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/MahjongTypes.h"
#include "MobileHandTileWidget.generated.h"

class UButton; class UTextBlock;

/** 可点击手牌组件。首次点击选中上浮，再次点击才发送出牌请求。 */
UCLASS(Abstract, BlueprintType)
class GUIYANGMAHJONG_API UMobileHandTileWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual void NativeConstruct() override;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Btn_Tile;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_TileName;
    UFUNCTION() void HandleTileClicked();
    UPROPERTY(BlueprintReadOnly) FMahjongTile TileData;
    UPROPERTY(BlueprintReadOnly) bool bSelected = false;
public:
    UFUNCTION(BlueprintCallable, Category="麻将|UI") void SetTile(const FMahjongTile& Tile, bool bInteractive);
};
