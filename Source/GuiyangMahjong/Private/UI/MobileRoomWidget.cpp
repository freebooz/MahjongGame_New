#include "UI/MobileRoomWidget.h"
#include "Game/GuiyangMahjongPlayerController.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GuiyangMahjong.h"

void UMobileRoomWidget::NativeConstruct()
{
    Super::NativeConstruct();
    Btn_Ready->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleReady);
    Btn_LeaveRoom->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLeave);
    UE_LOG(LogMahjongUI, Log, TEXT("房间界面创建完成"));
}

void UMobileRoomWidget::HandleReady()
{
    if (AGuiyangMahjongPlayerController* PC = Cast<AGuiyangMahjongPlayerController>(GetOwningPlayer())) PC->Server_RequestReady();
}

void UMobileRoomWidget::HandleLeave()
{
    if (AGuiyangMahjongPlayerController* PC = Cast<AGuiyangMahjongPlayerController>(GetOwningPlayer())) PC->Server_RequestLeaveRoom();
}

void UMobileRoomWidget::RefreshRoomState(const FMahjongRoomState& State, const int32 LocalSeat)
{
    Txt_RoomId->SetText(FText::FromString(FString::Printf(TEXT("房间号：%s"), *State.RoomInfo.RoomId)));
    Txt_RuleSummary->SetText(FText::FromString(State.RoomInfo.RuleSummary));
    UTextBlock* SeatWidgets[] = {Seat_Bottom, Seat_Right, Seat_Top, Seat_Left};
    for (int32 Index = 0; Index < 4; ++Index)
    {
        const FMahjongSeatInfo* Seat = State.Seats.FindByPredicate([Index](const FMahjongSeatInfo& Item){ return Item.SeatIndex == Index; });
        const FString Label = Seat && Seat->bOccupied
            ? FString::Printf(TEXT("%s%s\n%s"), Index == LocalSeat ? TEXT("【我】") : TEXT(""), *Seat->PlayerName, Seat->bReady ? TEXT("已准备") : TEXT("未准备"))
            : TEXT("等待玩家");
        SeatWidgets[Index]->SetText(FText::FromString(Label));
    }
    Txt_StartTip->SetText(FText::FromString(State.bGameStarting ? TEXT("四人已就绪，即将开局") : TEXT("满四人并准备后自动开始")));
}
