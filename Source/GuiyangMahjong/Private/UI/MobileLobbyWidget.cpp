#include "UI/MobileLobbyWidget.h"
#include "Game/GuiyangMahjongPlayerController.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GuiyangMahjong.h"

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
    if (AGuiyangMahjongPlayerController* PC = Cast<AGuiyangMahjongPlayerController>(GetOwningPlayer()))
        PC->Server_RequestCreateRoom();
}

void UMobileLobbyWidget::HandleJoinRoom()
{
    if (AGuiyangMahjongPlayerController* PC = Cast<AGuiyangMahjongPlayerController>(GetOwningPlayer()))
        PC->Server_RequestJoinRoom(PC->GetPendingPlayerName());
}

void UMobileLobbyWidget::HandleSetting()
{
    UE_LOG(LogMahjongUI, Log, TEXT("玩家打开本地设置界面"));
}

void UMobileLobbyWidget::HandleQuickStart()
{
    if (AGuiyangMahjongPlayerController* PC = Cast<AGuiyangMahjongPlayerController>(GetOwningPlayer()))
        PC->Server_RequestJoinRoom(PC->GetPendingPlayerName());
}

void UMobileLobbyWidget::RefreshPlayerInfo(const FString& PlayerName, const FString& PlayerId, const int32 OnlineCount)
{
    Txt_PlayerName->SetText(FText::FromString(PlayerName));
    Txt_PlayerId->SetText(FText::FromString(FString::Printf(TEXT("玩家ID：%s"), *PlayerId)));
    Txt_OnlineCount->SetText(FText::FromString(FString::Printf(TEXT("在线人数：%d"), OnlineCount)));
}
