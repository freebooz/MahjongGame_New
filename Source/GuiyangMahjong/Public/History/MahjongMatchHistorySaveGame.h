#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "History/MahjongMatchHistoryTypes.h"
#include "MahjongMatchHistorySaveGame.generated.h"

UCLASS()
class GUIYANGMAHJONG_API UMahjongMatchHistorySaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() TArray<FMahjongMatchHistoryRecord> Records;
};
