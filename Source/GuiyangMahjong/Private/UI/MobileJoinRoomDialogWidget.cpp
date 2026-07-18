#include "UI/MobileJoinRoomDialogWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "GuiyangMahjong.h"
#include "Lobby/GuiyangLobbySubsystem.h"

void UMobileJoinRoomDialogWidget::NativeConstruct()
{
    Super::NativeConstruct();
    Btn_Join->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleJoin);
    Btn_Cancel->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCancel);
    Txt_Status->SetText(FText::FromString(TEXT("请输入 6 位房间号")));
}

void UMobileJoinRoomDialogWidget::HandleJoin()
{
    const FString RoomCode = Txt_RoomCode->GetText().ToString().TrimStartAndEnd();
    if (RoomCode.Len() != 6 || !RoomCode.IsNumeric())
    {
        Txt_Status->SetText(FText::FromString(TEXT("房间号必须为 6 位数字")));
        return;
    }

    FMahjongJoinRoomRequest Request;
    Request.RoomCode = RoomCode;
    Request.Password = Txt_Password->GetText().ToString();
    UGuiyangLobbySubsystem* Lobby = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UGuiyangLobbySubsystem>() : nullptr;
    if (!Lobby)
    {
        Txt_Status->SetText(FText::FromString(TEXT("大厅服务尚未初始化，请稍后重试")));
        return;
    }

    const FGuiyangLobbyOperationResult Result = Lobby->RequestJoinRoom(GetOwningPlayer(), Request);
    if (!Result.bAccepted)
    {
        Txt_Status->SetText(FText::FromString(Result.ChineseMessage));
        return;
    }

    UE_LOG(LogMahjongUI, Log, TEXT("加入房间请求已提交：RequestId=%s，Room=%s"),
        *Result.RequestId, *RoomCode);
    RemoveFromParent();
}

void UMobileJoinRoomDialogWidget::HandleCancel()
{
    RemoveFromParent();
}
