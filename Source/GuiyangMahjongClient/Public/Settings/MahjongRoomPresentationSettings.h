#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPtr.h"
#include "MahjongRoomPresentationSettings.generated.h"

class AMahjongRoomPresentationActor;

/** Client-only soft reference to the designer-authored Mahjong room presentation class. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Mahjong Room Presentation"))
class GUIYANGMAHJONGCLIENT_API UMahjongRoomPresentationSettings final : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UMahjongRoomPresentationSettings();

    virtual FName GetCategoryName() const override { return TEXT("Game"); }

    UPROPERTY(Config, EditAnywhere, Category="Presentation",
        meta=(AllowedClasses="/Script/GuiyangMahjongClient.MahjongRoomPresentationActor"))
    TSoftClassPtr<AMahjongRoomPresentationActor> PresentationClass;
};
