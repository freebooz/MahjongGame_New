#include "UI/MobileCreateRoomDialogWidget.h"

#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Game/GuiyangMahjongPlayerController.h"
#include "GuiyangMahjong.h"
#include "UI/MobileRuleConfigWidget.h"
#include "UI/MobileRuleSummaryWidget.h"

void UMobileCreateRoomDialogWidget::NativeConstruct()
{
    Super::NativeConstruct();
    Btn_Create->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCreate);
    Btn_Cancel->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCancel);
    Txt_RoundCount->SetText(FText::AsNumber(4));
    Chk_PublicRoom->SetIsChecked(true);
    RuleConfig->OnRuleConfigChanged.AddUniqueDynamic(this, &ThisClass::HandleRuleConfigChanged);
    Txt_RoundCount->OnTextChanged.AddUniqueDynamic(this, &ThisClass::HandleOptionTextChanged);
    Chk_PublicRoom->OnCheckStateChanged.AddUniqueDynamic(this, &ThisClass::HandleOptionCheckChanged);
    Chk_EnablePassword->OnCheckStateChanged.AddUniqueDynamic(this, &ThisClass::HandleOptionCheckChanged);
    Txt_Status->SetText(FText::FromString(TEXT("请确认规则摘要后创建房间")));
    RefreshRuleSummary();
}

void UMobileCreateRoomDialogWidget::HandleCreate()
{
    int32 RoundCount = 0;
    if (!LexTryParseString(RoundCount, *Txt_RoundCount->GetText().ToString())
        || RoundCount < 1 || RoundCount > 16)
    {
        Txt_Status->SetText(FText::FromString(TEXT("局数必须为 1 到 16")));
        return;
    }

    FMahjongCreateRoomRequest Request;
    Request.RoundCount = RoundCount;
    Request.bPublicRoom = Chk_PublicRoom->IsChecked();
    Request.bEnablePassword = Chk_EnablePassword->IsChecked();
    Request.Password = Txt_Password->GetText().ToString();
    FString RuleError;
    if (!RuleConfig->TryGetRuleConfig(Request.Rules, RuleError))
    {
        Txt_Status->SetText(FText::FromString(RuleError));
        return;
    }
    if (Request.bEnablePassword && (Request.Password.Len() < 6 || Request.Password.Len() > 12))
    {
        Txt_Status->SetText(FText::FromString(TEXT("房间密码必须为 6 到 12 个字符")));
        return;
    }

    if (AGuiyangMahjongPlayerController* PC = Cast<AGuiyangMahjongPlayerController>(GetOwningPlayer()))
    {
        PC->Server_RequestCreateRoomWithConfig(Request);
        UE_LOG(LogMahjongUI, Log, TEXT("创建房间参数已提交：局数=%d，公开房=%s，密码房=%s"),
            Request.RoundCount, Request.bPublicRoom ? TEXT("是") : TEXT("否"),
            Request.bEnablePassword ? TEXT("是") : TEXT("否"));
        RemoveFromParent();
    }
}

void UMobileCreateRoomDialogWidget::HandleCancel()
{
    RemoveFromParent();
}

void UMobileCreateRoomDialogWidget::HandleRuleConfigChanged(const FMahjongRuleConfig Config)
{
    RefreshRuleSummary();
}

void UMobileCreateRoomDialogWidget::HandleOptionCheckChanged(const bool bChecked)
{
    RefreshRuleSummary();
}

void UMobileCreateRoomDialogWidget::HandleOptionTextChanged(const FText& Text)
{
    RefreshRuleSummary();
}

void UMobileCreateRoomDialogWidget::RefreshRuleSummary()
{
    int32 RoundCount = 4;
    if (!LexTryParseString(RoundCount, *Txt_RoundCount->GetText().ToString())
        || RoundCount < 1 || RoundCount > 16)
    {
        return;
    }
    FMahjongRuleConfig Config;
    FString Error;
    if (RuleConfig->TryGetRuleConfig(Config, Error))
    {
        RuleSummary->SetRuleConfig(Config, RoundCount, Chk_EnablePassword->IsChecked());
    }
}
