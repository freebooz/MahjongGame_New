#include "Misc/AutomationTest.h"
#include "Table/MahjongDeckManager.h"
#include "Rules/MahjongHuChecker.h"
#include "Rules/MahjongGangChecker.h"
#include "Rules/MahjongChiPengChecker.h"
#include "Rules/GuiyangJiCalculator.h"
#include "Rules/MahjongScoreCalculator.h"
#include "Rules/GuiyangRuleSnapshot.h"
#include "Room/GuiyangRoomManager.h"
#include "Table/MahjongTableEngine.h"
#include "Auth/GuiyangLoginSaveGame.h"
#include "Auth/GuiyangLoginSubsystem.h"
#include "History/MahjongMatchHistorySaveGame.h"
#include "History/GuiyangMatchHistorySubsystem.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Network/MahjongNetworkTypes.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace MahjongTest
{
    static FMahjongTile MakeTile(const int32 RuleIndex, const int32 UniqueId)
    {
        FMahjongTile Tile;
        Tile.UniqueId = UniqueId;
        if (RuleIndex < 27)
        {
            Tile.Type = EMahjongTileType::Number;
            Tile.Suit = RuleIndex < 9 ? EMahjongSuit::Characters : RuleIndex < 18 ? EMahjongSuit::Bamboo : EMahjongSuit::Dots;
            Tile.Rank = RuleIndex % 9 + 1;
        }
        else
        {
            static const EMahjongTileType Honors[] = { EMahjongTileType::East, EMahjongTileType::South, EMahjongTileType::West,
                EMahjongTileType::North, EMahjongTileType::RedDragon, EMahjongTileType::GreenDragon, EMahjongTileType::WhiteDragon };
            Tile.Type = Honors[RuleIndex - 27];
            Tile.Suit = RuleIndex <= 30 ? EMahjongSuit::Winds : EMahjongSuit::Dragons;
        }
        return Tile;
    }

    static FMahjongHand MakeHand(std::initializer_list<int32> RuleIndices)
    {
        FMahjongHand Hand;
        int32 UniqueId = 0;
        for (const int32 Index : RuleIndices) Hand.Tiles.Add(MakeTile(Index, UniqueId++));
        return Hand;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMahjongDefaultDeckTest, "GuiyangMahjong.Core.Deck.Default108", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMahjongDefaultDeckTest::RunTest(const FString& Parameters)
{
    UMahjongDeckManager* Deck = NewObject<UMahjongDeckManager>();
    Deck->InitializeDeck(FMahjongRuleConfig());
    TestEqual(TEXT("贵阳主流默认牌墙必须为 108 张"), Deck->GetRemainingCount(), 108);
    int32 Counts[34] = {};
    for (const FMahjongTile& Tile : Deck->GetDeckForServerTest())
    {
        if (Tile.GetRuleIndex() >= 0) ++Counts[Tile.GetRuleIndex()];
    }
    for (int32 Index = 0; Index < 27; ++Index) TestEqual(FString::Printf(TEXT("数牌类型 %d 必须有 4 张"), Index), Counts[Index], 4);
    for (int32 Index = 27; Index < 34; ++Index) TestEqual(FString::Printf(TEXT("默认牌墙不得包含字牌类型 %d"), Index), Counts[Index], 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMahjongDeckTest, "GuiyangMahjong.Core.Deck.Optional136", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMahjongDeckTest::RunTest(const FString& Parameters)
{
    UMahjongDeckManager* Deck = NewObject<UMahjongDeckManager>();
    Deck->InitializeStandardDeck();
    TestEqual(TEXT("牌墙必须为136张"), Deck->GetRemainingCount(), 136);
    TSet<int32> UniqueIds;
    int32 Counts[34] = {};
    for (const FMahjongTile& Tile : Deck->GetDeckForServerTest())
    {
        UniqueIds.Add(Tile.UniqueId);
        if (Tile.GetRuleIndex() >= 0) ++Counts[Tile.GetRuleIndex()];
    }
    TestEqual(TEXT("每张物理牌ID唯一"), UniqueIds.Num(), 136);
    for (int32 Index = 0; Index < 34; ++Index) TestEqual(FString::Printf(TEXT("牌型%d必须有4张"), Index), Counts[Index], 4);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMahjongShuffleDealTest, "GuiyangMahjong.Core.Deck.ShuffleAndDeal", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMahjongShuffleDealTest::RunTest(const FString& Parameters)
{
    UMahjongDeckManager* Deck = NewObject<UMahjongDeckManager>();
    Deck->InitializeDeck(FMahjongRuleConfig());
    Deck->ShuffleDeck(20260715);
    TArray<FMahjongHand> Hands;
    TestTrue(TEXT("初始发牌应成功"), Deck->DealInitialHands(Hands, 2));
    TestEqual(TEXT("必须有四手牌"), Hands.Num(), 4);
    for (int32 Seat = 0; Seat < 4; ++Seat) TestEqual(TEXT("庄家14张、闲家13张"), Hands[Seat].Tiles.Num(), Seat == 2 ? 14 : 13);
    TestEqual(TEXT("108 张牌墙发牌后应剩余 55 张"), Deck->GetRemainingCount(), 55);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMahjongRuleSnapshotTest, "GuiyangMahjong.Rules.SnapshotDeterminism", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMahjongRuleSnapshotTest::RunTest(const FString& Parameters)
{
    FMahjongRuleConfig Requested;
    Requested.BaseScore = -5;
    Requested.ReconnectTimeoutSeconds = 9999;
    const FGuiyangRuleSnapshot First = UGuiyangRuleSnapshotLibrary::CreateSnapshot(Requested);
    const FGuiyangRuleSnapshot Second = UGuiyangRuleSnapshotLibrary::CreateSnapshot(Requested);

    TestEqual(TEXT("默认规则快照必须锁定 108 张牌"), First.GetTileCount(), 108);
    TestEqual(TEXT("非法底分必须规范化"), First.Config.BaseScore, 1);
    TestEqual(TEXT("重连超时必须限制在服务端允许范围"), First.Config.ReconnectTimeoutSeconds, 600);
    TestEqual(TEXT("相同配置必须产生相同规则哈希"), First.RuleHash, Second.RuleHash);
    TestTrue(TEXT("新建规则快照必须通过完整性校验"), UGuiyangRuleSnapshotLibrary::VerifySnapshot(First));

    FGuiyangRuleSnapshot Tampered = First;
    Tampered.Config.bEnableQiDui = !Tampered.Config.bEnableQiDui;
    TestFalse(TEXT("被修改的规则快照必须校验失败"), UGuiyangRuleSnapshotLibrary::VerifySnapshot(Tampered));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMahjongPasswordRoomTest, "GuiyangMahjong.Room.PasswordSecurityAndLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMahjongPasswordRoomTest::RunTest(const FString& Parameters)
{
    UGuiyangRoomManager* Manager = NewObject<UGuiyangRoomManager>();
    FMahjongCreateRoomRequest CreateRequest;
    CreateRequest.bEnablePassword = true;
    CreateRequest.Password = TEXT("628628");
    FMahjongRoomState State;
    EMahjongRoomError Error = EMahjongRoomError::None;
    TestTrue(TEXT("合法密码房必须创建成功"), Manager->CreateRoom(TEXT("owner-1"), TEXT("房主"), CreateRequest, State, Error));
    TestEqual(TEXT("房间号固定为 6 位"), State.RoomInfo.RoomId.Len(), 6);
    TestTrue(TEXT("房间号只能包含数字"), State.RoomInfo.RoomId.IsNumeric());
    TestTrue(TEXT("公开状态只能暴露密码保护标志"), State.RoomInfo.bPasswordProtected);
    TestTrue(TEXT("房间必须锁定有效规则快照"), UGuiyangRuleSnapshotLibrary::VerifySnapshot(State.RuleSnapshot));

    FMahjongJoinRoomRequest WrongJoin;
    WrongJoin.RoomCode = State.RoomInfo.RoomId;
    WrongJoin.Password = TEXT("111111");
    for (int32 Attempt = 0; Attempt < 4; ++Attempt)
    {
        TestFalse(TEXT("错误密码不得加入房间"), Manager->JoinRoom(TEXT("attacker"), TEXT("测试玩家"), WrongJoin, State, Error));
        TestEqual(TEXT("前四次错误密码返回 WrongPassword"), Error, EMahjongRoomError::WrongPassword);
    }
    TestFalse(TEXT("第五次错误密码触发锁定"), Manager->JoinRoom(TEXT("attacker"), TEXT("测试玩家"), WrongJoin, State, Error));
    TestEqual(TEXT("密码爆破限制必须生效"), Error, EMahjongRoomError::TooManyPasswordAttempts);
    WrongJoin.Password = TEXT("628628");
    TestFalse(TEXT("锁定窗口内即使密码正确也不得加入"), Manager->JoinRoom(TEXT("attacker"), TEXT("测试玩家"), WrongJoin, State, Error));
    TestEqual(TEXT("锁定窗口返回 TooManyPasswordAttempts"), Error, EMahjongRoomError::TooManyPasswordAttempts);

    FMahjongJoinRoomRequest ValidJoin;
    ValidJoin.RoomCode = State.RoomInfo.RoomId;
    ValidJoin.Password = TEXT("628628");
    TestTrue(TEXT("另一账号使用正确密码可加入"), Manager->JoinRoom(TEXT("player-2"), TEXT("玩家二"), ValidJoin, State, Error));
    TestTrue(TEXT("房主离开前房主标识存在"), State.Seats[0].bOwner);
    TestTrue(TEXT("房主可在开局前离开"), Manager->LeaveRoom(TEXT("owner-1"), State, Error));
    TestEqual(TEXT("房主离开后所有权转移"), State.RoomInfo.OwnerPlayerId, FString(TEXT("player-2")));
    TestTrue(TEXT("最后玩家离开会销毁房间"), Manager->LeaveRoom(TEXT("player-2"), State, Error));
    TestEqual(TEXT("空房必须被清理"), Manager->GetRoomCount(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMahjongQuickStartRoomTest, "GuiyangMahjong.Room.QuickStartMatchmaking", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMahjongQuickStartRoomTest::RunTest(const FString& Parameters)
{
    UGuiyangRoomManager* Manager = NewObject<UGuiyangRoomManager>();
    FMahjongRoomState State;
    EMahjongRoomError Error = EMahjongRoomError::None;
    TestTrue(TEXT("首名快速开始玩家创建公开房"), Manager->QuickStart(TEXT("quick-p0"), TEXT("玩家0"), State, Error));
    const FString FirstRoomCode = State.RoomInfo.RoomId;
    TestEqual(TEXT("首个快速房仅有一名玩家"), State.Seats.FilterByPredicate(
        [](const FMahjongSeatInfo& Seat) { return Seat.bOccupied; }).Num(), 1);

    for (int32 Index = 1; Index < 4; ++Index)
    {
        TestTrue(TEXT("后续快速开始玩家加入现有公开房"), Manager->QuickStart(
            FString::Printf(TEXT("quick-p%d"), Index), FString::Printf(TEXT("玩家%d"), Index), State, Error));
        TestEqual(TEXT("快速开始必须复用同一房间"), State.RoomInfo.RoomId, FirstRoomCode);
    }
    TestEqual(TEXT("四名玩家匹配后房间已满"), State.Seats.FilterByPredicate(
        [](const FMahjongSeatInfo& Seat) { return Seat.bOccupied; }).Num(), 4);

    TestTrue(TEXT("满房后新玩家快速开始会创建新房"), Manager->QuickStart(
        TEXT("quick-p4"), TEXT("玩家4"), State, Error));
    TestNotEqual(TEXT("新快速房房间号不同"), State.RoomInfo.RoomId, FirstRoomCode);
    TestEqual(TEXT("快速匹配创建两个房间"), Manager->GetRoomCount(), 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMahjongRoomReadyTest, "GuiyangMahjong.Room.FourPlayersReady", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMahjongRoomReadyTest::RunTest(const FString& Parameters)
{
    UGuiyangRoomManager* Manager = NewObject<UGuiyangRoomManager>();
    FMahjongCreateRoomRequest CreateRequest;
    FMahjongRoomState State;
    EMahjongRoomError Error = EMahjongRoomError::None;
    TestTrue(TEXT("公开房创建成功"), Manager->CreateRoom(TEXT("p0"), TEXT("玩家0"), CreateRequest, State, Error));
    const FString RoomCode = State.RoomInfo.RoomId;
    for (int32 Index = 1; Index < 4; ++Index)
    {
        FMahjongJoinRoomRequest Join;
        Join.RoomCode = RoomCode;
        TestTrue(TEXT("四人房加入成功"), Manager->JoinRoom(FString::Printf(TEXT("p%d"), Index), FString::Printf(TEXT("玩家%d"), Index), Join, State, Error));
    }
    TestFalse(TEXT("满员但未准备不得启动"), State.bGameStarting);
    for (int32 Index = 0; Index < 4; ++Index)
    {
        TestTrue(TEXT("玩家切换准备成功"), Manager->ToggleReady(FString::Printf(TEXT("p%d"), Index), State, Error));
    }
    TestTrue(TEXT("四人全部准备后进入启动状态"), State.bGameStarting);
    TestEqual(TEXT("生命周期必须进入 Starting"), State.Lifecycle, EMahjongRoomLifecycle::Starting);
    TestFalse(TEXT("启动后不得取消准备"), Manager->ToggleReady(TEXT("p0"), State, Error));
    TestEqual(TEXT("启动后准备请求返回已开局"), Error, EMahjongRoomError::GameAlreadyStarted);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMahjongMultiRoundRoomTest, "GuiyangMahjong.Room.MultiRoundLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMahjongMultiRoundRoomTest::RunTest(const FString& Parameters)
{
    UGuiyangRoomManager* Manager = NewObject<UGuiyangRoomManager>();
    FMahjongCreateRoomRequest Create;
    Create.RoundCount = 2;
    FMahjongRoomState State;
    EMahjongRoomError Error = EMahjongRoomError::None;
    TestTrue(TEXT("Multi-round room is created"), Manager->CreateRoom(TEXT("round-p0"), TEXT("P0"), Create, State, Error));
    const FString RoomCode = State.RoomInfo.RoomId;
    TestEqual(TEXT("Owner is first dealer"), State.RoomInfo.DealerSeat, 0);
    for (int32 Seat = 1; Seat < 4; ++Seat)
    {
        FMahjongJoinRoomRequest Join;
        Join.RoomCode = RoomCode;
        TestTrue(TEXT("Player joins multi-round room"), Manager->JoinRoom(
            FString::Printf(TEXT("round-p%d"), Seat), FString::Printf(TEXT("P%d"), Seat), Join, State, Error));
    }
    for (int32 Seat = 0; Seat < 4; ++Seat)
        TestTrue(TEXT("Player readies for first round"), Manager->ToggleReady(FString::Printf(TEXT("round-p%d"), Seat), State, Error));
    TestEqual(TEXT("Four ready players start first round"), State.Lifecycle, EMahjongRoomLifecycle::Starting);
    TestTrue(TEXT("Room enters first playing round"), Manager->BeginPlaying(RoomCode, State, Error));
    TestEqual(TEXT("Current round increments to one"), State.RoomInfo.CurrentRound, 1);
    TestTrue(TEXT("Active player can be marked disconnected"), Manager->MarkDisconnected(TEXT("round-p1"), State, Error));
    TestFalse(TEXT("Disconnected seat is retained but offline"), State.Seats[1].bOnline);
    FString RetainedRoomCode;
    TestTrue(TEXT("Disconnected player keeps room mapping"), Manager->GetPlayerRoomCode(TEXT("round-p1"), RetainedRoomCode));
    int32 RemainingReconnectSeconds = 0;
    TestTrue(TEXT("Player reconnects within retention window"), Manager->ReconnectPlayer(
        TEXT("round-p1"), State, RemainingReconnectSeconds, Error));
    TestTrue(TEXT("Reconnected seat becomes online"), State.Seats[1].bOnline);
    TestTrue(TEXT("Reconnect snapshot reports positive remaining time"), RemainingReconnectSeconds > 0);

    FMahjongSettlementResult Settlement;
    Settlement.WinnerSeat = 2;
    Settlement.WinningSeats = { 2 };
    Settlement.PlayerResults.SetNum(4);
    const int32 Deltas[] = { -2, -2, 6, -2 };
    for (int32 Seat = 0; Seat < 4; ++Seat)
    {
        Settlement.PlayerResults[Seat].SeatIndex = Seat;
        Settlement.PlayerResults[Seat].TotalDelta = Deltas[Seat];
    }
    FMahjongSettlementResult InvalidSettlement = Settlement;
    InvalidSettlement.PlayerResults[0].TotalDelta = 10;
    TestFalse(TEXT("Non-zero-sum settlement is rejected"), Manager->FinishRound(RoomCode, InvalidSettlement, State, Error));
    TestTrue(TEXT("First round settlement is committed"), Manager->FinishRound(RoomCode, Settlement, State, Error));
    TestEqual(TEXT("Non-final round waits for next-round confirmation"), State.Lifecycle, EMahjongRoomLifecycle::WaitingNextRound);
    TestEqual(TEXT("Winner becomes next dealer"), State.RoomInfo.DealerSeat, 2);
    TestEqual(TEXT("Winner accumulated score is stored"), State.Seats[2].Score, 6);
    TestEqual(TEXT("Loser accumulated score is stored"), State.Seats[0].Score, -2);
    TestFalse(TEXT("Same round cannot be settled twice"), Manager->FinishRound(RoomCode, Settlement, State, Error));

    for (int32 Seat = 0; Seat < 4; ++Seat)
    {
        TestTrue(TEXT("Player confirms next round"), Manager->RequestNextRound(
            FString::Printf(TEXT("round-p%d"), Seat), State, Error));
    }
    TestEqual(TEXT("Four confirmations enter starting"), State.Lifecycle, EMahjongRoomLifecycle::Starting);
    TestTrue(TEXT("Room enters second playing round"), Manager->BeginPlaying(RoomCode, State, Error));
    TestEqual(TEXT("Current round increments to two"), State.RoomInfo.CurrentRound, 2);

    FMahjongSettlementResult DrawSettlement;
    DrawSettlement.bDrawGame = true;
    DrawSettlement.PlayerResults.SetNum(4);
    for (int32 Seat = 0; Seat < 4; ++Seat) DrawSettlement.PlayerResults[Seat].SeatIndex = Seat;
    TestTrue(TEXT("Final draw settlement is committed"), Manager->FinishRound(RoomCode, DrawSettlement, State, Error));
    TestEqual(TEXT("Configured rounds end in final settlement"), State.Lifecycle, EMahjongRoomLifecycle::Settlement);
    TestEqual(TEXT("Draw keeps dealer by default"), State.RoomInfo.DealerSeat, 2);
    TestEqual(TEXT("Accumulated score survives final round"), State.Seats[2].Score, 6);
    const FMahjongFinalSettlementResult FinalResult = UGuiyangRoomManager::BuildFinalSettlement(State);
    TestTrue(TEXT("Final settlement has stable match id"), !FinalResult.MatchId.IsEmpty());
    TestEqual(TEXT("Final settlement contains completed rounds"), FinalResult.CompletedRounds, 2);
    TestEqual(TEXT("Final settlement contains four ranked players"), FinalResult.Players.Num(), 4);
    TestEqual(TEXT("Highest score player ranks first"), FinalResult.Players[0].SeatIndex, 2);
    TestEqual(TEXT("Equal scores use seat order for stable ranking"), FinalResult.Players[1].SeatIndex, 0);
    TestEqual(TEXT("First result has rank one"), FinalResult.Players[0].Rank, 1);
    TestFalse(TEXT("Finished room rejects another round"), Manager->RequestNextRound(TEXT("round-p0"), State, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMahjongAuthoritativeTurnTest, "GuiyangMahjong.Table.AuthoritativeTurnAndReplayGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMahjongAuthoritativeTurnTest::RunTest(const FString& Parameters)
{
    TArray<FMahjongSeatInfo> Seats;
    Seats.SetNum(4);
    for (int32 Index = 0; Index < 4; ++Index)
    {
        Seats[Index].SeatIndex = Index;
        Seats[Index].PlayerId = FString::Printf(TEXT("table-p%d"), Index);
        Seats[Index].PlayerName = FString::Printf(TEXT("牌桌玩家%d"), Index);
        Seats[Index].bOccupied = true;
        Seats[Index].bOnline = true;
    }
    const FGuiyangRuleSnapshot Rules = UGuiyangRuleSnapshotLibrary::CreateSnapshot(FMahjongRuleConfig());
    UMahjongTableEngine* First = NewObject<UMahjongTableEngine>();
    UMahjongTableEngine* Second = NewObject<UMahjongTableEngine>();
    FString Error;
    TestTrue(TEXT("第一张牌桌应成功开局"), First->StartRound(Rules, Seats, 0, 20260716, Error));
    TestTrue(TEXT("第二张牌桌应成功开局"), Second->StartRound(Rules, Seats, 0, 20260716, Error));

    const FMahjongPublicTableState& Initial = First->GetPublicState();
    TestEqual(TEXT("开局阶段为庄家出牌"), Initial.Phase, EMahjongTablePhase::PlayerTurn);
    TestEqual(TEXT("庄家座位为 0"), Initial.CurrentTurnSeat, 0);
    TestEqual(TEXT("108 张牌发完 53 张后剩余 55 张"), Initial.RemainingTileCount, 55);
    TestEqual(TEXT("庄家公开牌数为 14"), Initial.Seats[0].HandTileCount, 14);
    for (int32 Seat = 1; Seat < 4; ++Seat) TestEqual(TEXT("闲家公开牌数为 13"), Initial.Seats[Seat].HandTileCount, 13);

    FMahjongPrivatePlayerState FirstDealer;
    FMahjongPrivatePlayerState SecondDealer;
    TestTrue(TEXT("可读取庄家私有快照"), First->GetPrivateState(0, FirstDealer));
    TestTrue(TEXT("可读取第二桌庄家私有快照"), Second->GetPrivateState(0, SecondDealer));
    TestEqual(TEXT("相同种子产生相同庄家手牌数量"), FirstDealer.Hand.Tiles.Num(), SecondDealer.Hand.Tiles.Num());
    for (int32 Index = 0; Index < FirstDealer.Hand.Tiles.Num(); ++Index)
        TestEqual(TEXT("相同种子必须产生相同牌序"), FirstDealer.Hand.Tiles[Index].UniqueId, SecondDealer.Hand.Tiles[Index].UniqueId);

    FMahjongActionRequest Play;
    Play.Type = EMahjongActionType::Play;
    Play.RoundId = Initial.RoundId;
    Play.TurnId = Initial.TurnId;
    Play.TargetTileId = FirstDealer.Hand.Tiles[0].UniqueId;
    Play.ClientSequence = 1;
    const FMahjongActionResult Played = First->SubmitPlayTile(0, Play);
    TestTrue(TEXT("庄家合法出牌必须成功"), Played.bSuccess);
    TestFalse(TEXT("完全相同的请求不得重放"), First->SubmitPlayTile(0, Play).bSuccess);

    if (First->GetPublicState().Phase == EMahjongTablePhase::WaitingForAction)
    {
        for (int32 Seat = 1; Seat < 4 && First->GetPublicState().Phase == EMahjongTablePhase::WaitingForAction; ++Seat)
        {
            if (First->GetAvailableActions(Seat).IsEmpty()) continue;
            FMahjongActionRequest Pass;
            Pass.Type = EMahjongActionType::Pass;
            Pass.RoundId = First->GetPublicState().RoundId;
            Pass.TurnId = First->GetPublicState().TurnId;
            Pass.ClientSequence = 1;
            TestTrue(TEXT("反应窗口过牌必须成功"), First->SubmitReaction(Seat, Pass).bSuccess);
        }
    }

    const FMahjongPublicTableState& Advanced = First->GetPublicState();
    TestEqual(TEXT("无人声明后轮到下一座位"), Advanced.CurrentTurnSeat, 1);
    TestEqual(TEXT("下一家摸牌后持有 14 张"), Advanced.Seats[1].HandTileCount, 14);
    TestEqual(TEXT("轮转摸牌后牌墙剩余 54 张"), Advanced.RemainingTileCount, 54);
    TestEqual(TEXT("弃牌记录必须由服务端生成"), Advanced.Discards.Num(), 1);

    FMahjongPrivatePlayerState NextPlayer;
    First->GetPrivateState(1, NextPlayer);
    FMahjongActionRequest Forged;
    Forged.Type = EMahjongActionType::Play;
    Forged.RoundId = Advanced.RoundId;
    Forged.TurnId = Advanced.TurnId;
    Forged.TargetTileId = 999999;
    Forged.ClientSequence = 2;
    const int32 SequenceBefore = Advanced.ServerActionSequence;
    TestFalse(TEXT("伪造不属于手牌的牌 ID 必须被拒绝"), First->SubmitPlayTile(1, Forged).bSuccess);
    TestEqual(TEXT("拒绝请求不得推进服务端动作序号"), First->GetPublicState().ServerActionSequence, SequenceBefore);
    const int32 PreviousRoundId = First->GetPublicState().RoundId;
    TestTrue(TEXT("Same engine starts next round"), First->StartRound(Rules, Seats, 1, 20260717, Error));
    TestEqual(TEXT("Round id increases across rounds"), First->GetPublicState().RoundId, PreviousRoundId + 1);
    Forged.ClientSequence = 99;
    TestFalse(TEXT("Request from previous round is rejected"), First->SubmitPlayTile(1, Forged).bSuccess);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMahjongActionTimeoutTest, "GuiyangMahjong.Table.AuthoritativeActionTimeout", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMahjongActionTimeoutTest::RunTest(const FString& Parameters)
{
    TArray<FMahjongSeatInfo> Seats;
    Seats.SetNum(4);
    for (int32 Seat = 0; Seat < 4; ++Seat)
    {
        Seats[Seat].SeatIndex = Seat;
        Seats[Seat].PlayerId = FString::Printf(TEXT("timeout-p%d"), Seat);
        Seats[Seat].bOccupied = true;
    }
    const FGuiyangRuleSnapshot Rules = UGuiyangRuleSnapshotLibrary::CreateSnapshot(FMahjongRuleConfig());
    FString Error;
    UMahjongTableEngine* TurnEngine = NewObject<UMahjongTableEngine>();
    TestTrue(TEXT("Turn-timeout table starts"), TurnEngine->StartRound(Rules, Seats, 0, 106, Error));
    const FMahjongPublicTableState Initial = TurnEngine->GetPublicState();
    TestFalse(TEXT("Stale timeout token is rejected"), TurnEngine->ResolveActionTimeout(
        Initial.RoundId + 1, Initial.TurnId, Initial.Phase).bSuccess);
    TestEqual(TEXT("Stale timeout does not discard"), TurnEngine->GetPublicState().Discards.Num(), 0);
    TestTrue(TEXT("Current turn timeout auto-plays"), TurnEngine->ResolveActionTimeout(
        Initial.RoundId, Initial.TurnId, Initial.Phase).bSuccess);
    TestEqual(TEXT("Turn timeout creates exactly one discard"), TurnEngine->GetPublicState().Discards.Num(), 1);
    FMahjongPrivatePlayerState TimedOutPlayer;
    TurnEngine->GetPrivateState(0, TimedOutPlayer);
    TestEqual(TEXT("Private snapshot carries last accepted sequence"), TimedOutPlayer.LastAcceptedClientSequence, 0);

    UMahjongTableEngine* ReactionEngine = NewObject<UMahjongTableEngine>();
    TestTrue(TEXT("Reaction-timeout table starts"), ReactionEngine->StartRound(Rules, Seats, 0, 107, Error));
    ReactionEngine->SetHandForServerTest(0, MahjongTest::MakeHand({0,1,2,3,4,5,6,7,8,9,10,11,12,13}));
    ReactionEngine->SetHandForServerTest(1, MahjongTest::MakeHand({0,0,3,4,5,6,7,8,9,10,11,12,13}));
    const FMahjongHand NoHu = MahjongTest::MakeHand({6,8,10,12,14,16,18,20,22,24,26,5,7});
    ReactionEngine->SetHandForServerTest(2, NoHu);
    ReactionEngine->SetHandForServerTest(3, NoHu);
    FMahjongActionRequest Play;
    Play.Type = EMahjongActionType::Play;
    Play.RoundId = ReactionEngine->GetPublicState().RoundId;
    Play.TurnId = ReactionEngine->GetPublicState().TurnId;
    Play.TargetTileId = 0;
    Play.ClientSequence = 1;
    TestTrue(TEXT("Discard opens reaction timeout window"), ReactionEngine->SubmitPlayTile(0, Play).bSuccess);
    const FMahjongPublicTableState Waiting = ReactionEngine->GetPublicState();
    TestEqual(TEXT("Peng candidate keeps reaction window open"), Waiting.Phase, EMahjongTablePhase::WaitingForAction);
    TestTrue(TEXT("Reaction timeout auto-passes pending seats"), ReactionEngine->ResolveActionTimeout(
        Waiting.RoundId, Waiting.TurnId, Waiting.Phase).bSuccess);
    TestEqual(TEXT("Auto-pass advances to next player"), ReactionEngine->GetPublicState().CurrentTurnSeat, 1);
    TestEqual(TEXT("Auto-pass does not claim discard"), ReactionEngine->GetPublicState().Discards[0].bClaimed, false);
    TestFalse(TEXT("Expired reaction timeout cannot fire twice"), ReactionEngine->ResolveActionTimeout(
        Waiting.RoundId, Waiting.TurnId, Waiting.Phase).bSuccess);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMahjongReactionPriorityTest, "GuiyangMahjong.Table.ReactionPriorityAndMultiHu", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMahjongReactionPriorityTest::RunTest(const FString& Parameters)
{
    TArray<FMahjongSeatInfo> Seats;
    Seats.SetNum(4);
    for (int32 Index = 0; Index < 4; ++Index)
    {
        Seats[Index].SeatIndex = Index;
        Seats[Index].PlayerId = FString::Printf(TEXT("priority-p%d"), Index);
        Seats[Index].PlayerName = FString::Printf(TEXT("优先级玩家%d"), Index);
        Seats[Index].bOccupied = true;
    }
    UMahjongTableEngine* Engine = NewObject<UMahjongTableEngine>();
    const FGuiyangRuleSnapshot Rules = UGuiyangRuleSnapshotLibrary::CreateSnapshot(FMahjongRuleConfig());
    FString Error;
    TestTrue(TEXT("优先级测试牌桌开局成功"), Engine->StartRound(Rules, Seats, 0, 99, Error));

    FMahjongHand Discarder = MahjongTest::MakeHand({0,1,2,3,4,5,6,7,8,9,10,11,12,13});
    FMahjongHand PengPlayer = MahjongTest::MakeHand({0,0,3,4,5,6,7,8,9,10,11,12,13});
    const FMahjongHand WaitingHu = MahjongTest::MakeHand({1,2, 3,4,5, 9,10,11, 18,18,18, 31,31});
    TestTrue(TEXT("注入出牌者测试手牌"), Engine->SetHandForServerTest(0, Discarder));
    TestTrue(TEXT("注入碰牌候选手牌"), Engine->SetHandForServerTest(1, PengPlayer));
    TestTrue(TEXT("注入第一胡家手牌"), Engine->SetHandForServerTest(2, WaitingHu));
    TestTrue(TEXT("注入第二胡家手牌"), Engine->SetHandForServerTest(3, WaitingHu));

    FMahjongActionRequest Play;
    Play.Type = EMahjongActionType::Play;
    Play.RoundId = Engine->GetPublicState().RoundId;
    Play.TurnId = Engine->GetPublicState().TurnId;
    Play.TargetTileId = Discarder.Tiles[0].UniqueId;
    Play.ClientSequence = 1;
    TestTrue(TEXT("测试牌出牌成功"), Engine->SubmitPlayTile(0, Play).bSuccess);
    TestTrue(TEXT("座位1必须获得碰候选"), Engine->GetAvailableActions(1).ContainsByPredicate([](const FMahjongAction& A) { return A.Type == EMahjongActionType::Peng; }));
    TestTrue(TEXT("座位2必须获得胡候选"), Engine->GetAvailableActions(2).ContainsByPredicate([](const FMahjongAction& A) { return A.Type == EMahjongActionType::Hu; }));
    TestTrue(TEXT("座位3必须获得胡候选"), Engine->GetAvailableActions(3).ContainsByPredicate([](const FMahjongAction& A) { return A.Type == EMahjongActionType::Hu; }));

    for (const TPair<int32, EMahjongActionType> Response : {
        TPair<int32, EMahjongActionType>(1, EMahjongActionType::Peng),
        TPair<int32, EMahjongActionType>(2, EMahjongActionType::Hu),
        TPair<int32, EMahjongActionType>(3, EMahjongActionType::Hu) })
    {
        FMahjongActionRequest Reaction;
        Reaction.Type = Response.Value;
        Reaction.RoundId = Engine->GetPublicState().RoundId;
        Reaction.TurnId = Engine->GetPublicState().TurnId;
        Reaction.ClientSequence = 1;
        TestTrue(TEXT("声明动作必须成功记录"), Engine->SubmitReaction(Response.Key, Reaction).bSuccess);
    }
    TestEqual(TEXT("胡牌必须压过碰并进入结算"), Engine->GetPublicState().Phase, EMahjongTablePhase::Settlement);
    TestEqual(TEXT("默认一炮多响应保留两个胡家"), Engine->GetPublicState().WinningSeats.Num(), 2);
    TestTrue(TEXT("第一胡家在赢家列表"), Engine->GetPublicState().WinningSeats.Contains(2));
    TestTrue(TEXT("第二胡家在赢家列表"), Engine->GetPublicState().WinningSeats.Contains(3));
    TestFalse(TEXT("碰牌玩家不得成为赢家"), Engine->GetPublicState().WinningSeats.Contains(1));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMahjongSelfDrawTest, "GuiyangMahjong.Table.AuthoritativeSelfDraw", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMahjongSelfDrawTest::RunTest(const FString& Parameters)
{
    TArray<FMahjongSeatInfo> Seats;
    Seats.SetNum(4);
    for (int32 Index = 0; Index < 4; ++Index)
    {
        Seats[Index].SeatIndex = Index;
        Seats[Index].PlayerId = FString::Printf(TEXT("self-draw-p%d"), Index);
        Seats[Index].bOccupied = true;
        Seats[Index].Score = 100;
    }
    UMahjongTableEngine* Engine = NewObject<UMahjongTableEngine>();
    FString Error;
    TestTrue(TEXT("Self draw table starts"), Engine->StartRound(
        UGuiyangRuleSnapshotLibrary::CreateSnapshot(FMahjongRuleConfig()), Seats, 0, 101, Error));
    const FMahjongHand WinningHand = MahjongTest::MakeHand({0,1,2, 9,10,11, 18,18,18, 3,4,5, 13,13});
    TestTrue(TEXT("Inject winning hand"), Engine->SetHandForServerTest(0, WinningHand));
    TestTrue(TEXT("Server offers self draw"), Engine->GetAvailableActions(0).ContainsByPredicate(
        [](const FMahjongAction& Action) { return Action.Type == EMahjongActionType::Hu; }));

    FMahjongActionRequest Request;
    Request.Type = EMahjongActionType::Hu;
    Request.RoundId = Engine->GetPublicState().RoundId;
    Request.TurnId = Engine->GetPublicState().TurnId;
    Request.ClientSequence = 1;
    TestTrue(TEXT("Server accepts valid self draw"), Engine->SubmitTurnAction(0, Request).bSuccess);
    TestEqual(TEXT("Self draw enters settlement"), Engine->GetPublicState().Phase, EMahjongTablePhase::Settlement);
    FMahjongSettlementResult Settlement;
    TestTrue(TEXT("Self draw settlement is readable"), Engine->GetSettlementResult(Settlement));
    TestTrue(TEXT("Settlement marks self draw"), Settlement.bSelfDraw);
    TestEqual(TEXT("Dealer is self draw winner"), Settlement.WinnerSeat, 0);
    TestTrue(TEXT("Self draw winner gains base score"), Settlement.PlayerResults[0].BaseScoreDelta > 0);
    TestTrue(TEXT("Winning settlement flips a ji tile"), Settlement.FlippedJiTile.IsValid());
    TestEqual(TEXT("Settlement contains four ji counts"), Settlement.PlayerJiCounts.Num(), 4);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMahjongConcealedGangTest, "GuiyangMahjong.Table.AuthoritativeConcealedGang", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMahjongConcealedGangTest::RunTest(const FString& Parameters)
{
    TArray<FMahjongSeatInfo> Seats;
    Seats.SetNum(4);
    for (int32 Index = 0; Index < 4; ++Index)
    {
        Seats[Index].SeatIndex = Index;
        Seats[Index].PlayerId = FString::Printf(TEXT("gang-p%d"), Index);
        Seats[Index].bOccupied = true;
    }
    UMahjongTableEngine* Engine = NewObject<UMahjongTableEngine>();
    FString Error;
    TestTrue(TEXT("Concealed gang table starts"), Engine->StartRound(
        UGuiyangRuleSnapshotLibrary::CreateSnapshot(FMahjongRuleConfig()), Seats, 0, 102, Error));
    TestTrue(TEXT("Inject concealed gang hand"), Engine->SetHandForServerTest(
        0, MahjongTest::MakeHand({4,4,4,4, 0,1,2, 9,10,11, 18,19,20, 13})));
    const TArray<FMahjongAction> Actions = Engine->GetAvailableActions(0);
    const FMahjongAction* Gang = Actions.FindByPredicate(
        [](const FMahjongAction& Action) { return Action.Type == EMahjongActionType::AnGang; });
    TestNotNull(TEXT("Server offers concealed gang"), Gang);
    if (!Gang) return false;

    FMahjongActionRequest Request;
    Request.Type = EMahjongActionType::AnGang;
    Request.RoundId = Engine->GetPublicState().RoundId;
    Request.TurnId = Engine->GetPublicState().TurnId;
    Request.TargetTileId = Gang->TargetTile.UniqueId;
    Request.ClientSequence = 1;
    TestTrue(TEXT("Server accepts concealed gang"), Engine->SubmitTurnAction(0, Request).bSuccess);
    TestEqual(TEXT("Replacement tile keeps player turn"), Engine->GetPublicState().Phase, EMahjongTablePhase::PlayerTurn);
    TestEqual(TEXT("Concealed gang is in public meld list"), Engine->GetPublicState().PublicMelds.Num(), 1);
    TestFalse(TEXT("Public concealed gang hides every tile face"), Engine->GetPublicState().PublicMelds[0].Tiles.ContainsByPredicate(
        [](const FMahjongTile& Tile) { return Tile.IsValid(); }));
    TestEqual(TEXT("Replacement draw consumes one wall tile"), Engine->GetPublicState().RemainingTileCount, 54);
    TestEqual(TEXT("Four tiles become meld and one replacement remains"), Engine->GetPublicState().Seats[0].HandTileCount, 11);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMahjongSupplementalGangTest, "GuiyangMahjong.Table.AuthoritativeSupplementalGang", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMahjongSupplementalGangTest::RunTest(const FString& Parameters)
{
    TArray<FMahjongSeatInfo> Seats;
    Seats.SetNum(4);
    for (int32 Seat = 0; Seat < 4; ++Seat)
    {
        Seats[Seat].SeatIndex = Seat;
        Seats[Seat].PlayerId = FString::Printf(TEXT("bu-gang-p%d"), Seat);
        Seats[Seat].bOccupied = true;
    }
    FMahjongRuleConfig Config;
    Config.bEnableQiangGangHu = false;
    UMahjongTableEngine* Engine = NewObject<UMahjongTableEngine>();
    FString Error;
    TestTrue(TEXT("Supplemental gang table starts"), Engine->StartRound(
        UGuiyangRuleSnapshotLibrary::CreateSnapshot(Config), Seats, 0, 103, Error));

    FMahjongHand GangHand = MahjongTest::MakeHand({0,1,2,3,4,5,6,7,8,9,10});
    FMahjongMeld Peng;
    Peng.Type = EMahjongMeldType::Peng;
    Peng.FromSeat = 3;
    Peng.Tiles = { MahjongTest::MakeTile(0, 200), MahjongTest::MakeTile(0, 201), MahjongTest::MakeTile(0, 202) };
    GangHand.Melds.Add(Peng);
    TestTrue(TEXT("Inject supplemental gang hand"), Engine->SetHandForServerTest(0, GangHand));
    const FMahjongAction* Candidate = Engine->GetAvailableActions(0).FindByPredicate(
        [](const FMahjongAction& Action) { return Action.Type == EMahjongActionType::BuGang; });
    TestNotNull(TEXT("Server offers supplemental gang"), Candidate);
    if (!Candidate) return false;

    FMahjongActionRequest Request;
    Request.Type = EMahjongActionType::BuGang;
    Request.RoundId = Engine->GetPublicState().RoundId;
    Request.TurnId = Engine->GetPublicState().TurnId;
    Request.TargetTileId = Candidate->TargetTile.UniqueId;
    Request.ClientSequence = 1;
    TestTrue(TEXT("Server accepts supplemental gang"), Engine->SubmitTurnAction(0, Request).bSuccess);
    TestEqual(TEXT("Supplemental gang draws replacement"), Engine->GetPublicState().RemainingTileCount, 54);
    TestEqual(TEXT("Supplemental gang remains player turn"), Engine->GetPublicState().Phase, EMahjongTablePhase::PlayerTurn);
    FMahjongPrivatePlayerState PrivateState;
    Engine->GetPrivateState(0, PrivateState);
    TestEqual(TEXT("Private peng upgrades to supplemental gang"), PrivateState.Hand.Melds[0].Type, EMahjongMeldType::BuGang);
    TestEqual(TEXT("Public peng upgrades to supplemental gang"), Engine->GetPublicState().PublicMelds[0].Type, EMahjongMeldType::BuGang);
    TestEqual(TEXT("Public meld records owner seat"), Engine->GetPublicState().PublicMelds[0].OwnerSeat, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMahjongQiangGangHuTest, "GuiyangMahjong.Table.QiangGangHu", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMahjongQiangGangHuTest::RunTest(const FString& Parameters)
{
    TArray<FMahjongSeatInfo> Seats;
    Seats.SetNum(4);
    for (int32 Seat = 0; Seat < 4; ++Seat)
    {
        Seats[Seat].SeatIndex = Seat;
        Seats[Seat].PlayerId = FString::Printf(TEXT("qiang-gang-p%d"), Seat);
        Seats[Seat].bOccupied = true;
    }
    UMahjongTableEngine* Engine = NewObject<UMahjongTableEngine>();
    FString Error;
    TestTrue(TEXT("Qiang gang table starts"), Engine->StartRound(
        UGuiyangRuleSnapshotLibrary::CreateSnapshot(FMahjongRuleConfig()), Seats, 0, 104, Error));

    FMahjongHand GangHand = MahjongTest::MakeHand({0,1,2,3,4,5,6,7,8,9,10});
    FMahjongMeld Peng;
    Peng.Type = EMahjongMeldType::Peng;
    Peng.FromSeat = 3;
    Peng.Tiles = { MahjongTest::MakeTile(0, 210), MahjongTest::MakeTile(0, 211), MahjongTest::MakeTile(0, 212) };
    GangHand.Melds.Add(Peng);
    const FMahjongHand WaitingHu = MahjongTest::MakeHand({1,2, 3,4,5, 9,10,11, 18,18,18, 13,13});
    const FMahjongHand NoHu = MahjongTest::MakeHand({6,8,10,12,14,16,18,20,22,24,26,5,7});
    Engine->SetHandForServerTest(0, GangHand);
    Engine->SetHandForServerTest(1, WaitingHu);
    Engine->SetHandForServerTest(2, NoHu);
    Engine->SetHandForServerTest(3, NoHu);
    const FMahjongAction* Candidate = Engine->GetAvailableActions(0).FindByPredicate(
        [](const FMahjongAction& Action) { return Action.Type == EMahjongActionType::BuGang; });
    TestNotNull(TEXT("Server offers robbable supplemental gang"), Candidate);
    if (!Candidate) return false;

    FMahjongActionRequest GangRequest;
    GangRequest.Type = EMahjongActionType::BuGang;
    GangRequest.RoundId = Engine->GetPublicState().RoundId;
    GangRequest.TurnId = Engine->GetPublicState().TurnId;
    GangRequest.TargetTileId = Candidate->TargetTile.UniqueId;
    GangRequest.ClientSequence = 1;
    TestTrue(TEXT("Supplemental gang declaration enters response"), Engine->SubmitTurnAction(0, GangRequest).bSuccess);
    TestEqual(TEXT("Qiang gang opens reaction window"), Engine->GetPublicState().Phase, EMahjongTablePhase::WaitingForAction);
    TestTrue(TEXT("Waiting player receives hu action"), Engine->GetAvailableActions(1).ContainsByPredicate(
        [](const FMahjongAction& Action) { return Action.Type == EMahjongActionType::Hu; }));

    for (int32 Seat = 2; Seat < 4; ++Seat)
    {
        if (Engine->GetAvailableActions(Seat).IsEmpty()) continue;
        FMahjongActionRequest Pass;
        Pass.Type = EMahjongActionType::Pass;
        Pass.RoundId = Engine->GetPublicState().RoundId;
        Pass.TurnId = Engine->GetPublicState().TurnId;
        Pass.ClientSequence = 1;
        Engine->SubmitReaction(Seat, Pass);
    }
    FMahjongActionRequest Hu;
    Hu.Type = EMahjongActionType::Hu;
    Hu.RoundId = Engine->GetPublicState().RoundId;
    Hu.TurnId = Engine->GetPublicState().TurnId;
    Hu.ClientSequence = 1;
    TestTrue(TEXT("Server accepts qiang gang hu"), Engine->SubmitReaction(1, Hu).bSuccess);
    TestEqual(TEXT("Qiang gang hu enters settlement"), Engine->GetPublicState().Phase, EMahjongTablePhase::Settlement);
    FMahjongSettlementResult Settlement;
    Engine->GetSettlementResult(Settlement);
    TestEqual(TEXT("Gang declarer is loser"), Settlement.LoserSeat, 0);
    TestEqual(TEXT("Robbing player is winner"), Settlement.WinnerSeat, 1);
    TestTrue(TEXT("Server flips ji tile at settlement"), Settlement.FlippedJiTile.IsValid());
    TestEqual(TEXT("Public flip matches settlement"), Engine->GetPublicState().FlippedJiTile.UniqueId, Settlement.FlippedJiTile.UniqueId);
    TestEqual(TEXT("Settlement publishes four ji counts"), Settlement.PlayerJiCounts.Num(), 4);
    FMahjongPrivatePlayerState GangPrivate;
    Engine->GetPrivateState(0, GangPrivate);
    TestEqual(TEXT("Robbed meld remains peng"), GangPrivate.Hand.Melds[0].Type, EMahjongMeldType::Peng);
    TestFalse(TEXT("Robbed tile leaves declarer hand"), GangPrivate.Hand.Tiles.ContainsByPredicate(
        [](const FMahjongTile& Tile) { return Tile.UniqueId == 0; }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMahjongHuTest, "GuiyangMahjong.Rules.StandardHu", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMahjongHuTest::RunTest(const FString& Parameters)
{
    const FMahjongHand Valid = MahjongTest::MakeHand({0,1,2, 9,10,11, 18,18,18, 27,27,27, 31,31});
    const FMahjongHand Invalid = MahjongTest::MakeHand({0,1,2,3,4,5,6,7,8,9,10,11,12,13});
    TestTrue(TEXT("标准四组加一对必须可胡"), UMahjongHuChecker::CanHu(Valid, false));
    TestFalse(TEXT("无将牌的牌型不能胡"), UMahjongHuChecker::CanHu(Invalid, false));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMahjongQiDuiTest, "GuiyangMahjong.Rules.QiDuiSwitch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMahjongQiDuiTest::RunTest(const FString& Parameters)
{
    const FMahjongHand QiDui = MahjongTest::MakeHand({0,0, 2,2, 9,9, 11,11, 18,18, 27,27, 31,31});
    TestTrue(TEXT("七对开关开启时可胡"), UMahjongHuChecker::CanHu(QiDui, true));
    TestFalse(TEXT("七对开关关闭时不可按七对胡"), UMahjongHuChecker::CanHu(QiDui, false));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMahjongActionCheckTest, "GuiyangMahjong.Rules.PengGang", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMahjongActionCheckTest::RunTest(const FString& Parameters)
{
    FMahjongHand Hand = MahjongTest::MakeHand({0,0,0, 4,4,4,4, 9,10, 11,12, 13,14,15});
    const FMahjongTile Discard = MahjongTest::MakeTile(0, 100);
    TestTrue(TEXT("两张同牌可碰"), UMahjongChiPengChecker::CanPeng(Hand, Discard));
    TestTrue(TEXT("三张同牌可明杠"), UMahjongGangChecker::CanMingGang(Hand, Discard));
    TestTrue(TEXT("四张同牌可暗杠"), UMahjongGangChecker::FindAnGangRuleIndices(Hand).Contains(4));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMahjongSpecialJiRuleTest, "GuiyangMahjong.Rules.SpecialJiConfig", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMahjongSpecialJiRuleTest::RunTest(const FString& Parameters)
{
    FMahjongRuleConfig Config;
    Config.WuGuJiValue = 3;
    Config.JiCountingScope = EMahjongJiCountingScope::HandOnly;
    FMahjongHand Hand = MahjongTest::MakeHand({25, 0,1,2,3,4,5,6,7,8,10,11,12,13});
    FMahjongMeld Meld;
    Meld.Type = EMahjongMeldType::Peng;
    Meld.Tiles = { MahjongTest::MakeTile(25, 100), MahjongTest::MakeTile(25, 101), MahjongTest::MakeTile(25, 102) };
    Hand.Melds.Add(Meld);
    TestTrue(TEXT("Eight dots is WuGu Ji"), UGuiyangJiCalculator::IsWuGuJi(Hand.Tiles[0]));
    TestEqual(TEXT("Hand-only scope ignores meld WuGu Ji"),
        UGuiyangJiCalculator::CountJiUnits(Hand, FMahjongTile(), Config), 3);
    Config.JiCountingScope = EMahjongJiCountingScope::HandAndMeld;
    TestEqual(TEXT("Hand-and-meld scope counts configured WuGu units"),
        UGuiyangJiCalculator::CountJiUnits(Hand, FMahjongTile(), Config), 12);
    const FGuiyangRuleSnapshot FirstSnapshot = UGuiyangRuleSnapshotLibrary::CreateSnapshot(Config);
    Config.WuGuJiValue = 4;
    const FGuiyangRuleSnapshot SecondSnapshot = UGuiyangRuleSnapshotLibrary::CreateSnapshot(Config);
    TestNotEqual(TEXT("Special Ji values participate in rule hash"), FirstSnapshot.RuleHash, SecondSnapshot.RuleHash);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMahjongSpecialJiEventTest, "GuiyangMahjong.Table.ChongFengAndZeRenJi", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMahjongSpecialJiEventTest::RunTest(const FString& Parameters)
{
    TArray<FMahjongSeatInfo> Seats;
    Seats.SetNum(4);
    for (int32 Seat = 0; Seat < 4; ++Seat)
    {
        Seats[Seat].SeatIndex = Seat;
        Seats[Seat].PlayerId = FString::Printf(TEXT("special-ji-p%d"), Seat);
        Seats[Seat].bOccupied = true;
        Seats[Seat].Score = 100;
    }
    FMahjongRuleConfig Config;
    Config.JiScore = 2;
    Config.ChongFengJiValue = 2;
    Config.ZeRenJiValue = 3;
    UMahjongTableEngine* Engine = NewObject<UMahjongTableEngine>();
    FString Error;
    TestTrue(TEXT("Special Ji table starts"), Engine->StartRound(
        UGuiyangRuleSnapshotLibrary::CreateSnapshot(Config), Seats, 0, 105, Error));

    Engine->SetHandForServerTest(0, MahjongTest::MakeHand({9,0,1,2,3,4,5,6,7,8,10,11,12,13}));
    Engine->SetHandForServerTest(1, MahjongTest::MakeHand({9,9,0,2,4,6,8,10,12,14,16,18,20}));
    const FMahjongHand NoHu = MahjongTest::MakeHand({6,8,10,12,14,16,18,20,22,24,26,5,7});
    Engine->SetHandForServerTest(2, NoHu);
    Engine->SetHandForServerTest(3, NoHu);

    FMahjongActionRequest Play;
    Play.Type = EMahjongActionType::Play;
    Play.RoundId = Engine->GetPublicState().RoundId;
    Play.TurnId = Engine->GetPublicState().TurnId;
    Play.TargetTileId = 0;
    Play.ClientSequence = 1;
    TestTrue(TEXT("First basic Ji discard succeeds"), Engine->SubmitPlayTile(0, Play).bSuccess);
    TestEqual(TEXT("First Ji discard records ChongFeng event"), Engine->GetPublicState().JiEvents.Num(), 1);
    TestEqual(TEXT("ChongFeng event belongs to discarder"), Engine->GetPublicState().JiEvents[0].ActorSeat, 0);

    for (int32 Seat = 2; Seat < 4; ++Seat)
    {
        if (Engine->GetAvailableActions(Seat).IsEmpty()) continue;
        FMahjongActionRequest Pass;
        Pass.Type = EMahjongActionType::Pass;
        Pass.RoundId = Engine->GetPublicState().RoundId;
        Pass.TurnId = Engine->GetPublicState().TurnId;
        Pass.ClientSequence = 1;
        Engine->SubmitReaction(Seat, Pass);
    }
    FMahjongActionRequest PengRequest;
    PengRequest.Type = EMahjongActionType::Peng;
    PengRequest.RoundId = Engine->GetPublicState().RoundId;
    PengRequest.TurnId = Engine->GetPublicState().TurnId;
    PengRequest.ClientSequence = 1;
    TestTrue(TEXT("Other player claims first Ji by Peng"), Engine->SubmitReaction(1, PengRequest).bSuccess);
    TestEqual(TEXT("Peng creates ZeRen event"), Engine->GetPublicState().JiEvents.Num(), 2);
    TestEqual(TEXT("ZeRen event targets discarder"), Engine->GetPublicState().JiEvents[1].TargetSeat, 0);

    FMahjongPrivatePlayerState Claimant;
    Engine->GetPrivateState(1, Claimant);
    Claimant.Hand.Tiles = MahjongTest::MakeHand({0,1,2, 3,4,5, 18,18,18, 13,13}).Tiles;
    Engine->SetHandForServerTest(1, Claimant.Hand);
    FMahjongActionRequest Hu;
    Hu.Type = EMahjongActionType::Hu;
    Hu.RoundId = Engine->GetPublicState().RoundId;
    Hu.TurnId = Engine->GetPublicState().TurnId;
    Hu.ClientSequence = 2;
    TestTrue(TEXT("Claimant self draw settles special Ji"), Engine->SubmitTurnAction(1, Hu).bSuccess);
    FMahjongSettlementResult Settlement;
    Engine->GetSettlementResult(Settlement);
    TestEqual(TEXT("Settlement carries both special Ji events"), Settlement.JiEvents.Num(), 2);
    TestEqual(TEXT("Responsibility payer receives configured negative delta"),
        Settlement.PlayerResults[0].SpecialJiScoreDelta, -6);
    TestEqual(TEXT("Responsibility claimant receives configured positive delta"),
        Settlement.PlayerResults[1].SpecialJiScoreDelta, 6);
    int32 TotalDelta = 0;
    for (const FMahjongPlayerScoreResult& Player : Settlement.PlayerResults) TotalDelta += Player.TotalDelta;
    TestEqual(TEXT("Special Ji settlement remains zero sum"), TotalDelta, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMahjongJiTest, "GuiyangMahjong.Rules.Ji", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMahjongJiTest::RunTest(const FString& Parameters)
{
    const FMahjongTile OneBamboo = MahjongTest::MakeTile(9, 1);
    const FMahjongTile NineCharacters = MahjongTest::MakeTile(8, 2);
    TestTrue(TEXT("幺鸡必须识别一条"), UGuiyangJiCalculator::IsBasicJi(OneBamboo));
    TestEqual(TEXT("九万翻鸡循环到一万"), UGuiyangJiCalculator::GetFlippedJiRuleIndex(NineCharacters), 0);
    const FMahjongHand Hand = MahjongTest::MakeHand({9,9,0,0,1,2,3,4,5,6,7,8,27,27});
    TestEqual(TEXT("两张幺鸡加两张翻鸡"), UGuiyangJiCalculator::CountJi(Hand, NineCharacters), 4);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMahjongScoreTest, "GuiyangMahjong.Rules.ScoreZeroSum", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMahjongScoreTest::RunTest(const FString& Parameters)
{
    FMahjongRuleConfig Config;
    const TArray<int32> JiCounts = {1, 2, 0, 3};
    const TArray<int32> GangDeltas = {0, 0, 0, 0};
    const TArray<int32> Scores = {100, 100, 100, 100};
    const FMahjongSettlementResult Result = UMahjongScoreCalculator::CalculateWin(1, 3, false, JiCounts, GangDeltas, Scores, Config);
    int32 TotalDelta = 0;
    for (const FMahjongPlayerScoreResult& Player : Result.PlayerResults) TotalDelta += Player.TotalDelta;
    TestEqual(TEXT("四名玩家总分变化必须为零"), TotalDelta, 0);
    TestTrue(TEXT("赢家基础分必须增加"), Result.PlayerResults[1].BaseScoreDelta > 0);
    TestTrue(TEXT("放炮玩家基础分必须减少"), Result.PlayerResults[3].BaseScoreDelta < 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMahjongMultiScoreTest, "GuiyangMahjong.Rules.MultiWinScoreZeroSum", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMahjongMultiScoreTest::RunTest(const FString& Parameters)
{
    const TArray<int32> JiCounts = {1, 2, 0, 3};
    const TArray<int32> GangDeltas = {0, 0, 0, 0};
    const TArray<int32> Scores = {100, 100, 100, 100};
    const FMahjongSettlementResult Result = UMahjongScoreCalculator::CalculateWins(
        {1, 2}, 3, false, JiCounts, GangDeltas, Scores, FMahjongRuleConfig());
    int32 TotalDelta = 0;
    for (const FMahjongPlayerScoreResult& Player : Result.PlayerResults) TotalDelta += Player.TotalDelta;
    TestEqual(TEXT("Multi-win settlement remains zero sum"), TotalDelta, 0);
    TestEqual(TEXT("Multi-win settlement keeps both winners"), Result.WinningSeats.Num(), 2);
    TestTrue(TEXT("First winner gains base score"), Result.PlayerResults[1].BaseScoreDelta > 0);
    TestTrue(TEXT("Second winner gains base score"), Result.PlayerResults[2].BaseScoreDelta > 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMahjongGuestLoginTest, "GuiyangMahjong.Auth.GuestLoginLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMahjongGuestLoginTest::RunTest(const FString& Parameters)
{
    static const FString SlotName = TEXT("GuiyangLoginSettings");
    UGameplayStatics::DeleteGameInSlot(SlotName, 0);
    UGuiyangLoginSaveGame* SeedSettings = Cast<UGuiyangLoginSaveGame>(UGameplayStatics::CreateSaveGameObject(UGuiyangLoginSaveGame::StaticClass()));
    UGameplayStatics::SaveGameToSlot(SeedSettings, SlotName, 0);

    UGameInstance* GameInstance = NewObject<UGameInstance>();
    GameInstance->Init();
    UGuiyangLoginSubsystem* Login = GameInstance->GetSubsystem<UGuiyangLoginSubsystem>();
    TestNotNull(TEXT("登录子系统必须可由 GameInstance 创建"), Login);
    if (Login)
    {
        Login->LoginAsGuest();
        TestEqual(TEXT("游客登录后状态必须为已登录"), Login->GetLoginState(), EGuiyangLoginState::LoggedIn);
        TestTrue(TEXT("游客会话必须有效"), Login->IsSessionValid());
        TestEqual(TEXT("Provider 必须为游客"), Login->GetCurrentProfile().Provider, EGuiyangLoginProvider::Guest);
        TestTrue(TEXT("PlayerId 必须生成"), !Login->GetCurrentProfile().PlayerId.IsEmpty());
        TestTrue(TEXT("内存 SessionToken 必须生成"), !Login->GetSessionTokenForNetwork().IsEmpty());

        Login->Logout();
        TestEqual(TEXT("退出后状态必须为未登录"), Login->GetLoginState(), EGuiyangLoginState::LoggedOut);
        TestFalse(TEXT("退出后会话必须失效"), Login->IsSessionValid());
        TestTrue(TEXT("退出后内存 SessionToken 必须清空"), Login->GetSessionTokenForNetwork().IsEmpty());
    }
    GameInstance->Shutdown();
    UGameplayStatics::DeleteGameInSlot(SlotName, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMahjongMatchHistoryTest, "GuiyangMahjong.History.FinalSettlementPersistence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMahjongMatchHistoryTest::RunTest(const FString& Parameters)
{
    static const FString SlotName = TEXT("GuiyangMatchHistory");
    UGameplayStatics::DeleteGameInSlot(SlotName, 0);

    UGameInstance* FirstInstance = NewObject<UGameInstance>();
    FirstInstance->Init();
    UGuiyangMatchHistorySubsystem* FirstHistory = FirstInstance->GetSubsystem<UGuiyangMatchHistorySubsystem>();
    TestNotNull(TEXT("Match history subsystem is available"), FirstHistory);
    FMahjongFinalSettlementResult Result;
    Result.MatchId = TEXT("match-history-test-1");
    Result.RoomId = TEXT("654321");
    Result.CompletedRounds = 4;
    FMahjongFinalPlayerResult Player;
    Player.Rank = 1;
    Player.SeatIndex = 0;
    Player.PlayerName = TEXT("Winner");
    Player.TotalScore = 18;
    Result.Players.Add(Player);
    TestTrue(TEXT("Final settlement is persisted"), FirstHistory && FirstHistory->RecordFinalSettlement(Result));
    TestFalse(TEXT("Duplicate match id is ignored"), FirstHistory && FirstHistory->RecordFinalSettlement(Result));
    TestEqual(TEXT("Only one history record exists"), FirstHistory ? FirstHistory->GetRecords().Num() : 0, 1);
    FirstInstance->Shutdown();

    UGameInstance* SecondInstance = NewObject<UGameInstance>();
    SecondInstance->Init();
    UGuiyangMatchHistorySubsystem* SecondHistory = SecondInstance->GetSubsystem<UGuiyangMatchHistorySubsystem>();
    TestEqual(TEXT("History reloads from SaveGame"), SecondHistory ? SecondHistory->GetRecords().Num() : 0, 1);
    if (SecondHistory)
    {
        TestEqual(TEXT("Reloaded history keeps match id"), SecondHistory->GetRecords()[0].MatchId, Result.MatchId);
        SecondHistory->ClearHistory();
    }
    SecondInstance->Shutdown();
    UGameplayStatics::DeleteGameInSlot(SlotName, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMahjongLoginPersistenceSecurityTest, "GuiyangMahjong.Security.LoginSaveContainsNoSecrets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMahjongLoginPersistenceSecurityTest::RunTest(const FString& Parameters)
{
    bool bContainsSecretField = false;
    for (TFieldIterator<FProperty> It(UGuiyangLoginSaveGame::StaticClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
    {
        const FString Name = It->GetName();
        bContainsSecretField |= Name.Contains(TEXT("Token"), ESearchCase::IgnoreCase)
            || Name.Contains(TEXT("Secret"), ESearchCase::IgnoreCase)
            || Name.Contains(TEXT("Password"), ESearchCase::IgnoreCase);
    }
    TestFalse(TEXT("登录 SaveGame 不得声明 Token、Secret 或 Password 字段"), bContainsSecretField);
    bool bHistoryContainsSecretField = false;
    for (TFieldIterator<FProperty> It(UMahjongMatchHistorySaveGame::StaticClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
    {
        const FString Name = It->GetName();
        bHistoryContainsSecretField |= Name.Contains(TEXT("Token"), ESearchCase::IgnoreCase)
            || Name.Contains(TEXT("Secret"), ESearchCase::IgnoreCase)
            || Name.Contains(TEXT("Password"), ESearchCase::IgnoreCase)
            || Name.Contains(TEXT("Hand"), ESearchCase::IgnoreCase);
    }
    TestFalse(TEXT("Match history SaveGame must not declare secrets or hands"), bHistoryContainsSecretField);

    bool bPublicStateContainsHand = false;
    for (TFieldIterator<FProperty> It(FMahjongPublicTableState::StaticStruct(), EFieldIteratorFlags::IncludeSuper); It; ++It)
    {
        bPublicStateContainsHand |= It->GetName().Contains(TEXT("Hand"), ESearchCase::IgnoreCase);
    }
    TestFalse(TEXT("公共牌桌状态不得包含私有手牌字段"), bPublicStateContainsHand);
    bool bReconnectSnapshotContainsCredential = false;
    for (TFieldIterator<FProperty> It(FMahjongReconnectSnapshot::StaticStruct(), EFieldIteratorFlags::IncludeSuper); It; ++It)
    {
        const FString Name = It->GetName();
        bReconnectSnapshotContainsCredential |= Name.Contains(TEXT("Token"), ESearchCase::IgnoreCase)
            || Name.Contains(TEXT("Secret"), ESearchCase::IgnoreCase)
            || Name.Contains(TEXT("Password"), ESearchCase::IgnoreCase);
    }
    TestFalse(TEXT("Reconnect snapshot must not expose credentials"), bReconnectSnapshotContainsCredential);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMahjongWechatAutoLoginTest, "GuiyangMahjong.Auth.SimulatedWechatAndAutoLogin", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMahjongWechatAutoLoginTest::RunTest(const FString& Parameters)
{
    static const FString SlotName = TEXT("GuiyangLoginSettings");
    UGameplayStatics::DeleteGameInSlot(SlotName, 0);
    UGuiyangLoginSaveGame* SeedSettings = Cast<UGuiyangLoginSaveGame>(UGameplayStatics::CreateSaveGameObject(UGuiyangLoginSaveGame::StaticClass()));
    UGameplayStatics::SaveGameToSlot(SeedSettings, SlotName, 0);

    UGameInstance* FirstInstance = NewObject<UGameInstance>();
    FirstInstance->Init();
    UGuiyangLoginSubsystem* FirstLogin = FirstInstance->GetSubsystem<UGuiyangLoginSubsystem>();
    FirstLogin->LoginWithWechat();
#if PLATFORM_WINDOWS
    TestEqual(TEXT("Windows 微信入口必须明确使用模拟 Provider"), FirstLogin->GetCurrentProfile().Provider, EGuiyangLoginProvider::SimulatedWechat);
    const FString FirstPlayerId = FirstLogin->GetCurrentProfile().PlayerId;
    TestTrue(TEXT("模拟微信 PlayerId 必须生成"), !FirstPlayerId.IsEmpty());
    FirstInstance->Shutdown();

    UGameInstance* SecondInstance = NewObject<UGameInstance>();
    SecondInstance->Init();
    UGuiyangLoginSubsystem* SecondLogin = SecondInstance->GetSubsystem<UGuiyangLoginSubsystem>();
    SecondLogin->TryAutoLogin();
    TestTrue(TEXT("模拟微信账号必须支持自动登录"), SecondLogin->IsSessionValid());
    TestEqual(TEXT("自动登录必须恢复同一匿名账号标识"), SecondLogin->GetCurrentProfile().PlayerId, FirstPlayerId);
    SecondLogin->Logout();
    SecondInstance->Shutdown();
#else
    TestEqual(TEXT("非 Windows 未配置正式微信时必须保持未登录"), FirstLogin->GetLoginState(), EGuiyangLoginState::LoggedOut);
    FirstInstance->Shutdown();
#endif
    UGameplayStatics::DeleteGameInSlot(SlotName, 0);
    return true;
}

#endif
