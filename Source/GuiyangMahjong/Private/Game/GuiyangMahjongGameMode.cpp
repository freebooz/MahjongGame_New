#include "Game/GuiyangMahjongGameMode.h"

#include "Game/GuiyangMahjongGameState.h"
#include "Game/GuiyangMahjongPlayerController.h"
#include "Game/GuiyangMahjongPlayerState.h"
#include "GuiyangMahjong.h"
#include "Room/GuiyangRoomManager.h"
#include "Table/MahjongTableEngine.h"
#include "HAL/PlatformTime.h"
#include "EngineUtils.h"
#include "Misc/SecureHash.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
    bool IsFullMatchIntegrationEnabled()
    {
        return FParse::Param(FCommandLine::Get(), TEXT("MahjongEnableIntegrationHooks"))
            && FParse::Param(FCommandLine::Get(), TEXT("MahjongIntegrationFullMatch"));
    }
}

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
}

void AGuiyangMahjongGameMode::Logout(AController* Exiting)
{
    if (AGuiyangMahjongPlayerController* Controller = Cast<AGuiyangMahjongPlayerController>(Exiting))
    {
        AGuiyangMahjongPlayerState* Player = Controller->GetPlayerState<AGuiyangMahjongPlayerState>();
        FMahjongRoomState State;
        if (Player && RoomManager && RoomManager->GetRoomState(Player->RoomCode, State)
            && (State.Lifecycle == EMahjongRoomLifecycle::Playing
                || State.Lifecycle == EMahjongRoomLifecycle::WaitingNextRound
                || State.Lifecycle == EMahjongRoomLifecycle::Starting
                || State.Lifecycle == EMahjongRoomLifecycle::Settlement))
        {
            EMahjongRoomError Error;
            if (RoomManager->MarkDisconnected(Player->MahjongPlayerId, State, Error)) PublishRoomState(State);
        }
        else
        {
            HandleLeaveRoom(Controller);
        }
    }
    Super::Logout(Exiting);
}

void AGuiyangMahjongGameMode::HandleAuthenticateSession(AGuiyangMahjongPlayerController* Controller,
    const FString& PlayerId, const FString& DisplayName, const EGuiyangLoginProvider Provider,
    const FString& SessionToken)
{
    const FString CleanPlayerId = PlayerId.TrimStartAndEnd();
    const FString CleanDisplayName = DisplayName.TrimStartAndEnd();
    const bool bProviderAllowed = Provider == EGuiyangLoginProvider::Guest
        || Provider == EGuiyangLoginProvider::SimulatedWechat;
    if (!Controller || CleanPlayerId.IsEmpty() || CleanPlayerId.Len() > 80
        || CleanDisplayName.IsEmpty() || CleanDisplayName.Len() > 24
        || SessionToken.Len() < 16 || SessionToken.Len() > 256 || !bProviderAllowed)
    {
        if (Controller) Controller->Client_ShowErrorMessage(TEXT("登录会话格式无效"));
        return;
    }

    const FString CandidateDigest = HashSessionToken(SessionToken);
    if (CandidateDigest.IsEmpty())
    {
        Controller->Client_ShowErrorMessage(TEXT("登录会话校验失败"));
        return;
    }
    if (const FString* ExistingDigest = SessionTokenDigestsByPlayer.Find(CleanPlayerId))
    {
        if (!ConstantTimeDigestEquals(*ExistingDigest, CandidateDigest))
        {
            Controller->Client_ShowErrorMessage(TEXT("重连凭据不匹配"));
            return;
        }
    }
    else
    {
        SessionTokenDigestsByPlayer.Add(CleanPlayerId, CandidateDigest);
    }

    for (TActorIterator<AGuiyangMahjongPlayerController> It(GetWorld()); It; ++It)
    {
        if (*It == Controller) continue;
        const AGuiyangMahjongPlayerState* Other = It->GetPlayerState<AGuiyangMahjongPlayerState>();
        if (Other && Other->MahjongPlayerId == CleanPlayerId && Other->HasValidServerSession())
        {
            Controller->Client_ShowErrorMessage(TEXT("该账号已经在线"));
            return;
        }
    }

    AGuiyangMahjongPlayerState* Player = Controller->GetPlayerState<AGuiyangMahjongPlayerState>();
    if (!Player || !Player->AuthenticateServer(CleanPlayerId, CleanDisplayName, Provider))
    {
        Controller->Client_ShowErrorMessage(TEXT("服务器认证失败"));
        return;
    }

    FString RoomCode;
    if (!RoomManager || !RoomManager->GetPlayerRoomCode(CleanPlayerId, RoomCode)) return;
    FMahjongRoomState State;
    EMahjongRoomError Error;
    int32 RemainingSeconds = 0;
    if (!RoomManager->ReconnectPlayer(CleanPlayerId, State, RemainingSeconds, Error))
    {
        Controller->Client_ShowErrorMessage(ErrorToMessage(Error));
        return;
    }
    const FMahjongSeatInfo* Seat = State.Seats.FindByPredicate([&CleanPlayerId](const FMahjongSeatInfo& Item)
    {
        return Item.PlayerId == CleanPlayerId;
    });
    Player->EnterRoomServer(State.RoomInfo.RoomId, Seat ? Seat->SeatIndex : INDEX_NONE, Seat ? Seat->bReady : false);
    PublishRoomState(State);
    PublishReconnectSnapshot(Controller, State, RemainingSeconds);
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
    if (IsFullMatchIntegrationEnabled() && Player->MahjongPlayerId == TEXT("integration-client-0"))
    {
        UE_LOG(LogMahjongServer, Display,
            TEXT("MAHJONG_INTEGRATION_FULL_MATCH_ROOM_READY Room=%s Rounds=%d TurnTimeout=%d ReactionTimeout=%d"),
            *State.RoomInfo.RoomId, State.RoomInfo.RoundCount, State.RuleSnapshot.Config.TurnTimeoutSeconds,
            State.RuleSnapshot.Config.ReactionTimeoutSeconds);
    }
}

void AGuiyangMahjongGameMode::HandleQuickStart(AGuiyangMahjongPlayerController* Controller)
{
    AGuiyangMahjongPlayerState* Player = nullptr;
    if (!ResolvePlayer(Controller, Player)) return;
    FMahjongRoomState State;
    EMahjongRoomError Error;
    if (!RoomManager->QuickStart(Player->MahjongPlayerId, Player->DisplayName, State, Error))
    {
        Controller->Client_ShowErrorMessage(ErrorToMessage(Error));
        return;
    }
    const FMahjongSeatInfo* Seat = State.Seats.FindByPredicate([Player](const FMahjongSeatInfo& Item)
    {
        return Item.PlayerId == Player->MahjongPlayerId;
    });
    Player->EnterRoomServer(State.RoomInfo.RoomId, Seat ? Seat->SeatIndex : INDEX_NONE);
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
    LastPublishedFinalRoomSequence = INDEX_NONE;
    ArmedTimeoutRoundId = INDEX_NONE;
    ArmedTimeoutTurnId = INDEX_NONE;
    ArmedTimeoutPhase = EMahjongTablePhase::WaitingForPlayers;
    PublishRoomState(PlayingState);
    PublishTableSnapshots();
    if (FParse::Param(FCommandLine::Get(), TEXT("MahjongEnableIntegrationHooks")))
    {
        UE_LOG(LogMahjongServer, Display,
            TEXT("MAHJONG_INTEGRATION_TABLE_STARTED Room=%s Players=%d Round=%d"),
            *PlayingState.RoomInfo.RoomId, PlayingState.Seats.Num(), TableEngine->GetPublicState().RoundId);
    }
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
    // 仅显式完整对局集成模式使用快速计时器；正式游戏仍严格采用规则快照秒数。
    const float TimerDelay = IsFullMatchIntegrationEnabled() ? 0.05f : static_cast<float>(TimeoutSeconds);
    TableEngine->SetActionDeadlineForServer(GetWorld()->GetTimeSeconds() + TimerDelay,
        IsFullMatchIntegrationEnabled() ? 1 : TimeoutSeconds);
    FTimerDelegate Delegate;
    Delegate.BindUObject(this, &ThisClass::HandleActionTimeout, State.RoundId, State.TurnId, State.Phase);
    GetWorldTimerManager().SetTimer(ActionTimeoutHandle, Delegate, TimerDelay, false);
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
    if (State.Lifecycle == EMahjongRoomLifecycle::Settlement
        && State.RoomInfo.CurrentRound >= State.RoomInfo.RoundCount)
        PublishFinalSettlement(State);
}

void AGuiyangMahjongGameMode::PublishReconnectSnapshot(AGuiyangMahjongPlayerController* Controller,
    const FMahjongRoomState& RoomState, const int32 RemainingReconnectSeconds)
{
    if (!Controller) return;
    const AGuiyangMahjongPlayerState* Player = Controller->GetPlayerState<AGuiyangMahjongPlayerState>();
    if (!Player || Player->SeatIndex == INDEX_NONE) return;

    FMahjongReconnectSnapshot Snapshot;
    Snapshot.RoomState = RoomState;
    Snapshot.RemainingReconnectSeconds = RemainingReconnectSeconds;
    TArray<FMahjongAction> Actions;
    if (TableEngine && ActiveRoomCode == RoomState.RoomInfo.RoomId)
    {
        Snapshot.TableState = TableEngine->GetPublicState();
        TableEngine->GetPrivateState(Player->SeatIndex, Snapshot.PrivateState);
        Actions = TableEngine->GetAvailableActions(Player->SeatIndex);
    }
    Controller->Client_RestoreReconnectSnapshot(Snapshot, Actions);
    if (FParse::Param(FCommandLine::Get(), TEXT("MahjongEnableIntegrationHooks"))
        && Player->MahjongPlayerId.StartsWith(TEXT("integration-client-")))
    {
        int32 OnlineSeats = 0;
        for (const FMahjongSeatInfo& Seat : RoomState.Seats)
        {
            OnlineSeats += Seat.bOccupied && Seat.bOnline ? 1 : 0;
        }
        UE_LOG(LogMahjongReconnect, Display,
            TEXT("MAHJONG_INTEGRATION_RECONNECT_OK Player=%s Seat=%d Online=%d Hand=%d Round=%d Remaining=%d"),
            *Player->MahjongPlayerId, Player->SeatIndex, OnlineSeats, Snapshot.PrivateState.Hand.Tiles.Num(),
            Snapshot.TableState.RoundId, RemainingReconnectSeconds);
    }
    FMahjongSettlementResult Settlement;
    if (TableEngine && TableEngine->GetSettlementResult(Settlement)) Controller->Client_ShowSettlement(Settlement);
    if (RoomState.Lifecycle == EMahjongRoomLifecycle::Settlement
        && RoomState.RoomInfo.CurrentRound >= RoomState.RoomInfo.RoundCount)
        Controller->Client_ShowFinalSettlement(UGuiyangRoomManager::BuildFinalSettlement(RoomState));
}

void AGuiyangMahjongGameMode::PublishFinalSettlement(const FMahjongRoomState& RoomState)
{
    if (RoomState.StateSequence == LastPublishedFinalRoomSequence) return;
    const FMahjongFinalSettlementResult Result = UGuiyangRoomManager::BuildFinalSettlement(RoomState);
    for (TActorIterator<AGuiyangMahjongPlayerController> It(GetWorld()); It; ++It)
        It->Client_ShowFinalSettlement(Result);
    if (IsFullMatchIntegrationEnabled())
    {
        UE_LOG(LogMahjongServer, Display,
            TEXT("MAHJONG_INTEGRATION_FULL_MATCH_COMPLETE Room=%s Rounds=%d Players=%d"),
            *Result.RoomId, Result.CompletedRounds, Result.Players.Num());
    }
    LastPublishedFinalRoomSequence = RoomState.StateSequence;
}

FString AGuiyangMahjongGameMode::HashSessionToken(const FString& SessionToken)
{
    FTCHARToUTF8 Utf8(*SessionToken);
    if (Utf8.Length() <= 0) return FString();
    uint8 Digest[FSHA1::DigestSize];
    FSHA1::HashBuffer(Utf8.Get(), Utf8.Length(), Digest);
    return BytesToHex(Digest, UE_ARRAY_COUNT(Digest)).ToLower();
}

bool AGuiyangMahjongGameMode::ConstantTimeDigestEquals(const FString& Left, const FString& Right)
{
    uint32 Difference = static_cast<uint32>(Left.Len() ^ Right.Len());
    const int32 Count = FMath::Max(Left.Len(), Right.Len());
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const TCHAR LeftChar = Left.IsValidIndex(Index) ? Left[Index] : 0;
        const TCHAR RightChar = Right.IsValidIndex(Index) ? Right[Index] : 0;
        Difference |= static_cast<uint32>(LeftChar ^ RightChar);
    }
    return Difference == 0;
}

FString AGuiyangMahjongGameMode::ErrorToMessage(const EMahjongRoomError Error)
{
    switch (Error)
    {
    case EMahjongRoomError::SessionExpired: return TEXT("重连保留时间已结束");
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
