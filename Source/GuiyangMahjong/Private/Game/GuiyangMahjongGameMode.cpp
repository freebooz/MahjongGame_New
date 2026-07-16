#include "Game/GuiyangMahjongGameMode.h"

#include "Game/GuiyangMahjongGameState.h"
#include "Game/GuiyangMahjongPlayerController.h"
#include "Game/GuiyangMahjongPlayerState.h"
#include "Room/GuiyangRoomManager.h"
#include "Table/MahjongTableEngine.h"
#include "HAL/PlatformTime.h"
#include "EngineUtils.h"

AGuiyangMahjongGameMode::AGuiyangMahjongGameMode()
{
    GameStateClass = AGuiyangMahjongGameState::StaticClass();
    PlayerControllerClass = AGuiyangMahjongPlayerController::StaticClass();
    PlayerStateClass = AGuiyangMahjongPlayerState::StaticClass();
}

void AGuiyangMahjongGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
    RoomManager = NewObject<UGuiyangRoomManager>(this);
}

void AGuiyangMahjongGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    if (AGuiyangMahjongPlayerState* State = NewPlayer ? NewPlayer->GetPlayerState<AGuiyangMahjongPlayerState>() : nullptr)
    {
        const FString GuestId = TEXT("guest-") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
        State->AuthenticateServer(GuestId, FString::Printf(TEXT("游客%s"), *GuestId.Right(4)), EGuiyangLoginProvider::Guest);
    }
}

void AGuiyangMahjongGameMode::Logout(AController* Exiting)
{
    if (AGuiyangMahjongPlayerController* Controller = Cast<AGuiyangMahjongPlayerController>(Exiting)) HandleLeaveRoom(Controller);
    Super::Logout(Exiting);
}

void AGuiyangMahjongGameMode::HandleCreateRoom(AGuiyangMahjongPlayerController* Controller, const FMahjongCreateRoomRequest& Request)
{
    AGuiyangMahjongPlayerState* Player = nullptr;
    if (!ResolvePlayer(Controller, Player)) return;
    FMahjongRoomState State;
    EMahjongRoomError Error;
    if (!RoomManager->CreateRoom(Player->MahjongPlayerId, Player->DisplayName, Request, State, Error))
    {
        Controller->Client_ShowErrorMessage(ErrorToMessage(Error));
        return;
    }
    Player->EnterRoomServer(State.RoomInfo.RoomId, 0);
    PublishRoomState(State);
}

void AGuiyangMahjongGameMode::HandleJoinRoom(AGuiyangMahjongPlayerController* Controller, const FMahjongJoinRoomRequest& Request)
{
    AGuiyangMahjongPlayerState* Player = nullptr;
    if (!ResolvePlayer(Controller, Player)) return;
    FMahjongRoomState State;
    EMahjongRoomError Error;
    if (!RoomManager->JoinRoom(Player->MahjongPlayerId, Player->DisplayName, Request, State, Error))
    {
        Controller->Client_ShowErrorMessage(ErrorToMessage(Error));
        return;
    }
    const FMahjongSeatInfo* Seat = State.Seats.FindByPredicate([Player](const FMahjongSeatInfo& Item) { return Item.PlayerId == Player->MahjongPlayerId; });
    Player->EnterRoomServer(State.RoomInfo.RoomId, Seat ? Seat->SeatIndex : INDEX_NONE);
    PublishRoomState(State);
}

void AGuiyangMahjongGameMode::HandleToggleReady(AGuiyangMahjongPlayerController* Controller)
{
    AGuiyangMahjongPlayerState* Player = nullptr;
    if (!ResolvePlayer(Controller, Player)) return;
    FMahjongRoomState State;
    EMahjongRoomError Error;
    if (!RoomManager->ToggleReady(Player->MahjongPlayerId, State, Error))
    {
        Controller->Client_ShowErrorMessage(ErrorToMessage(Error));
        return;
    }
    if (const FMahjongSeatInfo* Seat = State.Seats.FindByPredicate([Player](const FMahjongSeatInfo& Item) { return Item.PlayerId == Player->MahjongPlayerId; }))
        Player->EnterRoomServer(State.RoomInfo.RoomId, Seat->SeatIndex, Seat->bReady);
    PublishRoomState(State);
    if (State.Lifecycle == EMahjongRoomLifecycle::Starting) TryStartTable(State);
}

void AGuiyangMahjongGameMode::HandleLeaveRoom(AGuiyangMahjongPlayerController* Controller)
{
    AGuiyangMahjongPlayerState* Player = Controller ? Controller->GetPlayerState<AGuiyangMahjongPlayerState>() : nullptr;
    if (!Player || !RoomManager || Player->MahjongPlayerId.IsEmpty()) return;
    FMahjongRoomState State;
    EMahjongRoomError Error;
    if (RoomManager->LeaveRoom(Player->MahjongPlayerId, State, Error))
    {
        Player->LeaveRoomServer();
        PublishRoomState(State);
    }
}

bool AGuiyangMahjongGameMode::ResolvePlayer(AGuiyangMahjongPlayerController* Controller, AGuiyangMahjongPlayerState*& OutPlayerState) const
{
    OutPlayerState = Controller ? Controller->GetPlayerState<AGuiyangMahjongPlayerState>() : nullptr;
    if (!RoomManager || !OutPlayerState || !OutPlayerState->HasValidServerSession())
    {
        if (Controller) Controller->Client_ShowErrorMessage(TEXT("会话无效，请重新登录"));
        return false;
    }
    return true;
}

void AGuiyangMahjongGameMode::PublishRoomState(const FMahjongRoomState& State)
{
    if (AGuiyangMahjongGameState* MahjongState = GetGameState<AGuiyangMahjongGameState>()) MahjongState->SetRoomStateAuthority(State);
}

void AGuiyangMahjongGameMode::TryStartTable(const FMahjongRoomState& StartingRoomState)
{
    if (!RoomManager || (TableEngine && TableEngine->GetPublicState().Phase != EMahjongTablePhase::Settlement)) return;
    UMahjongTableEngine* RoundEngine = TableEngine ? TableEngine.Get() : NewObject<UMahjongTableEngine>(this);
    FString Error;
    const int32 Seed = static_cast<int32>(FPlatformTime::Cycles64());
    if (!RoundEngine->StartRound(StartingRoomState.RuleSnapshot, StartingRoomState.Seats,
        StartingRoomState.RoomInfo.DealerSeat, Seed, Error))
    {
        if (!TableEngine) RoundEngine = nullptr;
        return;
    }
    FMahjongRoomState PlayingState;
    EMahjongRoomError RoomError;
    if (!RoomManager->BeginPlaying(StartingRoomState.RoomInfo.RoomId, PlayingState, RoomError))
    {
        return;
    }
    TableEngine = RoundEngine;
    ActiveRoomCode = StartingRoomState.RoomInfo.RoomId;
    LastPublishedSettlementSequence = INDEX_NONE;
    LastFinalizedSettlementSequence = INDEX_NONE;
    ArmedTimeoutRoundId = INDEX_NONE;
    ArmedTimeoutTurnId = INDEX_NONE;
    ArmedTimeoutPhase = EMahjongTablePhase::WaitingForPlayers;
    PublishRoomState(PlayingState);
    PublishTableSnapshots();
}

void AGuiyangMahjongGameMode::HandleNextRound(AGuiyangMahjongPlayerController* Controller)
{
    AGuiyangMahjongPlayerState* Player = nullptr;
    if (!ResolvePlayer(Controller, Player)) return;
    FMahjongRoomState State;
    EMahjongRoomError Error;
    if (!RoomManager->RequestNextRound(Player->MahjongPlayerId, State, Error))
    {
        Controller->Client_ShowErrorMessage(ErrorToMessage(Error));
        return;
    }
    if (const FMahjongSeatInfo* Seat = State.Seats.FindByPredicate([Player](const FMahjongSeatInfo& Item)
    {
        return Item.PlayerId == Player->MahjongPlayerId;
    }))
    {
        Player->EnterRoomServer(State.RoomInfo.RoomId, Seat->SeatIndex, Seat->bReady);
    }
    PublishRoomState(State);
    if (State.Lifecycle == EMahjongRoomLifecycle::Starting) TryStartTable(State);
}

void AGuiyangMahjongGameMode::HandleTableAction(AGuiyangMahjongPlayerController* Controller, const FMahjongActionRequest& Request)
{
    AGuiyangMahjongPlayerState* Player = nullptr;
    if (!ResolvePlayer(Controller, Player) || !TableEngine || Player->SeatIndex == INDEX_NONE)
    {
        if (Controller) Controller->Client_ShowErrorMessage(TEXT("牌桌尚未开始"));
        return;
    }
    FMahjongActionResult Result;
    if (Request.Type == EMahjongActionType::Play)
        Result = TableEngine->SubmitPlayTile(Player->SeatIndex, Request);
    else if (TableEngine->GetPublicState().Phase == EMahjongTablePhase::PlayerTurn)
        Result = TableEngine->SubmitTurnAction(Player->SeatIndex, Request);
    else
        Result = TableEngine->SubmitReaction(Player->SeatIndex, Request);
    if (!Result.bSuccess)
    {
        Controller->Client_ShowErrorMessage(Result.Message);
        return;
    }
    PublishTableSnapshots();
    FinalizeRoundIfNeeded();
}

void AGuiyangMahjongGameMode::HandleLegacyPlayTile(AGuiyangMahjongPlayerController* Controller, const FMahjongTile& Tile, const int32 ClientSequence)
{
    if (!TableEngine) return;
    FMahjongActionRequest Request;
    Request.Type = EMahjongActionType::Play;
    Request.RoundId = TableEngine->GetPublicState().RoundId;
    Request.TurnId = TableEngine->GetPublicState().TurnId;
    Request.TargetTileId = Tile.UniqueId;
    Request.ClientSequence = ClientSequence;
    HandleTableAction(Controller, Request);
}

void AGuiyangMahjongGameMode::PublishTableSnapshots()
{
    if (!TableEngine) return;
    RefreshActionTimeoutTimer();
    if (AGuiyangMahjongGameState* MahjongState = GetGameState<AGuiyangMahjongGameState>())
        MahjongState->SetPublicTableStateAuthority(TableEngine->GetPublicState());
    FMahjongSettlementResult Settlement;
    const bool bPublishSettlement = TableEngine->GetSettlementResult(Settlement)
        && LastPublishedSettlementSequence != TableEngine->GetPublicState().StateSequence;
    for (TActorIterator<AGuiyangMahjongPlayerController> It(GetWorld()); It; ++It)
    {
        AGuiyangMahjongPlayerController* Controller = *It;
        const AGuiyangMahjongPlayerState* Player = Controller->GetPlayerState<AGuiyangMahjongPlayerState>();
        if (!Player || Player->SeatIndex == INDEX_NONE) continue;
        FMahjongPrivatePlayerState PrivateState;
        if (TableEngine->GetPrivateState(Player->SeatIndex, PrivateState)) Controller->Client_UpdatePrivateHand(PrivateState);
        Controller->Client_ShowAvailableActions(TableEngine->GetAvailableActions(Player->SeatIndex));
        if (bPublishSettlement) Controller->Client_ShowSettlement(Settlement);
    }
    if (bPublishSettlement) LastPublishedSettlementSequence = TableEngine->GetPublicState().StateSequence;
}

void AGuiyangMahjongGameMode::RefreshActionTimeoutTimer()
{
    if (!TableEngine || !GetWorld()) return;
    const FMahjongPublicTableState& State = TableEngine->GetPublicState();
    const bool bActionPhase = State.Phase == EMahjongTablePhase::PlayerTurn
        || State.Phase == EMahjongTablePhase::WaitingForAction;
    if (!bActionPhase || !TableEngine->GetLockedRuleSnapshot().Config.bEnableTimeoutAutoPlay)
    {
        GetWorldTimerManager().ClearTimer(ActionTimeoutHandle);
        ArmedTimeoutRoundId = INDEX_NONE;
        ArmedTimeoutTurnId = INDEX_NONE;
        ArmedTimeoutPhase = EMahjongTablePhase::WaitingForPlayers;
        TableEngine->SetActionDeadlineForServer(0.0, 0);
        return;
    }
    if (ArmedTimeoutRoundId == State.RoundId && ArmedTimeoutTurnId == State.TurnId
        && ArmedTimeoutPhase == State.Phase && GetWorldTimerManager().IsTimerActive(ActionTimeoutHandle)) return;

    GetWorldTimerManager().ClearTimer(ActionTimeoutHandle);
    ArmedTimeoutRoundId = State.RoundId;
    ArmedTimeoutTurnId = State.TurnId;
    ArmedTimeoutPhase = State.Phase;
    const int32 TimeoutSeconds = State.Phase == EMahjongTablePhase::PlayerTurn
        ? TableEngine->GetLockedRuleSnapshot().Config.TurnTimeoutSeconds
        : TableEngine->GetLockedRuleSnapshot().Config.ReactionTimeoutSeconds;
    TableEngine->SetActionDeadlineForServer(GetWorld()->GetTimeSeconds() + TimeoutSeconds, TimeoutSeconds);
    FTimerDelegate Delegate;
    Delegate.BindUObject(this, &ThisClass::HandleActionTimeout, State.RoundId, State.TurnId, State.Phase);
    GetWorldTimerManager().SetTimer(ActionTimeoutHandle, Delegate, TimeoutSeconds, false);
}

void AGuiyangMahjongGameMode::HandleActionTimeout(const int32 ExpectedRoundId, const int32 ExpectedTurnId,
    const EMahjongTablePhase ExpectedPhase)
{
    if (!TableEngine) return;
    const FMahjongActionResult Result = TableEngine->ResolveActionTimeout(ExpectedRoundId, ExpectedTurnId, ExpectedPhase);
    if (!Result.bSuccess) return;
    PublishTableSnapshots();
    FinalizeRoundIfNeeded();
}

void AGuiyangMahjongGameMode::FinalizeRoundIfNeeded()
{
    if (!TableEngine || !RoomManager || ActiveRoomCode.IsEmpty()) return;
    const int32 SettlementSequence = TableEngine->GetPublicState().StateSequence;
    if (TableEngine->GetPublicState().Phase != EMahjongTablePhase::Settlement
        || SettlementSequence == LastFinalizedSettlementSequence) return;

    FMahjongSettlementResult Settlement;
    if (!TableEngine->GetSettlementResult(Settlement)) return;
    FMahjongRoomState State;
    EMahjongRoomError Error;
    if (!RoomManager->FinishRound(ActiveRoomCode, Settlement, State, Error)) return;
    LastFinalizedSettlementSequence = SettlementSequence;
    for (TActorIterator<AGuiyangMahjongPlayerController> It(GetWorld()); It; ++It)
    {
        if (AGuiyangMahjongPlayerState* Player = It->GetPlayerState<AGuiyangMahjongPlayerState>())
            Player->EnterRoomServer(State.RoomInfo.RoomId, Player->SeatIndex, false);
    }
    PublishRoomState(State);
}

FString AGuiyangMahjongGameMode::ErrorToMessage(const EMahjongRoomError Error)
{
    switch (Error)
    {
    case EMahjongRoomError::AlreadyInRoom: return TEXT("你已经在房间中");
    case EMahjongRoomError::RoomNotFound: return TEXT("房间不存在");
    case EMahjongRoomError::RoomFull: return TEXT("房间已满");
    case EMahjongRoomError::PasswordRequired: return TEXT("请输入房间密码");
    case EMahjongRoomError::WrongPassword: return TEXT("房间密码错误");
    case EMahjongRoomError::TooManyPasswordAttempts: return TEXT("密码错误次数过多，请稍后再试");
    case EMahjongRoomError::GameAlreadyStarted: return TEXT("牌局已经开始");
    case EMahjongRoomError::NotInRoom: return TEXT("你当前不在房间中");
    default: return TEXT("房间请求无效");
    }
}
