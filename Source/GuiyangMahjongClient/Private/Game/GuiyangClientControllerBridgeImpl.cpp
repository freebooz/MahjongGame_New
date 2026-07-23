#include "Game/GuiyangClientControllerBridgeImpl.h"

#include "Auth/GuiyangLoginSubsystem.h"
#include "Camera/CameraActor.h"
#include "EngineUtils.h"
#include "Game/GuiyangMahjongGameState.h"
#include "Game/GuiyangMahjongPlayerController.h"
#include "Game/GuiyangMahjongPlayerState.h"
#include "Game/Mahjong3DTableActor.h"
#include "Game/MahjongRoomCameraActor.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "GuiyangMahjong.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "History/GuiyangMatchHistorySubsystem.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Lobby/GuiyangLobbySubsystem.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Network/GuiyangReconnectSubsystem.h"
#include "TimerManager.h"
#include "UI/MobileRootHUDWidget.h"
#include "UnrealClient.h"

namespace
{
    constexpr double MinimumCreatingRoomLoadingSeconds = 1.5;
    const FVector DefaultRoomCameraLocation(0.0f, -950.0f, 1320.0f);
}

UWorld* UGuiyangClientControllerBridgeImpl::GetWorld() const
{
    return Controller ? Controller->GetWorld() : nullptr;
}

void UGuiyangClientControllerBridgeImpl::InitializeClient(AGuiyangMahjongPlayerController& InController)
{
    Controller = &InController;
    const bool bUIReviewScreenshot = FParse::Param(FCommandLine::Get(), TEXT("UIReviewScreenshot"));
    if (!bUIReviewScreenshot)
    {
        if (const UGuiyangLoginSubsystem* Login = Controller->GetGameInstance()
            ? Controller->GetGameInstance()->GetSubsystem<UGuiyangLoginSubsystem>() : nullptr;
            Login && Login->IsSessionValid())
        {
            const FGuiyangLoginProfile& Profile = Login->GetCurrentProfile();
            Controller->Server_AuthenticateSession(Profile.PlayerId, Profile.DisplayName, Profile.Provider,
                Login->GetSessionTokenForNetwork());
        }
    }

    if (GetWorld() && GetWorld()->GetMapName().Contains(TEXT("MahjongRoomMap")))
    {
        EnsureRoomPresentation();
    }

    UClass* RootHUDClass = LoadClass<UMobileRootHUDWidget>(nullptr,
        TEXT("/Game/UI/Screens/WBP_RootHUD.WBP_RootHUD_C"));
    if (!RootHUDClass)
    {
        UE_LOG(LogMahjongUI, Error, TEXT("Unable to load WBP_RootHUD; client UI did not start"));
        return;
    }
    RootHUDInstance = CreateWidget<UMobileRootHUDWidget>(Controller, RootHUDClass);
    RootHUDInstance->AddToViewport(100);
    Controller->bShowMouseCursor = true;
    FInputModeGameAndUI InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    InputMode.SetHideCursorDuringCapture(false);
    Controller->SetInputMode(InputMode);

    if (!bUIReviewScreenshot)
    {
        return;
    }

    FString ReviewScreen = TEXT("Login");
    FParse::Value(FCommandLine::Get(), TEXT("UIReviewScreen="), ReviewScreen);
    if (!RootHUDInstance->ApplyVisualReviewScenario(ReviewScreen))
    {
        UE_LOG(LogMahjongUI, Error, TEXT("UI review scenario initialization failed: %s"), *ReviewScreen);
        return;
    }
    FString ReviewName = TEXT("UIReview");
    FParse::Value(FCommandLine::Get(), TEXT("UIReviewName="), ReviewName);
    ReviewName.ReplaceInline(TEXT("\\"), TEXT("/"));
    while (ReviewName.StartsWith(TEXT("/"))) ReviewName.RightChopInline(1);
    if (ReviewName.IsEmpty() || ReviewName.Contains(TEXT("..")) || !FPaths::IsRelative(ReviewName))
    {
        UE_LOG(LogMahjongUI, Error, TEXT("Unsafe UI review screenshot name rejected: %s"), *ReviewName);
        return;
    }
    if (!ReviewName.EndsWith(TEXT(".png"), ESearchCase::IgnoreCase)) ReviewName += TEXT(".png");
    const FString ScreenshotPath = FPaths::ProjectSavedDir() / TEXT("UIReview") / ReviewName;
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(ScreenshotPath), true);
    float CaptureDelaySeconds = 2.0f;
    FParse::Value(FCommandLine::Get(), TEXT("UIReviewDelaySeconds="), CaptureDelaySeconds);
    CaptureDelaySeconds = FMath::Clamp(CaptureDelaySeconds, 1.0f, 30.0f);
    FTimerHandle ScreenshotTimer;
    Controller->GetWorldTimerManager().SetTimer(ScreenshotTimer,
        FTimerDelegate::CreateWeakLambda(this, [this, ScreenshotPath]()
        {
            FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
            FTimerHandle ExitTimer;
            if (Controller)
            {
                Controller->GetWorldTimerManager().SetTimer(ExitTimer,
                    [] { FPlatformMisc::RequestExit(false); }, 1.0f, false);
            }
        }), CaptureDelaySeconds, false);
}

AActor* UGuiyangClientControllerBridgeImpl::EnsureRoomPresentation()
{
    if (!Controller || !Controller->IsLocalController() || !GetWorld()) return nullptr;
    if (!IsValid(RoomTableActor))
    {
        TActorIterator<AMahjong3DTableActor> It(GetWorld());
        if (It) RoomTableActor = *It;
        if (!RoomTableActor)
        {
            FActorSpawnParameters Parameters;
            Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            RoomTableActor = GetWorld()->SpawnActor<AMahjong3DTableActor>(
                AMahjong3DTableActor::StaticClass(), FTransform::Identity, Parameters);
        }
    }
    if (!IsValid(RoomCameraActor))
    {
        for (TActorIterator<ACameraActor> It(GetWorld()); It; ++It)
        {
            if (It->ActorHasTag(AMahjongRoomCameraActor::RoomCameraTag))
            {
                RoomCameraActor = *It;
                break;
            }
        }
        if (!RoomCameraActor)
        {
            const FRotator Rotation = (FVector::ZeroVector - DefaultRoomCameraLocation).Rotation();
            FActorSpawnParameters Parameters;
            Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            RoomCameraActor = GetWorld()->SpawnActor<AMahjongRoomCameraActor>(
                AMahjongRoomCameraActor::StaticClass(), DefaultRoomCameraLocation, Rotation, Parameters);
        }
    }
    if (RoomCameraActor && Controller->GetViewTarget() != RoomCameraActor)
    {
        Controller->SetViewTarget(RoomCameraActor);
    }
    return RoomTableActor;
}

void UGuiyangClientControllerBridgeImpl::ConnectToServer(
    const FString& ServerIP, const int32 Port, const FString& PlayerName)
{
    if (!Controller) return;
    const FString CleanIP = ServerIP.TrimStartAndEnd();
    const FString CleanName = PlayerName.TrimStartAndEnd();
    if (CleanIP.IsEmpty() || CleanName.IsEmpty() || CleanName.Len() > 24 || Port < 1 || Port > 65535)
    {
        Controller->Client_ShowErrorMessage(TEXT("服务器地址、端口或昵称格式不正确"));
        return;
    }
    Controller->SetPendingPlayerName(CleanName);
    if (UGuiyangReconnectSubsystem* Reconnect = Controller->GetGameInstance()
        ? Controller->GetGameInstance()->GetSubsystem<UGuiyangReconnectSubsystem>() : nullptr)
    {
        Reconnect->RememberConnection(CleanIP, Port, CleanName);
    }
    Controller->ClientTravel(FString::Printf(TEXT("%s:%d"), *CleanIP, Port), TRAVEL_Absolute);
}

void UGuiyangClientControllerBridgeImpl::ConnectToAllocatedServer(const FGuiyangGameServerRoute& Route)
{
    if (!Controller) return;
    const FString PlayerId = Route.PlayerId.TrimStartAndEnd();
    if (!Route.HasValidEndpoint() || PlayerId.IsEmpty() || PlayerId.Len() > 80
        || Route.JoinTicket.Len() < 32 || Route.JoinTicket.Len() > 4096
        || Route.TicketExpireAtUtc <= FDateTime::UtcNow())
    {
        CreatingRoomLoadingShownAtSeconds = 0.0;
        PendingAllocatedRoute = {};
        Controller->GetWorldTimerManager().ClearTimer(CreatingRoomTravelDelayTimer);
        if (RootHUDInstance) RootHUDInstance->ShowLobby();
        Controller->Client_ShowErrorMessage(TEXT("牌桌路由或入场票据无效"));
        return;
    }
    if (RootHUDInstance) RootHUDInstance->UpdateCreatingRoomStage(TEXT("服务器已就绪，正在进入房间……"));
    const double RemainingSeconds = MinimumCreatingRoomLoadingSeconds
        - (FPlatformTime::Seconds() - CreatingRoomLoadingShownAtSeconds);
    if (CreatingRoomLoadingShownAtSeconds > 0.0 && RemainingSeconds > 0.0)
    {
        PendingAllocatedRoute = Route;
        Controller->GetWorldTimerManager().ClearTimer(CreatingRoomTravelDelayTimer);
        Controller->GetWorldTimerManager().SetTimer(CreatingRoomTravelDelayTimer, this,
            &ThisClass::CompleteDelayedAllocatedServerConnection, static_cast<float>(RemainingSeconds), false);
        return;
    }
    CreatingRoomLoadingShownAtSeconds = 0.0;
    TravelToAllocatedServer(Route);
}

void UGuiyangClientControllerBridgeImpl::CompleteDelayedAllocatedServerConnection()
{
    FGuiyangGameServerRoute Route = MoveTemp(PendingAllocatedRoute);
    PendingAllocatedRoute = {};
    CreatingRoomLoadingShownAtSeconds = 0.0;
    TravelToAllocatedServer(MoveTemp(Route));
}

void UGuiyangClientControllerBridgeImpl::TravelToAllocatedServer(FGuiyangGameServerRoute Route)
{
    if (!Controller) return;
    FString ConnectServerIP = Route.ServerIP;
    FString Override;
    if (FParse::Value(FCommandLine::Get(), TEXT("MahjongGameServerHostOverride="), Override))
    {
        Override.TrimStartAndEndInline();
        FIPv4Address Address;
        if (FIPv4Address::Parse(Override, Address) && Address != FIPv4Address::Any) ConnectServerIP = Override;
    }
    if (UGuiyangReconnectSubsystem* Reconnect = Controller->GetGameInstance()
        ? Controller->GetGameInstance()->GetSubsystem<UGuiyangReconnectSubsystem>() : nullptr)
    {
        Reconnect->RememberRemoteRoute(Route.RoomId, Route.MatchId);
    }
    const FString URL = FString::Printf(TEXT("%s:%d?PlayerId=%s?JoinTicket=%s"),
        *ConnectServerIP, Route.ServerPort,
        *FGenericPlatformHttp::UrlEncode(Route.PlayerId.TrimStartAndEnd()),
        *FGenericPlatformHttp::UrlEncode(Route.JoinTicket));
    Controller->ClientTravel(URL, TRAVEL_Absolute);
}

void UGuiyangClientControllerBridgeImpl::RetryLastConnection()
{
    if (!Controller) return;
    UGuiyangReconnectSubsystem* Reconnect = Controller->GetGameInstance()
        ? Controller->GetGameInstance()->GetSubsystem<UGuiyangReconnectSubsystem>() : nullptr;
    UGuiyangLobbySubsystem* Lobby = Controller->GetGameInstance()
        ? Controller->GetGameInstance()->GetSubsystem<UGuiyangLobbySubsystem>() : nullptr;
    if (Reconnect && Lobby && Lobby->GetBackendMode() == EGuiyangLobbyBackendMode::RemoteLobby)
    {
        if (!Reconnect->CanRetry())
        {
            Controller->Client_ShowErrorMessage(TEXT("重连保留时间已结束或牌桌标识不可用"));
            return;
        }
        Reconnect->MarkRetrying();
        const FGuiyangLobbyOperationResult Result = Lobby->RequestReconnect(Controller);
        if (!Result.bAccepted) Reconnect->MarkRetryFailed(Result.ChineseMessage);
        return;
    }
    FString IP;
    FString Name;
    int32 Port = 7777;
    if (!Reconnect || !Reconnect->GetLastConnection(IP, Port, Name) || !Reconnect->CanRetry())
    {
        Controller->Client_ShowErrorMessage(TEXT("重连地址不可用或重连保留时间已结束"));
        return;
    }
    Reconnect->MarkRetrying();
    ConnectToServer(IP, Port, Name);
}

void UGuiyangClientControllerBridgeImpl::ReturnToConnectScreen()
{
    if (!Controller) return;
    if (UGuiyangReconnectSubsystem* Reconnect = Controller->GetGameInstance()
        ? Controller->GetGameInstance()->GetSubsystem<UGuiyangReconnectSubsystem>() : nullptr)
    {
        Reconnect->CancelReconnect();
    }
    const UGuiyangLobbySubsystem* Lobby = Controller->GetGameInstance()
        ? Controller->GetGameInstance()->GetSubsystem<UGuiyangLobbySubsystem>() : nullptr;
    if (RootHUDInstance)
    {
        if (Lobby && Lobby->GetBackendMode() == EGuiyangLobbyBackendMode::RemoteLobby) RootHUDInstance->ShowLobby();
        else RootHUDInstance->ShowConnectServer();
    }
}

void UGuiyangClientControllerBridgeImpl::ReturnToLobby()
{
    if (!Controller) return;
    UGuiyangLobbySubsystem* Lobby = Controller->GetGameInstance()
        ? Controller->GetGameInstance()->GetSubsystem<UGuiyangLobbySubsystem>() : nullptr;
    if (!Lobby || Lobby->GetBackendMode() == EGuiyangLobbyBackendMode::LocalLegacy)
    {
        Controller->Server_RequestLeaveRoom();
        return;
    }
    const AGuiyangMahjongGameState* State = GetWorld() ? GetWorld()->GetGameState<AGuiyangMahjongGameState>() : nullptr;
    const AGuiyangMahjongPlayerState* Player = Controller->GetPlayerState<AGuiyangMahjongPlayerState>();
    const bool bOwner = State && Player && State->RoomState.RoomInfo.OwnerPlayerId == Player->MahjongPlayerId;
    if (!bOwner)
    {
        CompleteRemoteReturnToLobby();
        return;
    }
    const FGuiyangLobbyOperationResult Result = Lobby->RequestCloseOwnedRoom(Controller);
    if (!Result.bAccepted) Controller->Client_ShowErrorMessage(Result.ChineseMessage);
}

void UGuiyangClientControllerBridgeImpl::ShowCreatingRoomLoading()
{
    if (!Controller) return;
    CreatingRoomLoadingShownAtSeconds = FPlatformTime::Seconds();
    PendingAllocatedRoute = {};
    Controller->GetWorldTimerManager().ClearTimer(CreatingRoomTravelDelayTimer);
    if (RootHUDInstance) RootHUDInstance->ShowCreatingRoom();
}

void UGuiyangClientControllerBridgeImpl::RequestCreateRoomWithLoading(const FMahjongCreateRoomRequest& Request)
{
    if (!Controller) return;
    ShowCreatingRoomLoading();
    Controller->GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, Request]()
    {
        if (!Controller) return;
        UGuiyangLobbySubsystem* Lobby = Controller->GetGameInstance()
            ? Controller->GetGameInstance()->GetSubsystem<UGuiyangLobbySubsystem>() : nullptr;
        if (!Lobby)
        {
            if (RootHUDInstance) RootHUDInstance->ShowLobby();
            Controller->Client_ShowErrorMessage(TEXT("大厅服务尚未初始化，请稍后重试"));
            return;
        }
        const FGuiyangLobbyOperationResult Result = Lobby->RequestCreateRoom(Controller, Request);
        if (!Result.bAccepted && RootHUDInstance) RootHUDInstance->ShowLobby();
    }));
}

void UGuiyangClientControllerBridgeImpl::CompleteRemoteReturnToLobby()
{
    if (!Controller) return;
    CreatingRoomLoadingShownAtSeconds = 0.0;
    PendingAllocatedRoute = {};
    Controller->GetWorldTimerManager().ClearTimer(CreatingRoomTravelDelayTimer);
    if (UGuiyangReconnectSubsystem* Reconnect = Controller->GetGameInstance()
        ? Controller->GetGameInstance()->GetSubsystem<UGuiyangReconnectSubsystem>() : nullptr)
    {
        Reconnect->CancelReconnect();
    }
    Controller->ClientTravel(TEXT("/Engine/Maps/Entry"), TRAVEL_Absolute);
}

void UGuiyangClientControllerBridgeImpl::NotifyReconnectRestored(const FMahjongReconnectSnapshot& Snapshot)
{
    if (Controller)
    {
        if (UGuiyangReconnectSubsystem* Reconnect = Controller->GetGameInstance()
            ? Controller->GetGameInstance()->GetSubsystem<UGuiyangReconnectSubsystem>() : nullptr)
        {
            Reconnect->MarkRestored();
        }
    }
}

void UGuiyangClientControllerBridgeImpl::NotifyFinalSettlement(const FMahjongFinalSettlementResult& Result)
{
    if (Controller)
    {
        if (UGuiyangMatchHistorySubsystem* History = Controller->GetGameInstance()
            ? Controller->GetGameInstance()->GetSubsystem<UGuiyangMatchHistorySubsystem>() : nullptr)
        {
            History->RecordFinalSettlement(Result);
        }
    }
}

void UGuiyangClientControllerBridgeImpl::HandleIntegrationPrivateState(const FMahjongPrivatePlayerState& PrivateState)
{
    // Automated integration remains opt-in and is intentionally excluded from production behavior.
}
