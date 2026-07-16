#include "UI/MobileSettlementWidget.h"
#include "Game/GuiyangMahjongPlayerController.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "GuiyangMahjong.h"

void UMobileSettlementWidget::NativeConstruct()
{
    Super::NativeConstruct();
    Btn_NextRound->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleNextRound);
    Btn_BackLobby->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBackLobby);
}

void UMobileSettlementWidget::SetSettlementResult(const FMahjongSettlementResult& Result)
{
    Txt_ResultTitle->SetText(FText::FromString(Result.bDrawGame ? TEXT("本局流局") : FString::Printf(TEXT("座位 %d 胡牌"), Result.WinnerSeat)));
    Txt_HuType->SetText(FText::FromString(Result.bSelfDraw ? TEXT("自摸") : TEXT("点炮")));
    FString JiSummary = Result.FlippedJiTile.IsValid()
        ? FString::Printf(TEXT("翻鸡牌：%s"), *Result.FlippedJiTile.ToDebugString())
        : TEXT("本局未翻鸡");
    for (int32 Seat = 0; Seat < Result.PlayerJiCounts.Num(); ++Seat)
        JiSummary += FString::Printf(TEXT("  座位%d：%d鸡"), Seat, Result.PlayerJiCounts[Seat]);
    Txt_JiResult->SetText(FText::FromString(JiSummary));
    Panel_PlayerScores->ClearChildren();
    for (const FMahjongPlayerScoreResult& Player : Result.PlayerResults)
    {
        UTextBlock* Row = NewObject<UTextBlock>(this);
        Row->SetText(FText::FromString(FString::Printf(TEXT("座位 %d　基础 %+d　鸡 %+d　特殊鸡 %+d　杠 %+d　合计 %+d"),
            Player.SeatIndex, Player.BaseScoreDelta, Player.JiScoreDelta,
            Player.SpecialJiScoreDelta, Player.GangScoreDelta, Player.TotalDelta)));
        Panel_PlayerScores->AddChildToVerticalBox(Row);
    }
    UE_LOG(LogMahjongUI, Log, TEXT("结算弹窗数据刷新完成"));
}

void UMobileSettlementWidget::HandleNextRound()
{
    if (AGuiyangMahjongPlayerController* PC = Cast<AGuiyangMahjongPlayerController>(GetOwningPlayer())) PC->Server_RequestNextRound();
}
void UMobileSettlementWidget::HandleBackLobby(){ RemoveFromParent(); }
