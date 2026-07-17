#include "Auth/GuiyangLoginSubsystem.h"

#include "Auth/GuiyangLoginSaveGame.h"
#include "Engine/World.h"
#include "GuiyangMahjong.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"

FString FGuiyangLoginProfile::GetProviderDisplayName() const
{
    switch (Provider)
    {
    case EGuiyangLoginProvider::Guest: return TEXT("游客登录");
    case EGuiyangLoginProvider::SimulatedWechat: return TEXT("模拟微信登录");
    case EGuiyangLoginProvider::Wechat: return TEXT("微信登录");
    default: return TEXT("未登录");
    }
}

void UGuiyangLoginSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
    {
        LoginSettings = Cast<UGuiyangLoginSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));
    }
    if (!LoginSettings)
    {
        LoginSettings = Cast<UGuiyangLoginSaveGame>(UGameplayStatics::CreateSaveGameObject(UGuiyangLoginSaveGame::StaticClass()));
    }
    UE_LOG(LogMahjongNet, Log, TEXT("登录子系统初始化完成，本地自动登录配置=%s"), LoginSettings && LoginSettings->bAutoLogin ? TEXT("开启") : TEXT("关闭"));
}

void UGuiyangLoginSubsystem::Deinitialize()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PendingLoginTimer);
    }
    SessionToken.Reset();
    CurrentProfile = {};
    LoginState = EGuiyangLoginState::LoggedOut;
    Super::Deinitialize();
}

void UGuiyangLoginSubsystem::TryAutoLogin()
{
    if (LoginState != EGuiyangLoginState::LoggedOut || !LoginSettings || !LoginSettings->bAutoLogin)
    {
        return;
    }
    if (LoginSettings->SavedProvider == EGuiyangLoginProvider::Guest || LoginSettings->SavedProvider == EGuiyangLoginProvider::SimulatedWechat)
    {
        UE_LOG(LogMahjongNet, Log, TEXT("开始自动登录：Provider=%s"), LoginSettings->SavedProvider == EGuiyangLoginProvider::Guest ? TEXT("游客") : TEXT("模拟微信"));
        BeginLogin(LoginSettings->SavedProvider, LoginSettings->SavedPlayerId, LoginSettings->SavedDisplayName);
    }
}

void UGuiyangLoginSubsystem::LoginAsGuest()
{
    BeginLogin(EGuiyangLoginProvider::Guest);
}

void UGuiyangLoginSubsystem::LoginWithWechat()
{
#if PLATFORM_WINDOWS
    UE_LOG(LogMahjongNet, Log, TEXT("PC 开发环境启用模拟微信授权，不代表正式微信开放平台登录"));
    BeginLogin(EGuiyangLoginProvider::SimulatedWechat);
#else
    FailLogin(TEXT("当前安装包尚未配置正式微信开放平台授权"));
#endif
}

void UGuiyangLoginSubsystem::BeginLogin(const EGuiyangLoginProvider Provider, const FString& ExistingPlayerId, const FString& ExistingName)
{
    if (LoginState == EGuiyangLoginState::LoggingIn)
    {
        FailLogin(TEXT("登录请求正在处理中，请勿重复点击"));
        return;
    }
    if (Provider != EGuiyangLoginProvider::Guest && Provider != EGuiyangLoginProvider::SimulatedWechat)
    {
        FailLogin(TEXT("当前登录方式不可用"));
        return;
    }

    LoginState = EGuiyangLoginState::LoggingIn;
    OnLoginStateChanged.Broadcast(LoginState, CurrentProfile);
    UE_LOG(LogMahjongNet, Log, TEXT("开始处理登录请求：Provider=%s"), Provider == EGuiyangLoginProvider::Guest ? TEXT("游客") : TEXT("模拟微信"));

    const FString NewPlayerId = ExistingPlayerId.IsEmpty()
        ? FString::Printf(TEXT("%s-%s"), Provider == EGuiyangLoginProvider::Guest ? TEXT("guest") : TEXT("wxsim"), *FGuid::NewGuid().ToString(EGuidFormats::Digits))
        : ExistingPlayerId;
    const FString NewDisplayName = ExistingName.IsEmpty()
        ? FString::Printf(TEXT("%s%s"), Provider == EGuiyangLoginProvider::Guest ? TEXT("游客") : TEXT("微信玩家"), *MakePlayerSuffix(NewPlayerId))
        : ExistingName.Left(24);

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(PendingLoginTimer, FTimerDelegate::CreateUObject(this, &ThisClass::CompleteLogin, Provider, NewPlayerId, NewDisplayName), 0.15f, false);
    }
    else
    {
        CompleteLogin(Provider, NewPlayerId, NewDisplayName);
    }
}

void UGuiyangLoginSubsystem::CompleteLogin(const EGuiyangLoginProvider Provider, FString PlayerId, FString DisplayName)
{
    if (LoginState != EGuiyangLoginState::LoggingIn)
    {
        return;
    }
    CurrentProfile.PlayerId = MoveTemp(PlayerId);
    CurrentProfile.DisplayName = MoveTemp(DisplayName);
    CurrentProfile.Provider = Provider;
    SessionToken = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    SessionExpireAtUtc = FDateTime::UtcNow() + FTimespan::FromHours(12.0);
    LoginState = EGuiyangLoginState::LoggedIn;
    SaveAutoLoginPreference();
    UE_LOG(LogMahjongNet, Log, TEXT("登录成功：PlayerId=%s，昵称=%s，Provider=%s"), *CurrentProfile.PlayerId, *CurrentProfile.DisplayName, *CurrentProfile.GetProviderDisplayName());
    OnLoginStateChanged.Broadcast(LoginState, CurrentProfile);
}

void UGuiyangLoginSubsystem::FailLogin(const FString& ChineseReason)
{
    LoginState = EGuiyangLoginState::LoggedOut;
    UE_LOG(LogMahjongNet, Warning, TEXT("登录失败：%s"), *ChineseReason);
    OnLoginFailed.Broadcast(ChineseReason);
    OnLoginStateChanged.Broadcast(LoginState, CurrentProfile);
}

void UGuiyangLoginSubsystem::Logout()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PendingLoginTimer);
    }
    SessionToken.Reset();
    SessionExpireAtUtc = FDateTime();
    CurrentProfile = {};
    LoginState = EGuiyangLoginState::LoggedOut;
    if (LoginSettings)
    {
        LoginSettings->bAutoLogin = false;
        LoginSettings->SavedPlayerId.Reset();
        LoginSettings->SavedDisplayName.Reset();
        LoginSettings->SavedProvider = EGuiyangLoginProvider::None;
        UGameplayStatics::SaveGameToSlot(LoginSettings, SaveSlotName, 0);
    }
    UE_LOG(LogMahjongNet, Log, TEXT("玩家已安全退出登录，本地会话已清理"));
    OnLoginStateChanged.Broadcast(LoginState, CurrentProfile);
}

void UGuiyangLoginSubsystem::ExpireSession(const FString& ChineseReason)
{
    SessionToken.Reset();
    SessionExpireAtUtc = FDateTime();
    LoginState = EGuiyangLoginState::Expired;
    UE_LOG(LogMahjongNet, Warning, TEXT("会话过期：%s"), *ChineseReason);
    OnLoginFailed.Broadcast(ChineseReason);
    OnLoginStateChanged.Broadcast(LoginState, CurrentProfile);
}

bool UGuiyangLoginSubsystem::IsSessionValid() const
{
    return LoginState == EGuiyangLoginState::LoggedIn && CurrentProfile.IsValid() && !SessionToken.IsEmpty() && FDateTime::UtcNow() < SessionExpireAtUtc;
}

bool UGuiyangLoginSubsystem::LoginForIntegrationTest(const FString& PlayerId, const FString& DisplayName,
    const FString& InSessionToken)
{
#if UE_BUILD_SHIPPING
    return false;
#else
    if (!FParse::Param(FCommandLine::Get(), TEXT("MahjongEnableIntegrationHooks"))
        || PlayerId.IsEmpty() || DisplayName.IsEmpty() || InSessionToken.Len() < 16)
    {
        return false;
    }
    if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(PendingLoginTimer);
    CurrentProfile.PlayerId = PlayerId.Left(80);
    CurrentProfile.DisplayName = DisplayName.Left(24);
    CurrentProfile.Provider = EGuiyangLoginProvider::Guest;
    SessionToken = InSessionToken.Left(256);
    SessionExpireAtUtc = FDateTime::UtcNow() + FTimespan::FromHours(1.0);
    LoginState = EGuiyangLoginState::LoggedIn;
    UE_LOG(LogMahjongNet, Display, TEXT("MAHJONG_INTEGRATION_LOGIN_READY Player=%s"), *CurrentProfile.PlayerId);
    OnLoginStateChanged.Broadcast(LoginState, CurrentProfile);
    return true;
#endif
}

void UGuiyangLoginSubsystem::SaveAutoLoginPreference()
{
    if (!LoginSettings || !CurrentProfile.IsValid())
    {
        return;
    }
    LoginSettings->bAutoLogin = true;
    LoginSettings->SavedPlayerId = CurrentProfile.PlayerId;
    LoginSettings->SavedDisplayName = CurrentProfile.DisplayName;
    LoginSettings->SavedProvider = CurrentProfile.Provider;
    UGameplayStatics::SaveGameToSlot(LoginSettings, SaveSlotName, 0);
}

FString UGuiyangLoginSubsystem::MakePlayerSuffix(const FString& PlayerId)
{
    return PlayerId.Len() >= 4 ? PlayerId.Right(4).ToUpper() : TEXT("0000");
}
