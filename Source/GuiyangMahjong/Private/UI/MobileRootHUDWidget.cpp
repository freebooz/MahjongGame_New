#include "UI/MobileRootHUDWidget.h"

#include "Auth/GuiyangLoginSubsystem.h"
#include "Components/Overlay.h"
#include "Game/GuiyangMahjongPlayerController.h"
#include "GuiyangMahjong.h"
#include "UI/MobileErrorToastWidget.h"
#include "UI/MobileLobbyWidget.h"

void UMobileRootHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (UGuiyangLoginSubsystem* Login = GetGameInstance()->GetSubsystem<UGuiyangLoginSubsystem>())
    {
        Login->OnLoginStateChanged.AddUniqueDynamic(this, &ThisClass::HandleLoginStateChanged);
        Login->OnLoginFailed.AddUniqueDynamic(this, &ThisClass::HandleLoginFailed);
        if (Login->IsSessionValid()) ShowLobby(); else ShowLogin();
    }
    else
    {
        ShowLogin();
    }
    if (AGuiyangMahjongPlayerController* PC = Cast<AGuiyangMahjongPlayerController>(GetOwningPlayer()))
    {
        PC->OnErrorShown.AddUniqueDynamic(this, &ThisClass::HandleLoginFailed);
    }
    UE_LOG(LogMahjongUI, Log, TEXT("全局 RootHUD 创建完成"));
}

void UMobileRootHUDWidget::NativeDestruct()
{
    if (UGuiyangLoginSubsystem* Login = GetGameInstance()->GetSubsystem<UGuiyangLoginSubsystem>())
    {
        Login->OnLoginStateChanged.RemoveDynamic(this, &ThisClass::HandleLoginStateChanged);
        Login->OnLoginFailed.RemoveDynamic(this, &ThisClass::HandleLoginFailed);
    }
    if (AGuiyangMahjongPlayerController* PC = Cast<AGuiyangMahjongPlayerController>(GetOwningPlayer()))
    {
        PC->OnErrorShown.RemoveDynamic(this, &ThisClass::HandleLoginFailed);
    }
    Super::NativeDestruct();
}

void UMobileRootHUDWidget::HandleLoginStateChanged(const EGuiyangLoginState State, const FGuiyangLoginProfile& Profile)
{
    if (State == EGuiyangLoginState::LoggedIn)
    {
        ShowLobby();
        if (UMobileLobbyWidget* Lobby = Cast<UMobileLobbyWidget>(CurrentScreen))
        {
            Lobby->RefreshPlayerInfo(Profile.DisplayName, Profile.PlayerId, 1);
        }
    }
    else if (State == EGuiyangLoginState::LoggedOut || State == EGuiyangLoginState::Expired)
    {
        ShowLogin();
    }
}

void UMobileRootHUDWidget::HandleLoginFailed(const FString& ChineseReason)
{
    ShowChineseError(ChineseReason);
}

void UMobileRootHUDWidget::ShowLogin()
{
    ShowScreenByClassPath(TEXT("/Game/UI/Screens/WBP_Login.WBP_Login_C"));
}

void UMobileRootHUDWidget::ShowLobby()
{
    ShowScreenByClassPath(TEXT("/Game/UI/Screens/WBP_Lobby.WBP_Lobby_C"));
}

void UMobileRootHUDWidget::ShowScreenByClassPath(const TCHAR* ClassPath)
{
    UClass* ScreenClass = LoadClass<UUserWidget>(nullptr, ClassPath);
    if (!ScreenClass)
    {
        UE_LOG(LogMahjongUI, Error, TEXT("页面类加载失败：%s"), ClassPath);
        return;
    }
    ScreenLayer->ClearChildren();
    CurrentScreen = CreateWidget<UUserWidget>(GetOwningPlayer(), ScreenClass);
    ScreenLayer->AddChildToOverlay(CurrentScreen);
}

void UMobileRootHUDWidget::ShowChineseError(const FString& ChineseReason)
{
    if (!ErrorToastInstance)
    {
        UClass* ErrorClass = LoadClass<UMobileErrorToastWidget>(nullptr, TEXT("/Game/UI/Components/WBP_ErrorToast.WBP_ErrorToast_C"));
        if (ErrorClass)
        {
            ErrorToastInstance = CreateWidget<UMobileErrorToastWidget>(GetOwningPlayer(), ErrorClass);
            PopupLayer->AddChildToOverlay(ErrorToastInstance);
        }
    }
    if (ErrorToastInstance) ErrorToastInstance->ShowToast(ChineseReason, 3.0f);
}
