#include "UI/MobileLobbyWidget.h"
#include "Game/GuiyangMahjongPlayerController.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GuiyangMahjong.h"
#include "UI/MobileCreateRoomDialogWidget.h"
#include "UI/MobileJoinRoomDialogWidget.h"

void UMobileLobbyWidget::NativeConstruct()
{
    Super::NativeConstruct();
    Btn_QuickStart->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleQuickStart);
    Btn_CreateRoom->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCreateRoom);
    Btn_JoinRoom->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleJoinRoom);
    Btn_Setting->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSetting);
    UE_LOG(LogMahjongUI, Log, TEXT("大厅界面创建完成"));
}

void UMobileLobbyWidget::HandleCreateRoom()
{
    if (!CreateRoomDialogInstance || !CreateRoomDialogInstance->IsInViewport())
    {
        UClass* DialogClass = LoadClass<UMobileCreateRoomDialogWidget>(nullptr,
            TEXT("/Game/UI/Dialogs/WBP_CreateRoomDialog.WBP_CreateRoomDialog_C"));
        if (!DialogClass)
        {
            UE_LOG(LogMahjongUI, Error, TEXT("无法加载创建房间弹窗"));
            return;
        }
        CreateRoomDialogInstance = CreateWidget<UMobileCreateRoomDialogWidget>(GetOwningPlayer(), DialogClass);
        CreateRoomDialogInstance->AddToViewport(200);
    }
}

void UMobileLobbyWidget::HandleJoinRoom()
{
    if (!JoinRoomDialogInstance || !JoinRoomDialogInstance->IsInViewport())
    {
        UClass* DialogClass = LoadClass<UMobileJoinRoomDialogWidget>(nullptr,
            TEXT("/Game/UI/Dialogs/WBP_JoinRoomDialog.WBP_JoinRoomDialog_C"));
        if (!DialogClass)
        {
            UE_LOG(LogMahjongUI, Error, TEXT("无法加载加入房间弹窗"));
            return;
        }
        JoinRoomDialogInstance = CreateWidget<UMobileJoinRoomDialogWidget>(GetOwningPlayer(), DialogClass);
        JoinRoomDialogInstance->AddToViewport(200);
    }
}

void UMobileLobbyWidget::HandleSetting()
{
    UE_LOG(LogMahjongUI, Log, TEXT("玩家打开本地设置界面"));
}

void UMobileLobbyWidget::HandleQuickStart()
{
    if (AGuiyangMahjongPlayerController* PC = Cast<AGuiyangMahjongPlayerController>(GetOwningPlayer()))
        PC->Server_RequestQuickStart();
}

void UMobileLobbyWidget::RefreshPlayerInfo(const FString& PlayerName, const FString& PlayerId, const int32 OnlineCount)
{
    Txt_PlayerName->SetText(FText::FromString(PlayerName));
    Txt_PlayerId->SetText(FText::FromString(FString::Printf(TEXT("玩家ID：%s"), *PlayerId)));
    Txt_OnlineCount->SetText(FText::FromString(FString::Printf(TEXT("在线人数：%d"), OnlineCount)));
}
