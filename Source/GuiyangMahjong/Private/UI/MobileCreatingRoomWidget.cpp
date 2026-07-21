#include "UI/MobileCreatingRoomWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "HAL/PlatformTime.h"
#include "TimerManager.h"

void UMobileCreatingRoomWidget::NativeConstruct()
{
    Super::NativeConstruct();
    StartedAtSeconds = FPlatformTime::Seconds();
    bConnecting = false;
    Progress_CreatingRoom->SetPercent(0.08f);
    Txt_CreatingStatus->SetText(FText::FromString(TEXT("正在创建房间……")));
    Txt_CreatingPercent->SetText(FText::FromString(TEXT("8%")));
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            ProgressTimer, this, &ThisClass::RefreshProgress, 0.1f, true);
    }
}

void UMobileCreatingRoomWidget::NativeDestruct()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ProgressTimer);
    }
    Super::NativeDestruct();
}

void UMobileCreatingRoomWidget::SetConnectionStage(const FString& ChineseStatus)
{
    bConnecting = true;
    Progress_CreatingRoom->SetPercent(0.94f);
    Txt_CreatingStatus->SetText(FText::FromString(
        ChineseStatus.IsEmpty() ? TEXT("服务器已就绪，正在进入房间……") : ChineseStatus));
    Txt_CreatingPercent->SetText(FText::FromString(TEXT("94%")));
}

void UMobileCreatingRoomWidget::RefreshProgress()
{
    if (bConnecting) return;
    const double Elapsed = FMath::Max(0.0, FPlatformTime::Seconds() - StartedAtSeconds);
    const float Percent = FMath::Clamp(
        0.08f + static_cast<float>(1.0 - FMath::Exp(-Elapsed / 7.0)) * 0.82f,
        0.08f,
        0.90f);
    Progress_CreatingRoom->SetPercent(Percent);
    Txt_CreatingPercent->SetText(FText::FromString(
        FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Percent * 100.0f))));
    const TCHAR* Status = Elapsed < 2.0
        ? TEXT("正在创建房间……")
        : Elapsed < 7.0
            ? TEXT("正在分配专用服务器……")
            : TEXT("正在启动牌桌，请稍候……");
    Txt_CreatingStatus->SetText(FText::FromString(Status));
}
