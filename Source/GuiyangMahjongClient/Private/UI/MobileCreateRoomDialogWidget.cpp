#include "UI/MobileCreateRoomDialogWidget.h"

#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "GuiyangMahjong.h"
#include "Game/GuiyangMahjongPlayerController.h"
#include "Lobby/GuiyangLobbySubsystem.h"
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

    AGuiyangMahjongPlayerController* Controller =
        Cast<AGuiyangMahjongPlayerController>(GetOwningPlayer());
    if (!Controller)
    {
        Txt_Status->SetText(FText::FromString(TEXT("当前玩家控制器不可用，请稍后重试")));
        return;
    }

    Btn_Create->SetIsEnabled(false);
    Controller->RequestCreateRoomWithLoading(Request);
    RemoveFromParent();
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
