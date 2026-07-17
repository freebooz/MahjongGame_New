#include "UI/MobileRuleConfigWidget.h"

#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"

namespace
{
    bool ParseRuleNumber(const UEditableTextBox* Widget, const TCHAR* Label,
        const int32 Minimum, const int32 Maximum, int32& OutValue, FString& OutError)
    {
        if (!Widget || !LexTryParseString(OutValue, *Widget->GetText().ToString())
            || OutValue < Minimum || OutValue > Maximum)
        {
            OutError = FString::Printf(TEXT("%s必须为 %d 到 %d"), Label, Minimum, Maximum);
            return false;
        }
        return true;
    }
}

void UMobileRuleConfigWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetRuleConfig(FMahjongRuleConfig());

    UCheckBox* CheckBoxes[] = {
        Chk_Standard136, Chk_ChongFengJi, Chk_ZeRenJi, Chk_WuGuJi,
        Chk_QiangGangHu, Chk_YiPaoDuoXiang, Chk_QiDui, Chk_TimeoutAutoPlay
    };
    for (UCheckBox* CheckBox : CheckBoxes)
    {
        CheckBox->OnCheckStateChanged.AddUniqueDynamic(this, &ThisClass::HandleCheckChanged);
    }

    UEditableTextBox* NumberFields[] = {
        Txt_BaseScore, Txt_JiScore, Txt_GangScore, Txt_ZiMoMultiplier,
        Txt_TurnTimeout, Txt_ReactionTimeout, Txt_ReconnectTimeout
    };
    for (UEditableTextBox* NumberField : NumberFields)
    {
        NumberField->OnTextChanged.AddUniqueDynamic(this, &ThisClass::HandleTextChanged);
    }
}

void UMobileRuleConfigWidget::SetRuleConfig(const FMahjongRuleConfig& Config)
{
    Chk_Standard136->SetIsChecked(Config.TileSetMode == EMahjongTileSetMode::Standard136);
    Chk_ChongFengJi->SetIsChecked(Config.bEnableChongFengJi);
    Chk_ZeRenJi->SetIsChecked(Config.bEnableZeRenJi);
    Chk_WuGuJi->SetIsChecked(Config.bEnableWuGuJi);
    Chk_QiangGangHu->SetIsChecked(Config.bEnableQiangGangHu);
    Chk_YiPaoDuoXiang->SetIsChecked(Config.bEnableYiPaoDuoXiang);
    Chk_QiDui->SetIsChecked(Config.bEnableQiDui);
    Chk_TimeoutAutoPlay->SetIsChecked(Config.bEnableTimeoutAutoPlay);
    Txt_BaseScore->SetText(FText::AsNumber(Config.BaseScore));
    Txt_JiScore->SetText(FText::AsNumber(Config.JiScore));
    Txt_GangScore->SetText(FText::AsNumber(Config.GangScore));
    Txt_ZiMoMultiplier->SetText(FText::AsNumber(Config.ZiMoMultiplier));
    Txt_TurnTimeout->SetText(FText::AsNumber(Config.TurnTimeoutSeconds));
    Txt_ReactionTimeout->SetText(FText::AsNumber(Config.ReactionTimeoutSeconds));
    Txt_ReconnectTimeout->SetText(FText::AsNumber(Config.ReconnectTimeoutSeconds));
}

bool UMobileRuleConfigWidget::TryGetRuleConfig(FMahjongRuleConfig& OutConfig, FString& OutError) const
{
    OutConfig = FMahjongRuleConfig();
    OutError.Reset();
    OutConfig.TileSetMode = Chk_Standard136->IsChecked()
        ? EMahjongTileSetMode::Standard136 : EMahjongTileSetMode::Suited108;
    OutConfig.bEnableChongFengJi = Chk_ChongFengJi->IsChecked();
    OutConfig.bEnableZeRenJi = Chk_ZeRenJi->IsChecked();
    OutConfig.bEnableWuGuJi = Chk_WuGuJi->IsChecked();
    OutConfig.bEnableQiangGangHu = Chk_QiangGangHu->IsChecked();
    OutConfig.bEnableYiPaoDuoXiang = Chk_YiPaoDuoXiang->IsChecked();
    OutConfig.bEnableQiDui = Chk_QiDui->IsChecked();
    OutConfig.bEnableTimeoutAutoPlay = Chk_TimeoutAutoPlay->IsChecked();

    return ParseRuleNumber(Txt_BaseScore, TEXT("底分"), 1, 100, OutConfig.BaseScore, OutError)
        && ParseRuleNumber(Txt_JiScore, TEXT("鸡分"), 0, 100, OutConfig.JiScore, OutError)
        && ParseRuleNumber(Txt_GangScore, TEXT("豆分"), 0, 100, OutConfig.GangScore, OutError)
        && ParseRuleNumber(Txt_ZiMoMultiplier, TEXT("自摸倍数"), 1, 16, OutConfig.ZiMoMultiplier, OutError)
        && ParseRuleNumber(Txt_TurnTimeout, TEXT("出牌超时"), 3, 120, OutConfig.TurnTimeoutSeconds, OutError)
        && ParseRuleNumber(Txt_ReactionTimeout, TEXT("操作超时"), 3, 60, OutConfig.ReactionTimeoutSeconds, OutError)
        && ParseRuleNumber(Txt_ReconnectTimeout, TEXT("重连时限"), 15, 600, OutConfig.ReconnectTimeoutSeconds, OutError);
}

void UMobileRuleConfigWidget::HandleCheckChanged(const bool bChecked)
{
    BroadcastRuleConfigChanged();
}

void UMobileRuleConfigWidget::HandleTextChanged(const FText& Text)
{
    BroadcastRuleConfigChanged();
}

void UMobileRuleConfigWidget::BroadcastRuleConfigChanged()
{
    FMahjongRuleConfig Config;
    FString Error;
    if (TryGetRuleConfig(Config, Error))
    {
        OnRuleConfigChanged.Broadcast(Config);
    }
}
