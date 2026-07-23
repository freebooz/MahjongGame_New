#include "UI/MobileConnectServerWidget.h"
#include "Game/GuiyangMahjongPlayerController.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "GuiyangMahjong.h"

void UMobileConnectServerWidget::NativeConstruct()
{
    Super::NativeConstruct();
    Btn_Connect->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleConnectClicked);
    Txt_Version->SetText(FText::FromString(TEXT("版本 0.1.0 · UE 5.8")));
    UE_LOG(LogMahjongUI, Log, TEXT("连接服务器界面创建完成"));
}

void UMobileConnectServerWidget::HandleConnectClicked()
{
    int32 Port = 7777;
    if (!LexTryParseString(Port, *Txt_ServerPort->GetText().ToString())) Port = 0;
    if (AGuiyangMahjongPlayerController* PC = Cast<AGuiyangMahjongPlayerController>(GetOwningPlayer()))
    {
        SetConnecting(true);
        PC->ConnectToServer(Txt_ServerIP->GetText().ToString(), Port, Txt_PlayerName->GetText().ToString());
    }
}

void UMobileConnectServerWidget::SetConnecting(const bool bConnecting)
{
    Btn_Connect->SetIsEnabled(!bConnecting);
    Txt_ConnectButton->SetText(FText::FromString(bConnecting ? TEXT("连接中…") : TEXT("连接服务器")));
}
