#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateTypes.h"
#include "GuiyangUIStyleDataAssets.generated.h"

/** Centralized visual tokens shared by PC and Android UMG. */
UCLASS(BlueprintType)
class GUIYANGMAHJONGCLIENT_API UGuiyangUIThemeDataAsset : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="颜色") TMap<FName, FLinearColor> Colors;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="圆角") TMap<FName, float> CornerRadii;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="边框") TMap<FName, float> BorderWidths;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="尺寸") FVector2D PCDesignSize = FVector2D(1920.0, 1080.0);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="尺寸") FVector2D AndroidWideSize = FVector2D(2400.0, 1080.0);
};

UCLASS(BlueprintType)
class GUIYANGMAHJONGCLIENT_API UGuiyangUIButtonStylesDataAsset : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="按钮") TMap<FName, FButtonStyle> Styles;
};

UCLASS(BlueprintType)
class GUIYANGMAHJONGCLIENT_API UGuiyangUIPanelStylesDataAsset : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="面板") TMap<FName, FSlateBrush> Brushes;
};

UCLASS(BlueprintType)
class GUIYANGMAHJONGCLIENT_API UGuiyangUIFontStylesDataAsset : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="字体") TMap<FName, FTextBlockStyle> Styles;
};

UCLASS(BlueprintType)
class GUIYANGMAHJONGCLIENT_API UGuiyangUIIconRegistryDataAsset : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="图标") TMap<FName, TSoftObjectPtr<UTexture2D>> Icons;
};

UCLASS(BlueprintType)
class GUIYANGMAHJONGCLIENT_API UGuiyangUITileTextureRegistryDataAsset : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="麻将牌") TMap<int32, TSoftObjectPtr<UTexture2D>> RuleIndexToTexture;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="麻将牌") TSoftObjectPtr<UTexture2D> BackTexture;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="麻将牌") TSoftObjectPtr<UTexture2D> FrontBlankTexture;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="麻将牌") TSoftObjectPtr<UTexture2D> SelectedGlowTexture;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="麻将牌") TSoftObjectPtr<UTexture2D> DisabledMaskTexture;
};
