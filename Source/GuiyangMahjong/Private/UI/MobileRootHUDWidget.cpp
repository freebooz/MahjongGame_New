#include "UI/MobileRootHUDWidget.h"

#include "Auth/GuiyangLoginSubsystem.h"
#include "Components/Overlay.h"
#include "Game/GuiyangMahjongGameState.h"
#include "Game/GuiyangMahjongPlayerController.h"
#include "Game/GuiyangMahjongPlayerState.h"
#include "GuiyangMahjong.h"
#include "UI/MobileErrorToastWidget.h"
#include "UI/MobileLobbyWidget.h"
#include "UI/MobileMahjongHUDWidget.h"
#include "UI/MobileRoomWidget.h"

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
        PC->OnReconnectRestored.AddUniqueDynamic(this, &ThisClass::HandleReconnectRestored);
    }
    if (AGuiyangMahjongGameState* GS = GetWorld() ? GetWorld()->GetGameState<AGuiyangMahjongGameState>() : nullptr)
    {
        GS->OnRoomStateUpdated.AddUniqueDynamic(this, &ThisClass::HandleRoomStateUpdated);
        RouteFromRoomState(GS->RoomState);
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
        PC->OnReconnectRestored.RemoveDynamic(this, &ThisClass::HandleReconnectRestored);
    }
    if (AGuiyangMahjongGameState* GS = GetWorld() ? GetWorld()->GetGameState<AGuiyangMahjongGameState>() : nullptr)
    {
        GS->OnRoomStateUpdated.RemoveDynamic(this, &ThisClass::HandleRoomStateUpdated);
    }
    Super::NativeDestruct();
}

void UMobileRootHUDWidget::HandleLoginStateChanged(const EGuiyangLoginState State, const FGuiyangLoginProfile& Profile)
{
    if (State == EGuiyangLoginState::LoggedIn)
    {
        if (UGuiyangLoginSubsystem* Login = GetGameInstance()->GetSubsystem<UGuiyangLoginSubsystem>())
        {
            if (AGuiyangMahjongPlayerController* PC = Cast<AGuiyangMahjongPlayerController>(GetOwningPlayer()))
            {
                PC->Server_AuthenticateSession(Profile.PlayerId, Profile.DisplayName, Profile.Provider,
                    Login->GetSessionTokenForNetwork());
            }
        }
        ShowLobby();
        if (UMobileLobbyWidget* Lobby = Cast<UMobileLobbyWidget>(CurrentScreen))
        {
            Lobby->RefreshPlayerInfo(Profile.DisplayName, Profile.PlayerId, 1);
        }
        if (const AGuiyangMahjongGameState* GS = GetWorld() ? GetWorld()->GetGameState<AGuiyangMahjongGameState>() : nullptr)
        {
            RouteFromRoomState(GS->RoomState);
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
    if (UMobileLobbyWidget* Lobby = Cast<UMobileLobbyWidget>(
        ShowScreenByClassPath(TEXT("/Game/UI/Screens/WBP_Lobby.WBP_Lobby_C"))))
    {
        if (const UGuiyangLoginSubsystem* Login = GetGameInstance()->GetSubsystem<UGuiyangLoginSubsystem>())
        {
            const FGuiyangLoginProfile& Profile = Login->GetCurrentProfile();
            Lobby->RefreshPlayerInfo(Profile.DisplayName, Profile.PlayerId, 1);
        }
    }
}

void UMobileRootHUDWidget::ShowRoom(const FMahjongRoomState& State, const int32 LocalSeat)
{
    if (UMobileRoomWidget* Room = Cast<UMobileRoomWidget>(
        ShowScreenByClassPath(TEXT("/Game/UI/Screens/WBP_Room.WBP_Room_C"))))
    {
        Room->RefreshRoomState(State, LocalSeat);
    }
}

void UMobileRootHUDWidget::ShowGameHUD()
{
    ShowScreenByClassPath(TEXT("/Game/UI/Screens/WBP_GameHUD.WBP_GameHUD_C"));
}

UUserWidget* UMobileRootHUDWidget::ShowScreenByClassPath(const TCHAR* ClassPath)
{
    if (CurrentScreen && CurrentScreenClassPath == ClassPath)
    {
        return CurrentScreen;
    }
    UClass* ScreenClass = LoadClass<UUserWidget>(nullptr, ClassPath);
    if (!ScreenClass)
    {
        UE_LOG(LogMahjongUI, Error, TEXT("页面类加载失败：%s"), ClassPath);
        return nullptr;
    }
    ScreenLayer->ClearChildren();
    CurrentScreen = CreateWidget<UUserWidget>(GetOwningPlayer(), ScreenClass);
    if (!CurrentScreen)
    {
        UE_LOG(LogMahjongUI, Error, TEXT("页面实例创建失败：%s"), ClassPath);
        CurrentScreenClassPath.Reset();
        return nullptr;
    }
    CurrentScreenClassPath = ClassPath;
    ScreenLayer->AddChildToOverlay(CurrentScreen);
    return CurrentScreen;
}

int32 UMobileRootHUDWidget::FindLocalSeat(const FMahjongRoomState& State) const
{
    FString PlayerId;
    if (const UGuiyangLoginSubsystem* Login = GetGameInstance()->GetSubsystem<UGuiyangLoginSubsystem>())
    {
        PlayerId = Login->GetCurrentProfile().PlayerId;
    }
    if (!PlayerId.IsEmpty())
    {
        if (const FMahjongSeatInfo* Seat = State.Seats.FindByPredicate([&PlayerId](const FMahjongSeatInfo& Item)
        {
            return Item.bOccupied && Item.PlayerId == PlayerId;
        }))
        {
            return Seat->SeatIndex;
        }
    }
    if (const AGuiyangMahjongPlayerState* PlayerState = GetOwningPlayer()
        ? GetOwningPlayer()->GetPlayerState<AGuiyangMahjongPlayerState>() : nullptr)
    {
        if (!PlayerState->RoomCode.IsEmpty() && PlayerState->RoomCode == State.RoomInfo.RoomId)
        {
            return PlayerState->SeatIndex;
        }
    }
    return INDEX_NONE;
}

void UMobileRootHUDWidget::RouteFromRoomState(const FMahjongRoomState& State)
{
    const UGuiyangLoginSubsystem* Login = GetGameInstance()->GetSubsystem<UGuiyangLoginSubsystem>();
    if (!Login || !Login->IsSessionValid())
    {
        ShowLogin();
        return;
    }

    const int32 LocalSeat = FindLocalSeat(State);
    if (LocalSeat == INDEX_NONE)
    {
        ShowLobby();
        return;
    }

    switch (State.Lifecycle)
    {
    case EMahjongRoomLifecycle::Creating:
    case EMahjongRoomLifecycle::WaitingForPlayers:
    case EMahjongRoomLifecycle::ReadyCheck:
    case EMahjongRoomLifecycle::Starting:
        ShowRoom(State, LocalSeat);
        break;
    case EMahjongRoomLifecycle::Playing:
    case EMahjongRoomLifecycle::Settlement:
    case EMahjongRoomLifecycle::WaitingNextRound:
        ShowGameHUD();
        break;
    case EMahjongRoomLifecycle::Closing:
    case EMahjongRoomLifecycle::Closed:
    default:
        ShowLobby();
        break;
    }
}

void UMobileRootHUDWidget::HandleRoomStateUpdated(const FMahjongRoomState& State)
{
    RouteFromRoomState(State);
}

void UMobileRootHUDWidget::HandleReconnectRestored(const FMahjongReconnectSnapshot& Snapshot)
{
    RouteFromRoomState(Snapshot.RoomState);
    if (UMobileMahjongHUDWidget* HUD = Cast<UMobileMahjongHUDWidget>(CurrentScreen))
    {
        HUD->RefreshTableState(Snapshot.TableState);
        HUD->RefreshPrivateHand(Snapshot.PrivateState);
    }
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
