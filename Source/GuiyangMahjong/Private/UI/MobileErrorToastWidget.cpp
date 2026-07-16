#include "UI/MobileErrorToastWidget.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GuiyangMahjong.h"

void UMobileErrorToastWidget::ShowToast(const FString& Message, const float DurationSeconds)
{
    Txt_Message->SetText(FText::FromString(Message));
    SetVisibility(ESlateVisibility::HitTestInvisible);
    GetWorld()->GetTimerManager().ClearTimer(HideTimer);
    GetWorld()->GetTimerManager().SetTimer(HideTimer, this, &ThisClass::HideToast, FMath::Max(0.1f, DurationSeconds), false);
    UE_LOG(LogMahjongUI, Warning, TEXT("Toast：%s"), *Message);
}
void UMobileErrorToastWidget::HideToast(){ SetVisibility(ESlateVisibility::Collapsed); }
