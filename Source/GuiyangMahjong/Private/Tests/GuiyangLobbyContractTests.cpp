#include "Misc/AutomationTest.h"

#include "Lobby/GuiyangLobbySubsystem.h"
#include "Lobby/GuiyangLobbyTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGuiyangLobbyBackendModeContractTest,
    "GuiyangMahjong.Lobby.BackendModeContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGuiyangLobbyBackendModeContractTest::RunTest(const FString& Parameters)
{
    EGuiyangLobbyBackendMode Mode = EGuiyangLobbyBackendMode::RemoteLobby;
    TestTrue(TEXT("必须识别 LocalLegacy"),
        UGuiyangLobbySubsystem::TryParseBackendMode(TEXT("LocalLegacy"), Mode));
    TestEqual(TEXT("LocalLegacy 映射必须稳定"), Mode, EGuiyangLobbyBackendMode::LocalLegacy);

    TestTrue(TEXT("模式解析必须忽略大小写和两端空白"),
        UGuiyangLobbySubsystem::TryParseBackendMode(TEXT("  remotelobby  "), Mode));
    TestEqual(TEXT("RemoteLobby 映射必须稳定"), Mode, EGuiyangLobbyBackendMode::RemoteLobby);

    TestFalse(TEXT("未知模式必须被拒绝"),
        UGuiyangLobbySubsystem::TryParseBackendMode(TEXT("SilentFallback"), Mode));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGuiyangLobbyDtoContractTest,
    "GuiyangMahjong.Lobby.DtoContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGuiyangLobbyDtoContractTest::RunTest(const FString& Parameters)
{
    FGuiyangGameServerRoute Route;
    Route.ServerIP = TEXT("127.0.0.1");
    Route.ServerPort = 7777;
    TestTrue(TEXT("合法服务器端点必须可用"), Route.HasValidEndpoint());

    Route.ServerPort = 0;
    TestFalse(TEXT("端口 0 必须被拒绝"), Route.HasValidEndpoint());
    Route.ServerPort = 7777;
    Route.ServerIP.Reset();
    TestFalse(TEXT("空服务器地址必须被拒绝"), Route.HasValidEndpoint());

    FGuiyangLobbyOperationResult Result;
    TestFalse(TEXT("请求结果默认必须为未接受"), Result.bAccepted);
    TestEqual(TEXT("请求结果默认错误码必须为 None"), Result.ErrorCode, EGuiyangLobbyErrorCode::None);
    return true;
}

#endif

