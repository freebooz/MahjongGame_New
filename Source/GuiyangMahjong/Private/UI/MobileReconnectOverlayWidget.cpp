#include "UI/MobileReconnectOverlayWidget.h"
#include "Game/GuiyangMahjongPlayerController.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GuiyangMahjong.h"

void UMobileReconnectOverlayWidget::NativeConstruct()
{
    Super::NativeConstruct();
    Btn_Reconnect->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleReconnect);
    Btn_BackConnect->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBackConnect);
}
void UMobileReconnectOverlayWidget::RefreshReconnectState(const FString& Status, const int32 RemainingSeconds, const bool bCanRetry)
{
    Txt_ReconnectStatus->SetText(FText::FromString(Status));
    Txt_RemainingTime->SetText(FText::FromString(FString::Printf(TEXT("剩余 %d 秒"), RemainingSeconds)));
    Btn_Reconnect->SetIsEnabled(bCanRetry);
    UE_LOG(LogMahjongReconnect, Log, TEXT("重连界面刷新：%s，剩余=%d"), *Status, RemainingSeconds);
}
void UMobileReconnectOverlayWidget::HandleReconnect()
{
    UE_LOG(LogMahjongReconnect, Log, TEXT("玩家点击重新连接"));
    if (AGuiyangMahjongPlayerController* PC = Cast<AGuiyangMahjongPlayerController>(GetOwningPlayer())) PC->RetryLastConnection();
}
void UMobileReconnectOverlayWidget::HandleBackConnect(){ RemoveFromParent(); }
