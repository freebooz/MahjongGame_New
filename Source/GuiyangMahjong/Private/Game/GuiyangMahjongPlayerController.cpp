#include "Game/GuiyangMahjongPlayerController.h"
#include "GuiyangMahjong.h"
#include "UI/MobileRootHUDWidget.h"
#include "Game/GuiyangMahjongGameMode.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

void AGuiyangMahjongPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController() || IsRunningDedicatedServer())
    {
        return;
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

    if (FParse::Param(FCommandLine::Get(), TEXT("UIReviewScreenshot")))
    {
        FString ReviewName = TEXT("UIReview");
        FParse::Value(FCommandLine::Get(), TEXT("UIReviewName="), ReviewName);
        const FString ReviewDirectory = FPaths::ProjectSavedDir() / TEXT("UIReview");
        IFileManager::Get().MakeDirectory(*ReviewDirectory, true);
        const FString ScreenshotPath = ReviewDirectory / (ReviewName + TEXT(".png"));
        FTimerHandle ScreenshotTimer;
        GetWorldTimerManager().SetTimer(ScreenshotTimer, FTimerDelegate::CreateWeakLambda(this, [this, ScreenshotPath]()
        {
            FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
            UE_LOG(LogMahjongUI, Log, TEXT("UI 审查截图已请求：%s"), *ScreenshotPath);
            FTimerHandle ExitTimer;
            GetWorldTimerManager().SetTimer(ExitTimer, [] { FPlatformMisc::RequestExit(false); }, 1.0f, false);
        }), 2.0f, false);
    }
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
    LastServerIP = CleanIP;
    LastServerPort = Port;
    const FString TravelURL = FString::Printf(TEXT("%s:%d"), *CleanIP, Port);
    UE_LOG(LogMahjongNet, Log, TEXT("客户端准备连接服务器：地址=%s，玩家=%s"), *TravelURL, *CleanName);
    ClientTravel(TravelURL, TRAVEL_Absolute);
}

void AGuiyangMahjongPlayerController::RetryLastConnection()
{
    if (LastServerIP.IsEmpty())
    {
        Client_ShowErrorMessage(TEXT("没有可用的上次连接地址"));
        return;
    }
    UE_LOG(LogMahjongReconnect, Log, TEXT("客户端尝试重连：%s:%d"), *LastServerIP, LastServerPort);
    ConnectToServer(LastServerIP, LastServerPort, PendingPlayerName);
}

void AGuiyangMahjongPlayerController::Server_RequestCreateRoom_Implementation()
{
    Server_RequestCreateRoomWithConfig_Implementation(FMahjongCreateRoomRequest());
    UE_LOG(LogMahjongServer, Log, TEXT("收到创建房间请求：控制器=%s"), *GetName());
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
    const FString CleanName = PlayerName.TrimStartAndEnd();
    if (CleanName.IsEmpty() || CleanName.Len() > 24)
    {
        UE_LOG(LogMahjongNet, Warning, TEXT("RPC拒绝：加入房间昵称非法"));
        Client_ShowErrorMessage(TEXT("昵称不能为空且不能超过24个字符"));
        return;
    }
    UE_LOG(LogMahjongServer, Log, TEXT("收到加入房间请求：玩家=%s"), *CleanName);
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

void AGuiyangMahjongPlayerController::Client_UpdatePrivateHand_Implementation(const FMahjongPrivatePlayerState& PrivateState)
{
    UE_LOG(LogMahjongNet, Log, TEXT("收到私有手牌：座位=%d，牌数=%d，序号=%d"), PrivateState.SeatIndex, PrivateState.Hand.Tiles.Num(), PrivateState.StateSequence);
    OnPrivateHandUpdated.Broadcast(PrivateState);
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

void AGuiyangMahjongPlayerController::Client_ShowErrorMessage_Implementation(const FString& Message)
{
    UE_LOG(LogMahjongUI, Warning, TEXT("显示中文错误提示：%s"), *Message);
    OnErrorShown.Broadcast(Message);
}
