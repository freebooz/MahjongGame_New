#include "UI/MobileConfirmDialogWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GuiyangMahjong.h"

void UMobileConfirmDialogWidget::NativeConstruct()
{
    Super::NativeConstruct();
    Btn_Confirm->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleConfirm);
    Btn_Cancel->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCancel);
}

void UMobileConfirmDialogWidget::Configure(const FString& Title, const FString& Message)
{
    Txt_Title->SetText(FText::FromString(Title));
    Txt_Message->SetText(FText::FromString(Message));
}

void UMobileConfirmDialogWidget::HandleConfirm()
{
    UE_LOG(LogMahjongUI, Log, TEXT("玩家确认弹窗操作"));
    OnConfirmed.Broadcast();
    RemoveFromParent();
}

void UMobileConfirmDialogWidget::HandleCancel()
{
    UE_LOG(LogMahjongUI, Log, TEXT("玩家取消弹窗操作"));
    OnCancelled.Broadcast();
    RemoveFromParent();
}
