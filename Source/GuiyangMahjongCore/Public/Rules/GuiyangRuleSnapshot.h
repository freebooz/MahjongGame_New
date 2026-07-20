#pragma once

#include "CoreMinimal.h"
#include "Core/MahjongTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GuiyangRuleSnapshot.generated.h"

/** 房间创建时生成的不可变规则快照，可用于开局、重连和回放校验。 */
USTRUCT(BlueprintType)
struct GUIYANGMAHJONGCORE_API FGuiyangRuleSnapshot
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FMahjongRuleConfig Config;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString CanonicalDefinition;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString RuleHash;

    int32 GetTileCount() const;
};

UCLASS()
class GUIYANGMAHJONGCORE_API UGuiyangRuleSnapshotLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="麻将|规则")
    static FGuiyangRuleSnapshot CreateSnapshot(const FMahjongRuleConfig& RequestedConfig);

    UFUNCTION(BlueprintPure, Category="麻将|规则")
    static bool VerifySnapshot(const FGuiyangRuleSnapshot& Snapshot);

private:
    static FMahjongRuleConfig NormalizeConfig(const FMahjongRuleConfig& RequestedConfig);
    static FString BuildCanonicalDefinition(const FMahjongRuleConfig& Config);
    static FString CalculateHash(const FString& CanonicalDefinition);
};
