#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/MahjongTypes.h"
#include "MobileRuleConfigWidget.generated.h"

class UCheckBox;
class UEditableTextBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMahjongRuleConfigChanged, FMahjongRuleConfig, Config);

/** 房间规则配置组件。只编辑请求参数，不直接修改房间或牌桌权威状态。 */
UCLASS(Abstract, BlueprintType)
class GUIYANGMAHJONGCLIENT_API UMobileRuleConfigWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta=(BindWidget)) TObjectPtr<UCheckBox> Chk_Standard136;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UCheckBox> Chk_ChongFengJi;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UCheckBox> Chk_ZeRenJi;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UCheckBox> Chk_WuGuJi;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UCheckBox> Chk_QiangGangHu;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UCheckBox> Chk_YiPaoDuoXiang;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UCheckBox> Chk_QiDui;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UCheckBox> Chk_TimeoutAutoPlay;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UEditableTextBox> Txt_BaseScore;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UEditableTextBox> Txt_JiScore;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UEditableTextBox> Txt_GangScore;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UEditableTextBox> Txt_ZiMoMultiplier;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UEditableTextBox> Txt_TurnTimeout;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UEditableTextBox> Txt_ReactionTimeout;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UEditableTextBox> Txt_ReconnectTimeout;

    UFUNCTION() void HandleCheckChanged(bool bChecked);
    UFUNCTION() void HandleTextChanged(const FText& Text);

public:
    UPROPERTY(BlueprintAssignable, Category="麻将|规则") FMahjongRuleConfigChanged OnRuleConfigChanged;

    UFUNCTION(BlueprintCallable, Category="麻将|规则") void SetRuleConfig(const FMahjongRuleConfig& Config);
    bool TryGetRuleConfig(FMahjongRuleConfig& OutConfig, FString& OutError) const;

private:
    void BroadcastRuleConfigChanged();
};
