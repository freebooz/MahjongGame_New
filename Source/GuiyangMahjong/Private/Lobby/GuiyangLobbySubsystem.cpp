#include "Lobby/GuiyangLobbySubsystem.h"

#include "Game/GuiyangMahjongPlayerController.h"
#include "GuiyangMahjong.h"
#include "Lobby/GuiyangLobbyBackend.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"

namespace GuiyangLobbyPrivate
{
    constexpr const TCHAR* ConfigSection = TEXT("/Script/GuiyangMahjong.GuiyangLobbySubsystem");

    class FLocalLegacyLobbyBackend final : public ILobbyBackend
    {
    public:
        virtual EGuiyangLobbyBackendMode GetMode() const override
        {
            return EGuiyangLobbyBackendMode::LocalLegacy;
        }

        virtual FGuiyangLobbyOperationResult QuickStart(
            AGuiyangMahjongPlayerController& PlayerController, const FString& RequestId) override
        {
            PlayerController.Server_RequestQuickStart();
            return MakeAccepted(RequestId);
        }

        virtual FGuiyangLobbyOperationResult CreateRoom(
            AGuiyangMahjongPlayerController& PlayerController, const FMahjongCreateRoomRequest& Request,
            const FString& RequestId) override
        {
            PlayerController.Server_RequestCreateRoomWithConfig(Request);
            return MakeAccepted(RequestId);
        }

        virtual FGuiyangLobbyOperationResult JoinRoom(
            AGuiyangMahjongPlayerController& PlayerController, const FMahjongJoinRoomRequest& Request,
            const FString& RequestId) override
        {
            PlayerController.Server_RequestJoinRoomByCode(Request);
            return MakeAccepted(RequestId);
        }

    private:
        static FGuiyangLobbyOperationResult MakeAccepted(const FString& RequestId)
        {
            FGuiyangLobbyOperationResult Result;
            Result.bAccepted = true;
            Result.RequestId = RequestId;
            Result.ErrorCode = EGuiyangLobbyErrorCode::None;
            Result.ChineseMessage = TEXT("请求已提交");
            return Result;
        }
    };
}

void UGuiyangLobbySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    FString ConfiguredMode = TEXT("LocalLegacy");
    if (GConfig)
    {
        GConfig->GetString(GuiyangLobbyPrivate::ConfigSection, TEXT("BackendMode"), ConfiguredMode, GGameIni);
    }

    FString CommandLineMode;
    if (FParse::Value(FCommandLine::Get(), TEXT("MahjongLobbyBackend="), CommandLineMode))
    {
        ConfiguredMode = MoveTemp(CommandLineMode);
    }

    if (!TryParseBackendMode(ConfiguredMode, BackendMode))
    {
        UE_LOG(LogMahjongNet, Error,
            TEXT("大厅后端配置无效：BackendMode=%s，已安全使用 LocalLegacy"), *ConfiguredMode);
        BackendMode = EGuiyangLobbyBackendMode::LocalLegacy;
    }

    if (BackendMode == EGuiyangLobbyBackendMode::LocalLegacy)
    {
        Backend = MakeUnique<GuiyangLobbyPrivate::FLocalLegacyLobbyBackend>();
    }
    else
    {
        // 阶段 1 只冻结协议边界；远程实现完成前必须明确拒绝，避免误连或静默降级。
        Backend.Reset();
    }

    UE_LOG(LogMahjongNet, Log, TEXT("大厅子系统初始化完成：BackendMode=%s"), GetBackendModeName(BackendMode));
}

void UGuiyangLobbySubsystem::Deinitialize()
{
    Backend.Reset();
    Super::Deinitialize();
}

FGuiyangLobbyOperationResult UGuiyangLobbySubsystem::RequestQuickStart(APlayerController* PlayerController)
{
    const FString RequestId = MakeRequestId();
    AGuiyangMahjongPlayerController* MahjongController = Cast<AGuiyangMahjongPlayerController>(PlayerController);
    if (!MahjongController)
    {
        return RejectRequest(RequestId, EGuiyangLobbyErrorCode::InvalidRequest, TEXT("当前玩家控制器不可用"));
    }
    if (!Backend)
    {
        return RejectRequest(RequestId, EGuiyangLobbyErrorCode::BackendNotConfigured,
            TEXT("远程大厅尚未配置，请切换到本地兼容模式"));
    }

    FGuiyangLobbyOperationResult Result = Backend->QuickStart(*MahjongController, RequestId);
    OnRequestSubmitted.Broadcast(RequestId, BackendMode);
    return Result;
}

FGuiyangLobbyOperationResult UGuiyangLobbySubsystem::RequestCreateRoom(
    APlayerController* PlayerController, const FMahjongCreateRoomRequest& Request)
{
    const FString RequestId = MakeRequestId();
    AGuiyangMahjongPlayerController* MahjongController = Cast<AGuiyangMahjongPlayerController>(PlayerController);
    if (!MahjongController)
    {
        return RejectRequest(RequestId, EGuiyangLobbyErrorCode::InvalidRequest, TEXT("当前玩家控制器不可用"));
    }
    if (!Backend)
    {
        return RejectRequest(RequestId, EGuiyangLobbyErrorCode::BackendNotConfigured,
            TEXT("远程大厅尚未配置，请切换到本地兼容模式"));
    }

    FGuiyangLobbyOperationResult Result = Backend->CreateRoom(*MahjongController, Request, RequestId);
    OnRequestSubmitted.Broadcast(RequestId, BackendMode);
    return Result;
}

FGuiyangLobbyOperationResult UGuiyangLobbySubsystem::RequestJoinRoom(
    APlayerController* PlayerController, const FMahjongJoinRoomRequest& Request)
{
    const FString RequestId = MakeRequestId();
    AGuiyangMahjongPlayerController* MahjongController = Cast<AGuiyangMahjongPlayerController>(PlayerController);
    if (!MahjongController)
    {
        return RejectRequest(RequestId, EGuiyangLobbyErrorCode::InvalidRequest, TEXT("当前玩家控制器不可用"));
    }
    if (!Backend)
    {
        return RejectRequest(RequestId, EGuiyangLobbyErrorCode::BackendNotConfigured,
            TEXT("远程大厅尚未配置，请切换到本地兼容模式"));
    }

    FGuiyangLobbyOperationResult Result = Backend->JoinRoom(*MahjongController, Request, RequestId);
    OnRequestSubmitted.Broadcast(RequestId, BackendMode);
    return Result;
}

bool UGuiyangLobbySubsystem::TryParseBackendMode(const FString& Value, EGuiyangLobbyBackendMode& OutMode)
{
    const FString Normalized = Value.TrimStartAndEnd();
    if (Normalized.Equals(TEXT("LocalLegacy"), ESearchCase::IgnoreCase))
    {
        OutMode = EGuiyangLobbyBackendMode::LocalLegacy;
        return true;
    }
    if (Normalized.Equals(TEXT("RemoteLobby"), ESearchCase::IgnoreCase))
    {
        OutMode = EGuiyangLobbyBackendMode::RemoteLobby;
        return true;
    }
    return false;
}

const TCHAR* UGuiyangLobbySubsystem::GetBackendModeName(const EGuiyangLobbyBackendMode Mode)
{
    return Mode == EGuiyangLobbyBackendMode::RemoteLobby ? TEXT("RemoteLobby") : TEXT("LocalLegacy");
}

FString UGuiyangLobbySubsystem::MakeRequestId() const
{
    return FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
}

FGuiyangLobbyOperationResult UGuiyangLobbySubsystem::RejectRequest(
    const FString& RequestId, const EGuiyangLobbyErrorCode ErrorCode, const FString& ChineseMessage)
{
    FGuiyangLobbyOperationResult Result;
    Result.RequestId = RequestId;
    Result.ErrorCode = ErrorCode;
    Result.ChineseMessage = ChineseMessage;
    UE_LOG(LogMahjongNet, Warning, TEXT("大厅请求被拒绝：RequestId=%s，ErrorCode=%d，原因=%s"),
        *RequestId, static_cast<int32>(ErrorCode), *ChineseMessage);
    OnRequestFailed.Broadcast(RequestId, ErrorCode, ChineseMessage);
    return Result;
}

