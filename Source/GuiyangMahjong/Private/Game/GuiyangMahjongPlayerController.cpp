#include "Game/GuiyangMahjongPlayerController.h"
#include "GuiyangMahjong.h"
#include "UI/MobileRootHUDWidget.h"
#include "Game/GuiyangMahjongGameMode.h"
#include "Game/GuiyangMahjongGameState.h"
#include "Game/GuiyangMahjongPlayerState.h"
#include "Auth/GuiyangLoginSubsystem.h"
#include "History/GuiyangMatchHistorySubsystem.h"
#include "Lobby/GuiyangLobbySubsystem.h"
#include "Network/GuiyangReconnectSubsystem.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Engine/NetConnection.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Interfaces/IPv4/IPv4Address.h"

namespace
{
    bool GIntegrationDisconnectTriggered = false;
    constexpr double MinimumCreatingRoomLoadingSeconds = 1.5;

    bool IsClientFullMatchIntegrationEnabled()
    {
        return FParse::Param(FCommandLine::Get(), TEXT("MahjongEnableIntegrationHooks"))
            && FParse::Param(FCommandLine::Get(), TEXT("MahjongIntegrationFullMatch"));
    }
}

void AGuiyangMahjongPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController() || IsRunningDedicatedServer())
    {
        return;
    }

    const bool bUIReviewScreenshot = FParse::Param(FCommandLine::Get(), TEXT("UIReviewScreenshot"));
    if (!bUIReviewScreenshot)
    {
        if (const UGuiyangLoginSubsystem* Login = GetGameInstance()
            ? GetGameInstance()->GetSubsystem<UGuiyangLoginSubsystem>() : nullptr)
        {
            if (Login->IsSessionValid())
            {
                const FGuiyangLoginProfile& Profile = Login->GetCurrentProfile();
                Server_AuthenticateSession(Profile.PlayerId, Profile.DisplayName, Profile.Provider,
                    Login->GetSessionTokenForNetwork());
            }
        }
    }

    UClass* RootHUDClass = LoadClass<UMobileRootHUDWidget>(nullptr, TEXT("/Game/UI/Screens/WBP_RootHUD.WBP_RootHUD_C"));
    if (!RootHUDClass)
    {
        UE_LOG(LogMahjongUI, Error, TEXT("无法加载 WBP_RootHUD，客户端 UI 未启动"));
        return;
    }
    RootHUDInstance = CreateWidget<UMobileRootHUDWidget>(this, RootHUDClass);
    RootHUDInstance->AddToViewport(100);
    bShowMouseCursor = true;
    FInputModeGameAndUI InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    InputMode.SetHideCursorDuringCapture(false);
    SetInputMode(InputMode);
    UE_LOG(LogMahjongUI, Log, TEXT("本地客户端 RootHUD 已加入视口"));

    if (bUIReviewScreenshot)
    {
        FString ReviewScreen = TEXT("Login");
        FParse::Value(FCommandLine::Get(), TEXT("UIReviewScreen="), ReviewScreen);
        if (!RootHUDInstance->ApplyVisualReviewScenario(ReviewScreen))
        {
            UE_LOG(LogMahjongUI, Error, TEXT("UI 审查场景初始化失败：%s"), *ReviewScreen);
            return;
        }
        FString ReviewName = TEXT("UIReview");
        FParse::Value(FCommandLine::Get(), TEXT("UIReviewName="), ReviewName);
        ReviewName.ReplaceInline(TEXT("\\"), TEXT("/"));
        while (ReviewName.StartsWith(TEXT("/")))
        {
            ReviewName.RightChopInline(1);
        }
        if (ReviewName.IsEmpty() || ReviewName.Contains(TEXT("..")) || !FPaths::IsRelative(ReviewName))
        {
            UE_LOG(LogMahjongUI, Error, TEXT("UI 审查截图名称不安全，已拒绝：%s"), *ReviewName);
            return;
        }
        if (!ReviewName.EndsWith(TEXT(".png"), ESearchCase::IgnoreCase))
        {
            ReviewName += TEXT(".png");
        }
        const FString ReviewDirectory = FPaths::ProjectSavedDir() / TEXT("UIReview");
        const FString ScreenshotPath = ReviewDirectory / ReviewName;
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(ScreenshotPath), true);
        float CaptureDelaySeconds = 2.0f;
        FParse::Value(FCommandLine::Get(), TEXT("UIReviewDelaySeconds="), CaptureDelaySeconds);
        CaptureDelaySeconds = FMath::Clamp(CaptureDelaySeconds, 1.0f, 30.0f);
        FTimerHandle ScreenshotTimer;
        GetWorldTimerManager().SetTimer(ScreenshotTimer, FTimerDelegate::CreateWeakLambda(this, [this, ScreenshotPath]()
        {
            FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
            UE_LOG(LogMahjongUI, Log, TEXT("UI 审查截图已请求：%s"), *ScreenshotPath);
            FTimerHandle ExitTimer;
            GetWorldTimerManager().SetTimer(ExitTimer, [] { FPlatformMisc::RequestExit(false); }, 1.0f, false);
        }), CaptureDelaySeconds, false);
        return;
    }

    InitializeIntegrationClient();
}

void AGuiyangMahjongPlayerController::Server_AuthenticateSession_Implementation(const FString& PlayerId,
    const FString& DisplayName, const EGuiyangLoginProvider Provider, const FString& SessionToken)
{
    if (AGuiyangMahjongGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AGuiyangMahjongGameMode>() : nullptr)
        Mode->HandleAuthenticateSession(this, PlayerId, DisplayName, Provider, SessionToken);
}

void AGuiyangMahjongPlayerController::ConnectToServer(const FString& ServerIP, const int32 Port, const FString& PlayerName)
{
    const FString CleanIP = ServerIP.TrimStartAndEnd();
    const FString CleanName = PlayerName.TrimStartAndEnd();
    if (CleanIP.IsEmpty() || CleanName.IsEmpty() || CleanName.Len() > 24 || Port < 1 || Port > 65535)
    {
        Client_ShowErrorMessage(TEXT("服务器地址、端口或昵称格式不正确"));
        return;
    }
    PendingPlayerName = CleanName;
    if (UGuiyangReconnectSubsystem* Reconnect = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UGuiyangReconnectSubsystem>() : nullptr)
    {
        Reconnect->RememberConnection(CleanIP, Port, CleanName);
    }
    const FString TravelURL = FString::Printf(TEXT("%s:%d"), *CleanIP, Port);
    UE_LOG(LogMahjongNet, Log, TEXT("客户端准备连接服务器：地址=%s，玩家=%s"), *TravelURL, *CleanName);
    ClientTravel(TravelURL, TRAVEL_Absolute);
}

void AGuiyangMahjongPlayerController::ConnectToAllocatedServer(const FGuiyangGameServerRoute& Route)
{
    const FString PlayerId = Route.PlayerId.TrimStartAndEnd();
    if (!Route.HasValidEndpoint() || PlayerId.IsEmpty() || PlayerId.Len() > 80
        || Route.JoinTicket.Len() < 32 || Route.JoinTicket.Len() > 4096
        || Route.TicketExpireAtUtc <= FDateTime::UtcNow())
    {
        CreatingRoomLoadingShownAtSeconds = 0.0;
        PendingAllocatedRoute = {};
        GetWorldTimerManager().ClearTimer(CreatingRoomTravelDelayTimer);
        if (RootHUDInstance)
        {
            RootHUDInstance->ShowLobby();
        }
        Client_ShowErrorMessage(TEXT("牌桌路由或入场票据无效"));
        return;
    }
    if (RootHUDInstance)
    {
        RootHUDInstance->UpdateCreatingRoomStage(TEXT("服务器已就绪，正在进入房间……"));
    }
    if (CreatingRoomLoadingShownAtSeconds > 0.0)
    {
        const double VisibleSeconds = FPlatformTime::Seconds() - CreatingRoomLoadingShownAtSeconds;
        const double RemainingSeconds = MinimumCreatingRoomLoadingSeconds - VisibleSeconds;
        if (RemainingSeconds > 0.0)
        {
            PendingAllocatedRoute = Route;
            GetWorldTimerManager().ClearTimer(CreatingRoomTravelDelayTimer);
            GetWorldTimerManager().SetTimer(CreatingRoomTravelDelayTimer, this,
                &ThisClass::CompleteDelayedAllocatedServerConnection,
                static_cast<float>(RemainingSeconds), false);
            return;
        }
        CreatingRoomLoadingShownAtSeconds = 0.0;
    }
    TravelToAllocatedServer(Route);
}

void AGuiyangMahjongPlayerController::CompleteDelayedAllocatedServerConnection()
{
    FGuiyangGameServerRoute Route = MoveTemp(PendingAllocatedRoute);
    PendingAllocatedRoute = {};
    CreatingRoomLoadingShownAtSeconds = 0.0;
    TravelToAllocatedServer(MoveTemp(Route));
}

void AGuiyangMahjongPlayerController::TravelToAllocatedServer(FGuiyangGameServerRoute Route)
{
    const FString PlayerId = Route.PlayerId.TrimStartAndEnd();
    FString ConnectServerIP = Route.ServerIP;
    FString ConfiguredHostOverride;
    if (FParse::Value(FCommandLine::Get(), TEXT("MahjongGameServerHostOverride="), ConfiguredHostOverride))
    {
        ConfiguredHostOverride.TrimStartAndEndInline();
        FIPv4Address OverrideAddress;
        if (FIPv4Address::Parse(ConfiguredHostOverride, OverrideAddress)
            && OverrideAddress != FIPv4Address::Any)
        {
            ConnectServerIP = ConfiguredHostOverride;
        }
        else
        {
            UE_LOG(LogMahjongNet, Warning,
                TEXT("忽略无效的本机牌桌地址覆盖：%s"), *ConfiguredHostOverride);
        }
    }
    const FString TravelURL = FString::Printf(TEXT("%s:%d?PlayerId=%s?JoinTicket=%s"),
        *ConnectServerIP, Route.ServerPort,
        *FGenericPlatformHttp::UrlEncode(PlayerId),
        *FGenericPlatformHttp::UrlEncode(Route.JoinTicket));
    UE_LOG(LogMahjongNet, Log,
        TEXT("客户端准备连接已分配牌桌：InstanceId=%s RoomId=%s Endpoint=%s:%d Advertised=%s:%d"),
        *Route.ServerInstanceId, *Route.RoomId, *ConnectServerIP, Route.ServerPort,
        *Route.ServerIP, Route.ServerPort);
    if (UGuiyangReconnectSubsystem* Reconnect = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UGuiyangReconnectSubsystem>() : nullptr)
        Reconnect->RememberRemoteRoute(Route.RoomId, Route.MatchId);
    ClientTravel(TravelURL, TRAVEL_Absolute);
}

void AGuiyangMahjongPlayerController::RetryLastConnection()
{
    UGuiyangReconnectSubsystem* Reconnect = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UGuiyangReconnectSubsystem>() : nullptr;
    UGuiyangLobbySubsystem* Lobby = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UGuiyangLobbySubsystem>() : nullptr;
    if (Reconnect && Lobby && Lobby->GetBackendMode() == EGuiyangLobbyBackendMode::RemoteLobby)
    {
        if (!Reconnect->CanRetry())
        {
            Client_ShowErrorMessage(TEXT("重连保留时间已结束或牌桌标识不可用"));
            return;
        }
        Reconnect->MarkRetrying();
        UE_LOG(LogMahjongReconnect, Log, TEXT("客户端向远程大厅申请新的重连票据"));
        const FGuiyangLobbyOperationResult Result = Lobby->RequestReconnect(this);
        if (!Result.bAccepted) Reconnect->MarkRetryFailed(Result.ChineseMessage);
        return;
    }
    FString ServerIP;
    FString PlayerName;
    int32 ServerPort = 7777;
    if (!Reconnect || !Reconnect->GetLastConnection(ServerIP, ServerPort, PlayerName)
        || !Reconnect->CanRetry())
    {
        Client_ShowErrorMessage(TEXT("重连地址不可用或重连保留时间已结束"));
        return;
    }
    Reconnect->MarkRetrying();
    UE_LOG(LogMahjongReconnect, Log, TEXT("客户端尝试重连：%s:%d"), *ServerIP, ServerPort);
    ConnectToServer(ServerIP, ServerPort, PlayerName);
}

void AGuiyangMahjongPlayerController::ReturnToConnectScreen()
{
    if (UGuiyangReconnectSubsystem* Reconnect = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UGuiyangReconnectSubsystem>() : nullptr)
    {
        Reconnect->CancelReconnect();
    }
    const UGuiyangLobbySubsystem* Lobby = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UGuiyangLobbySubsystem>() : nullptr;
    if (RootHUDInstance)
    {
        if (Lobby && Lobby->GetBackendMode() == EGuiyangLobbyBackendMode::RemoteLobby)
            RootHUDInstance->ShowLobby();
        else
            RootHUDInstance->ShowConnectServer();
    }
}

void AGuiyangMahjongPlayerController::ReturnToLobby()
{
    UGuiyangLobbySubsystem* Lobby = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UGuiyangLobbySubsystem>() : nullptr;
    if (!Lobby || Lobby->GetBackendMode() == EGuiyangLobbyBackendMode::LocalLegacy)
    {
        Server_RequestLeaveRoom();
        return;
    }
    const AGuiyangMahjongGameState* MahjongGameState = GetWorld()
        ? GetWorld()->GetGameState<AGuiyangMahjongGameState>() : nullptr;
    const AGuiyangMahjongPlayerState* MahjongPlayerState = GetPlayerState<AGuiyangMahjongPlayerState>();
    const bool bIsRoomOwner = MahjongGameState && MahjongPlayerState
        && !MahjongGameState->RoomState.RoomInfo.OwnerPlayerId.IsEmpty()
        && MahjongGameState->RoomState.RoomInfo.OwnerPlayerId == MahjongPlayerState->MahjongPlayerId;
    if (!bIsRoomOwner)
    {
        UE_LOG(LogMahjongNet, Log, TEXT("非房主返回远程大厅，保留房间以便再次进入"));
        CompleteRemoteReturnToLobby();
        return;
    }

    UE_LOG(LogMahjongNet, Log, TEXT("房主返回大厅，正在关闭并释放远程房间"));
    const FGuiyangLobbyOperationResult Result = Lobby->RequestCloseOwnedRoom(this);
    if (!Result.bAccepted)
    {
        Client_ShowErrorMessage(Result.ChineseMessage);
    }
}

void AGuiyangMahjongPlayerController::ShowCreatingRoomLoading()
{
    CreatingRoomLoadingShownAtSeconds = FPlatformTime::Seconds();
    PendingAllocatedRoute = {};
    GetWorldTimerManager().ClearTimer(CreatingRoomTravelDelayTimer);
    if (RootHUDInstance)
    {
        RootHUDInstance->ShowCreatingRoom();
    }
}

void AGuiyangMahjongPlayerController::CompleteRemoteReturnToLobby()
{
    CreatingRoomLoadingShownAtSeconds = 0.0;
    PendingAllocatedRoute = {};
    GetWorldTimerManager().ClearTimer(CreatingRoomTravelDelayTimer);
    if (UGuiyangReconnectSubsystem* Reconnect = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UGuiyangReconnectSubsystem>() : nullptr)
    {
        Reconnect->CancelReconnect();
    }
    UE_LOG(LogMahjongNet, Log, TEXT("远程房间离开处理完成，返回大厅"));
    ClientTravel(TEXT("/Engine/Maps/Entry"), TRAVEL_Absolute);
}

void AGuiyangMahjongPlayerController::RequestTableAction(const EMahjongActionType Type, const int32 TargetTileId)
{
    if (Type == EMahjongActionType::Draw)
    {
        OnErrorShown.Broadcast(TEXT("摸牌只能由服务端发起"));
        return;
    }
    const AGuiyangMahjongGameState* GS = GetWorld() ? GetWorld()->GetGameState<AGuiyangMahjongGameState>() : nullptr;
    if (!GS || GS->PublicTableState.RoundId <= 0 || GS->PublicTableState.TurnId <= 0)
    {
        OnErrorShown.Broadcast(TEXT("牌局状态尚未同步，请稍后重试"));
        return;
    }
    FMahjongActionRequest Request;
    Request.Type = Type;
    Request.RoundId = GS->PublicTableState.RoundId;
    Request.TurnId = GS->PublicTableState.TurnId;
    Request.TargetTileId = TargetTileId;
    Request.ClientSequence = ++LastClientActionSequence;
    Server_RequestAction(Request);
}

void AGuiyangMahjongPlayerController::Server_RequestCreateRoom_Implementation()
{
    Server_RequestCreateRoomWithConfig_Implementation(FMahjongCreateRoomRequest());
    UE_LOG(LogMahjongServer, Log, TEXT("收到创建房间请求：控制器=%s"), *GetName());
}

void AGuiyangMahjongPlayerController::Server_RequestQuickStart_Implementation()
{
    if (AGuiyangMahjongGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AGuiyangMahjongGameMode>() : nullptr)
    {
        Mode->HandleQuickStart(this);
    }
}

void AGuiyangMahjongPlayerController::Server_RequestCreateRoomWithConfig_Implementation(const FMahjongCreateRoomRequest& Request)
{
    if (AGuiyangMahjongGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AGuiyangMahjongGameMode>() : nullptr)
    {
        Mode->HandleCreateRoom(this, Request);
    }
}

void AGuiyangMahjongPlayerController::Server_RequestJoinRoom_Implementation(const FString& PlayerName)
{
    if (AGuiyangMahjongGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AGuiyangMahjongGameMode>() : nullptr)
    {
        Mode->HandleQuickStart(this);
    }
}

void AGuiyangMahjongPlayerController::Server_RequestReady_Implementation()
{
    if (AGuiyangMahjongGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AGuiyangMahjongGameMode>() : nullptr) Mode->HandleToggleReady(this);
    UE_LOG(LogMahjongServer, Log, TEXT("收到玩家准备请求：控制器=%s"), *GetName());
}

void AGuiyangMahjongPlayerController::Server_RequestLeaveRoom_Implementation()
{
    if (AGuiyangMahjongGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AGuiyangMahjongGameMode>() : nullptr) Mode->HandleLeaveRoom(this);
    UE_LOG(LogMahjongServer, Log, TEXT("收到玩家离开房间请求：控制器=%s"), *GetName());
}

void AGuiyangMahjongPlayerController::Server_RequestJoinRoomByCode_Implementation(const FMahjongJoinRoomRequest& Request)
{
    if (AGuiyangMahjongGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AGuiyangMahjongGameMode>() : nullptr)
    {
        Mode->HandleJoinRoom(this, Request);
    }
}

void AGuiyangMahjongPlayerController::Server_RequestNextRound_Implementation()
{
    if (AGuiyangMahjongGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AGuiyangMahjongGameMode>() : nullptr)
        Mode->HandleNextRound(this);
    UE_LOG(LogMahjongServer, Log, TEXT("收到下一局请求：控制器=%s"), *GetName());
}

void AGuiyangMahjongPlayerController::Server_RequestPlayTile_Implementation(const FMahjongTile Tile)
{
    if (!Tile.IsValid())
    {
        UE_LOG(LogMahjongNet, Warning, TEXT("RPC拒绝：请求打出的牌无效"));
        Client_ShowErrorMessage(TEXT("出牌请求无效"));
        return;
    }
    if (AGuiyangMahjongGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AGuiyangMahjongGameMode>() : nullptr)
    {
        Mode->HandleLegacyPlayTile(this, Tile, ++LastClientActionSequence);
        return;
    }
    UE_LOG(LogMahjongServer, Log, TEXT("收到出牌请求：%s"), *Tile.ToDebugString());
}

void AGuiyangMahjongPlayerController::Server_RequestAction_Implementation(const FMahjongActionRequest Request)
{
    if (AGuiyangMahjongGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AGuiyangMahjongGameMode>() : nullptr)
    {
        LastClientActionSequence = FMath::Max(LastClientActionSequence, Request.ClientSequence);
        Mode->HandleTableAction(this, Request);
        return;
    }
    if (Request.ClientSequence <= LastClientActionSequence || Request.Type == EMahjongActionType::Draw)
    {
        UE_LOG(LogMahjongNet, Warning, TEXT("RPC拒绝：操作序号重复或客户端请求了服务端专属摸牌操作，序号=%d"), Request.ClientSequence);
        Client_ShowErrorMessage(TEXT("操作已过期或不允许由客户端发起"));
        return;
    }
    LastClientActionSequence = Request.ClientSequence;
    UE_LOG(LogMahjongServer, Log, TEXT("收到玩家操作请求：类型=%d，序号=%d"), static_cast<int32>(Request.Type), Request.ClientSequence);
}

void AGuiyangMahjongPlayerController::Server_RequestIntegrationDisconnect_Implementation()
{
#if !UE_BUILD_SHIPPING
    const AGuiyangMahjongPlayerState* MahjongPlayer = GetPlayerState<AGuiyangMahjongPlayerState>();
    if (!FParse::Param(FCommandLine::Get(), TEXT("MahjongEnableIntegrationHooks"))
        || !MahjongPlayer || !MahjongPlayer->MahjongPlayerId.StartsWith(TEXT("integration-client-")))
    {
        UE_LOG(LogMahjongReconnect, Warning, TEXT("拒绝未授权的集成断线请求"));
        return;
    }
    UE_LOG(LogMahjongReconnect, Display, TEXT("MAHJONG_INTEGRATION_DISCONNECT_TRIGGERED Player=%s Seat=%d"),
        *MahjongPlayer->MahjongPlayerId, MahjongPlayer->SeatIndex);
    GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        if (UNetConnection* Connection = GetNetConnection()) Connection->Close();
    }));
#endif
}

void AGuiyangMahjongPlayerController::Client_UpdatePrivateHand_Implementation(const FMahjongPrivatePlayerState& PrivateState)
{
    LastClientActionSequence = FMath::Max(LastClientActionSequence, PrivateState.LastAcceptedClientSequence);
    UE_LOG(LogMahjongNet, Log, TEXT("收到私有手牌：座位=%d，牌数=%d，序号=%d"), PrivateState.SeatIndex, PrivateState.Hand.Tiles.Num(), PrivateState.StateSequence);
    OnPrivateHandUpdated.Broadcast(PrivateState);
    HandleIntegrationPrivateState(PrivateState);
}

void AGuiyangMahjongPlayerController::Client_ShowAvailableActions_Implementation(const TArray<FMahjongAction>& Actions)
{
    UE_LOG(LogMahjongUI, Log, TEXT("刷新可操作列表：%d 项"), Actions.Num());
    OnAvailableActionsUpdated.Broadcast(Actions);
}

void AGuiyangMahjongPlayerController::Client_ShowSettlement_Implementation(const FMahjongSettlementResult& Result)
{
    UE_LOG(LogMahjongUI, Log, TEXT("显示单局结算：%s"), *Result.ToDebugString());
    OnSettlementShown.Broadcast(Result);
}

void AGuiyangMahjongPlayerController::Client_RestoreReconnectSnapshot_Implementation(
    const FMahjongReconnectSnapshot& Snapshot, const TArray<FMahjongAction>& AvailableActions)
{
    LastClientActionSequence = Snapshot.PrivateState.LastAcceptedClientSequence;
    if (UGuiyangReconnectSubsystem* Reconnect = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UGuiyangReconnectSubsystem>() : nullptr)
    {
        Reconnect->MarkRestored();
    }
    OnReconnectRestored.Broadcast(Snapshot);
    OnPrivateHandUpdated.Broadcast(Snapshot.PrivateState);
    OnAvailableActionsUpdated.Broadcast(AvailableActions);
    UE_LOG(LogMahjongReconnect, Log, TEXT("重连快照恢复完成：Room=%s，Seat=%d，Round=%d"),
        *Snapshot.RoomState.RoomInfo.RoomId, Snapshot.PrivateState.SeatIndex, Snapshot.TableState.RoundId);
    if (IntegrationClientIndex != INDEX_NONE)
    {
        UE_LOG(LogMahjongReconnect, Display,
            TEXT("MAHJONG_INTEGRATION_CLIENT_RESTORED Client=%d Seat=%d Hand=%d Round=%d Remaining=%d"),
            IntegrationClientIndex, Snapshot.PrivateState.SeatIndex, Snapshot.PrivateState.Hand.Tiles.Num(),
            Snapshot.TableState.RoundId, Snapshot.RemainingReconnectSeconds);
    }
}

void AGuiyangMahjongPlayerController::Client_ShowFinalSettlement_Implementation(
    const FMahjongFinalSettlementResult& Result)
{
    if (UGuiyangMatchHistorySubsystem* History = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UGuiyangMatchHistorySubsystem>() : nullptr)
        History->RecordFinalSettlement(Result);
    OnFinalSettlementShown.Broadcast(Result);
    UE_LOG(LogMahjongUI, Log, TEXT("最终大结算已显示：Room=%s，Rounds=%d"),
        *Result.RoomId, Result.CompletedRounds);
#if !UE_BUILD_SHIPPING
    if (IntegrationClientIndex != INDEX_NONE && IsClientFullMatchIntegrationEnabled())
    {
        UE_LOG(LogMahjongNet, Display,
            TEXT("MAHJONG_INTEGRATION_FINAL_SETTLEMENT Client=%d Room=%s Rounds=%d Players=%d"),
            IntegrationClientIndex, *Result.RoomId, Result.CompletedRounds, Result.Players.Num());
    }
#endif
}

void AGuiyangMahjongPlayerController::Client_ShowErrorMessage_Implementation(const FString& Message)
{
    UE_LOG(LogMahjongUI, Warning, TEXT("显示中文错误提示：%s"), *Message);
    OnErrorShown.Broadcast(Message);
}

void AGuiyangMahjongPlayerController::InitializeIntegrationClient()
{
#if !UE_BUILD_SHIPPING
    if (!FParse::Param(FCommandLine::Get(), TEXT("MahjongEnableIntegrationHooks"))
        || !FParse::Value(FCommandLine::Get(), TEXT("MahjongIntegrationClient="), IntegrationClientIndex)
        || IntegrationClientIndex < 0 || IntegrationClientIndex > 3)
    {
        IntegrationClientIndex = INDEX_NONE;
        return;
    }

    FString Endpoint = TEXT("127.0.0.1:17777");
    FParse::Value(FCommandLine::Get(), TEXT("MahjongIntegrationServer="), Endpoint);
    FString ServerIP;
    FString PortText;
    int32 ServerPort = 17777;
    if (!Endpoint.Split(TEXT(":"), &ServerIP, &PortText, ESearchCase::IgnoreCase, ESearchDir::FromEnd)
        || !LexTryParseString(ServerPort, *PortText))
    {
        ServerIP = TEXT("127.0.0.1");
        ServerPort = 17777;
    }
    PendingPlayerName = FString::Printf(TEXT("集成玩家%d"), IntegrationClientIndex);
    if (UGuiyangReconnectSubsystem* Reconnect = GetGameInstance()->GetSubsystem<UGuiyangReconnectSubsystem>())
    {
        Reconnect->RememberConnection(ServerIP, ServerPort, PendingPlayerName);
    }

    if (UGuiyangLoginSubsystem* Login = GetGameInstance()->GetSubsystem<UGuiyangLoginSubsystem>();
        Login && !Login->IsSessionValid())
    {
        const FString PlayerId = FString::Printf(TEXT("integration-client-%d"), IntegrationClientIndex);
        const FString Token = FString::Printf(TEXT("integration-session-token-%d-2026"), IntegrationClientIndex);
        Login->LoginForIntegrationTest(PlayerId, PendingPlayerName, Token);
    }

    GetWorldTimerManager().SetTimer(IntegrationPollTimer, this, &ThisClass::PollIntegrationClient, 0.25f, true, 0.25f);
    UE_LOG(LogMahjongNet, Display, TEXT("MAHJONG_INTEGRATION_CLIENT_READY Client=%d Endpoint=%s:%d"),
        IntegrationClientIndex, *ServerIP, ServerPort);
#endif
}

void AGuiyangMahjongPlayerController::PollIntegrationClient()
{
#if !UE_BUILD_SHIPPING
    if (IntegrationClientIndex == INDEX_NONE) return;
    if (UGuiyangReconnectSubsystem* Reconnect = GetGameInstance()->GetSubsystem<UGuiyangReconnectSubsystem>();
        Reconnect && Reconnect->IsReconnectPending())
    {
        if (IntegrationReconnectObservedAtSeconds <= 0.0)
        {
            IntegrationReconnectObservedAtSeconds = FPlatformTime::Seconds();
            return;
        }
        if (!bIntegrationRetryRequested && Reconnect->CanRetry()
            && FPlatformTime::Seconds() - IntegrationReconnectObservedAtSeconds >= 1.0)
        {
            bIntegrationRetryRequested = true;
            UE_LOG(LogMahjongReconnect, Display, TEXT("MAHJONG_INTEGRATION_RETRY Client=%d"), IntegrationClientIndex);
            RetryLastConnection();
        }
        return;
    }

    const AGuiyangMahjongPlayerState* MahjongPlayer = GetPlayerState<AGuiyangMahjongPlayerState>();
    if (!MahjongPlayer || MahjongPlayer->MahjongPlayerId.IsEmpty()) return;
    if (MahjongPlayer->RoomCode.IsEmpty())
    {
        if (!bIntegrationQuickStartRequested)
        {
            bIntegrationQuickStartRequested = true;
            if (IntegrationClientIndex == 0 && IsClientFullMatchIntegrationEnabled())
            {
                // 完整对局集成只跑一局；牌桌动作仍由服务端权威托管推进。
                FMahjongCreateRoomRequest Request;
                Request.RoundCount = 1;
                Request.Rules.bEnableTimeoutAutoPlay = true;
                Request.Rules.TurnTimeoutSeconds = 3;
                Request.Rules.ReactionTimeoutSeconds = 3;
                Request.bPublicRoom = true;
                Request.bAutoStart = true;
                Server_RequestCreateRoomWithConfig(Request);
            }
            else
            {
                Server_RequestQuickStart();
            }
        }
        return;
    }
    if (!MahjongPlayer->bReady && !bIntegrationReadyRequested)
    {
        const AGuiyangMahjongGameState* State = GetWorld()->GetGameState<AGuiyangMahjongGameState>();
        if (State && (State->RoomState.Lifecycle == EMahjongRoomLifecycle::WaitingForPlayers
            || State->RoomState.Lifecycle == EMahjongRoomLifecycle::ReadyCheck))
        {
            bIntegrationReadyRequested = true;
            Server_RequestReady();
        }
    }
#endif
}

void AGuiyangMahjongPlayerController::HandleIntegrationPrivateState(const FMahjongPrivatePlayerState& PrivateState)
{
#if !UE_BUILD_SHIPPING
    if (IntegrationClientIndex == INDEX_NONE || PrivateState.RoundId <= 0 || PrivateState.Hand.Tiles.IsEmpty()) return;
    UE_LOG(LogMahjongNet, Display, TEXT("MAHJONG_INTEGRATION_PRIVATE_STATE Client=%d Seat=%d Hand=%d Round=%d"),
        IntegrationClientIndex, PrivateState.SeatIndex, PrivateState.Hand.Tiles.Num(), PrivateState.RoundId);
    if (IntegrationClientIndex == 0 && !GIntegrationDisconnectTriggered
        && !IsClientFullMatchIntegrationEnabled())
    {
        GIntegrationDisconnectTriggered = true;
        Server_RequestIntegrationDisconnect();
    }
#endif
}
