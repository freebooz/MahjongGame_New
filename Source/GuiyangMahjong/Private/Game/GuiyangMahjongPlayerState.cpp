#include "Game/GuiyangMahjongPlayerState.h"

#include "GuiyangMahjong.h"
#include "Net/UnrealNetwork.h"

AGuiyangMahjongPlayerState::AGuiyangMahjongPlayerState()
{
    bReplicates = true;
}

bool AGuiyangMahjongPlayerState::AuthenticateServer(const FString& InMahjongPlayerId, const FString& PlayerDisplayName, const EGuiyangLoginProvider Provider)
{
    const FString CleanId = InMahjongPlayerId.TrimStartAndEnd();
    const FString CleanName = PlayerDisplayName.TrimStartAndEnd();
    const bool bProviderAllowed = Provider == EGuiyangLoginProvider::Guest || Provider == EGuiyangLoginProvider::SimulatedWechat;
    if (!HasAuthority() || CleanId.IsEmpty() || CleanId.Len() > 80 || CleanName.IsEmpty() || CleanName.Len() > 24 || !bProviderAllowed)
    {
        UE_LOG(LogMahjongNet, Warning, TEXT("服务端拒绝连接认证：账号资料格式或 Provider 不合法"));
        return false;
    }
    MahjongPlayerId = CleanId;
    DisplayName = CleanName;
    LoginProvider = Provider;
    ServerSessionId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    ServerSessionExpireAtUtc = FDateTime::UtcNow() + FTimespan::FromHours(12.0);
    bOnline = true;
    SetPlayerName(DisplayName);
    ForceNetUpdate();
    UE_LOG(LogMahjongServer, Log, TEXT("服务端会话建立成功：PlayerId=%s，昵称=%s，Provider=%s"), *MahjongPlayerId, *DisplayName, Provider == EGuiyangLoginProvider::Guest ? TEXT("游客") : TEXT("模拟微信"));
    return true;
}

bool AGuiyangMahjongPlayerState::HasValidServerSession() const
{
    return HasAuthority() && !ServerSessionId.IsEmpty() && !MahjongPlayerId.IsEmpty() && FDateTime::UtcNow() < ServerSessionExpireAtUtc;
}

void AGuiyangMahjongPlayerState::ClearServerSession()
{
    if (!HasAuthority()) return;
    ServerSessionId.Reset();
    ServerSessionExpireAtUtc = FDateTime();
    MahjongPlayerId.Reset();
    DisplayName.Reset();
    LoginProvider = EGuiyangLoginProvider::None;
    LeaveRoomServer();
    ForceNetUpdate();
}

void AGuiyangMahjongPlayerState::EnterRoomServer(const FString& InRoomCode, const int32 InSeatIndex, const bool bIsReady)
{
    if (!HasAuthority()) return;
    RoomCode = InRoomCode;
    SeatIndex = InSeatIndex;
    bReady = bIsReady;
    ForceNetUpdate();
}

void AGuiyangMahjongPlayerState::LeaveRoomServer()
{
    if (!HasAuthority()) return;
    RoomCode.Reset();
    SeatIndex = INDEX_NONE;
    bReady = false;
    ForceNetUpdate();
}

void AGuiyangMahjongPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AGuiyangMahjongPlayerState, MahjongPlayerId);
    DOREPLIFETIME(AGuiyangMahjongPlayerState, DisplayName);
    DOREPLIFETIME(AGuiyangMahjongPlayerState, LoginProvider);
    DOREPLIFETIME(AGuiyangMahjongPlayerState, RoomCode);
    DOREPLIFETIME(AGuiyangMahjongPlayerState, SeatIndex);
    DOREPLIFETIME(AGuiyangMahjongPlayerState, bReady);
    DOREPLIFETIME(AGuiyangMahjongPlayerState, bOnline);
}
