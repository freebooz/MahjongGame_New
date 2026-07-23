#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Rules/GuiyangRuleSnapshot.h"
#include "MobileRuleSummaryWidget.generated.h"

class UTextBlock;

/** 展示不可变规则快照及其哈希，供创建房确认和房间内一致性核对。 */
UCLASS(Abstract, BlueprintType)
class GUIYANGMAHJONGCLIENT_API UMobileRuleSummaryWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_RuleTitle;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_RuleLines;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_RuleHash;

public:
    UFUNCTION(BlueprintCallable, Category="麻将|规则")
    void SetRuleConfig(const FMahjongRuleConfig& Config, int32 RoundCount, bool bPasswordProtected);

    UFUNCTION(BlueprintCallable, Category="麻将|规则")
    void SetRuleSnapshot(const FGuiyangRuleSnapshot& Snapshot, int32 RoundCount, bool bPasswordProtected);

    static FString BuildSummaryText(const FGuiyangRuleSnapshot& Snapshot, int32 RoundCount, bool bPasswordProtected);
};
