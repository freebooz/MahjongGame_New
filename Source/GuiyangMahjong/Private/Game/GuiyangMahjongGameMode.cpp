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
    if (TableEngine || !RoomManager) return;
    TableEngine = NewObject<UMahjongTableEngine>(this);
    FString Error;
    const int32 Seed = static_cast<int32>(FPlatformTime::Cycles64());
    if (!TableEngine->StartRound(StartingRoomState.RuleSnapshot, StartingRoomState.Seats, 0, Seed, Error))
    {
        TableEngine = nullptr;
        return;
    }
    FMahjongRoomState PlayingState;
    EMahjongRoomError RoomError;
    if (!RoomManager->BeginPlaying(StartingRoomState.RoomInfo.RoomId, PlayingState, RoomError))
    {
        TableEngine = nullptr;
        return;
    }
    PublishRoomState(PlayingState);
    PublishTableSnapshots();
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
