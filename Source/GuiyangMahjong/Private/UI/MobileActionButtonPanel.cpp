#include "UI/MobileActionButtonPanel.h"
#include "Game/GuiyangMahjongPlayerController.h"
#include "Components/Button.h"
#include "GuiyangMahjong.h"

void UMobileActionButtonPanel::NativeConstruct()
{
    Super::NativeConstruct();
    Btn_Hu->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleHu);
    Btn_Gang->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleGang);
    Btn_Peng->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePeng);
    Btn_Pass->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePass);
    ShowActions({});
}

void UMobileActionButtonPanel::ShowActions(const TArray<FMahjongAction>& Actions)
{
    CurrentActions = Actions;
    auto Has = [&Actions](const EMahjongActionType Type){ return Actions.ContainsByPredicate([Type](const FMahjongAction& A){ return A.Type == Type; }); };
    Btn_Hu->SetVisibility(Has(EMahjongActionType::Hu) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    Btn_Gang->SetVisibility((Has(EMahjongActionType::MingGang) || Has(EMahjongActionType::AnGang) || Has(EMahjongActionType::BuGang)) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    Btn_Peng->SetVisibility(Has(EMahjongActionType::Peng) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    Btn_Pass->SetVisibility(Actions.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    UE_LOG(LogMahjongUI, Log, TEXT("操作按钮面板刷新：服务端下发 %d 项"), Actions.Num());
}

void UMobileActionButtonPanel::SendAction(const EMahjongActionType Type)
{
    const FMahjongAction* Offered = CurrentActions.FindByPredicate([Type](const FMahjongAction& A)
    {
        if (Type != EMahjongActionType::MingGang) return A.Type == Type;
        return A.Type == EMahjongActionType::MingGang || A.Type == EMahjongActionType::AnGang || A.Type == EMahjongActionType::BuGang;
    });
    if (!Offered && Type != EMahjongActionType::Pass) return;
    FMahjongActionRequest Request;
    Request.Type = Offered ? Offered->Type : EMahjongActionType::Pass;
    Request.TargetTileId = Offered ? Offered->TargetTile.UniqueId : INDEX_NONE;
    Request.ClientSequence = ++ClientSequence;
    if (AGuiyangMahjongPlayerController* PC = Cast<AGuiyangMahjongPlayerController>(GetOwningPlayer())) PC->Server_RequestAction(Request);
    ShowActions({});
}

void UMobileActionButtonPanel::HandleHu(){ SendAction(EMahjongActionType::Hu); }
void UMobileActionButtonPanel::HandleGang(){ SendAction(EMahjongActionType::MingGang); }
void UMobileActionButtonPanel::HandlePeng(){ SendAction(EMahjongActionType::Peng); }
void UMobileActionButtonPanel::HandlePass(){ SendAction(EMahjongActionType::Pass); }
