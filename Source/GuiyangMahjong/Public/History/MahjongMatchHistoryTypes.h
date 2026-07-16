#pragma once

#include "CoreMinimal.h"
#include "Network/MahjongNetworkTypes.h"
#include "MahjongMatchHistoryTypes.generated.h"

USTRUCT(BlueprintType)
struct GUIYANGMAHJONG_API FMahjongMatchHistoryRecord
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString MatchId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString RecordedAtUtc;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMahjongFinalSettlementResult FinalResult;
};
