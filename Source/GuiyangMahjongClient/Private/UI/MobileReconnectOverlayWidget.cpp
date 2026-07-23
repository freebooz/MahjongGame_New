#include "UI/MobileReconnectOverlayWidget.h"
#include "Game/GuiyangMahjongPlayerController.h"
#include "Network/GuiyangReconnectSubsystem.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GuiyangMahjong.h"

void UMobileReconnectOverlayWidget::NativeConstruct()
{
    Super::NativeConstruct();
    Btn_Reconnect->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleReconnect);
    Btn_BackConnect->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBackConnect);
}

void UMobileReconnectOverlayWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    const UGuiyangReconnectSubsystem* Reconnect = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UGuiyangReconnectSubsystem>() : nullptr;
    if (!Reconnect || !Reconnect->IsReconnectPending()) return;
    const int32 RemainingSeconds = Reconnect->GetRemainingSeconds();
    const FString Status = RemainingSeconds > 0
        ? Reconnect->GetStatus()
        : TEXT("重连保留时间已结束，请返回连接界面");
    const bool bCanRetry = Reconnect->CanRetry();
    if (Status != LastDisplayedStatus || RemainingSeconds != LastDisplayedSeconds || bCanRetry != bLastCanRetry)
    {
        RefreshReconnectState(Status, RemainingSeconds, bCanRetry);
    }
}

void UMobileReconnectOverlayWidget::RefreshReconnectState(const FString& Status, const int32 RemainingSeconds, const bool bCanRetry)
{
    LastDisplayedStatus = Status;
    LastDisplayedSeconds = FMath::Max(0, RemainingSeconds);
    bLastCanRetry = bCanRetry;
    Txt_ReconnectStatus->SetText(FText::FromString(Status));
    Txt_RemainingTime->SetText(FText::FromString(FormatRemainingTime(RemainingSeconds)));
    Btn_Reconnect->SetIsEnabled(bCanRetry);
    UE_LOG(LogMahjongReconnect, Log, TEXT("重连界面刷新：%s，剩余=%d"), *Status, RemainingSeconds);
}

FString UMobileReconnectOverlayWidget::FormatRemainingTime(const int32 RemainingSeconds)
{
    return FString::Printf(TEXT("剩余 %d 秒"), FMath::Max(0, RemainingSeconds));
}
void UMobileReconnectOverlayWidget::HandleReconnect()
{
    UE_LOG(LogMahjongReconnect, Log, TEXT("玩家点击重新连接"));
    if (AGuiyangMahjongPlayerController* PC = Cast<AGuiyangMahjongPlayerController>(GetOwningPlayer())) PC->RetryLastConnection();
}
void UMobileReconnectOverlayWidget::HandleBackConnect()
{
    if (AGuiyangMahjongPlayerController* PC = Cast<AGuiyangMahjongPlayerController>(GetOwningPlayer()))
    {
        PC->ReturnToConnectScreen();
    }
    else
    {
        RemoveFromParent();
    }
}
