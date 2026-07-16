#pragma once

#include "CoreMinimal.h"
#include "Network/MahjongNetworkTypes.h"
#include "UObject/Object.h"
#include "GuiyangRoomManager.generated.h"

/** Dedicated Server 持有的房间领域服务；客户端不得创建该对象或直接改写房间记录。 */
UCLASS()
class GUIYANGMAHJONG_API UGuiyangRoomManager : public UObject
{
    GENERATED_BODY()

public:
    bool CreateRoom(const FString& PlayerId, const FString& DisplayName, const FMahjongCreateRoomRequest& Request,
        FMahjongRoomState& OutState, EMahjongRoomError& OutError);
    bool JoinRoom(const FString& PlayerId, const FString& DisplayName, const FMahjongJoinRoomRequest& Request,
        FMahjongRoomState& OutState, EMahjongRoomError& OutError);
    bool ToggleReady(const FString& PlayerId, FMahjongRoomState& OutState, EMahjongRoomError& OutError);
    bool LeaveRoom(const FString& PlayerId, FMahjongRoomState& OutState, EMahjongRoomError& OutError);
    bool BeginPlaying(const FString& RoomCode, FMahjongRoomState& OutState, EMahjongRoomError& OutError);
    bool GetRoomState(const FString& RoomCode, FMahjongRoomState& OutState) const;
    bool GetPlayerRoomCode(const FString& PlayerId, FString& OutRoomCode) const;
    int32 GetRoomCount() const { return Rooms.Num(); }

private:
    struct FPasswordAttemptState
    {
        int32 FailureCount = 0;
        FDateTime LockedUntilUtc;
    };

    struct FRoomRecord
    {
        FMahjongRoomState PublicState;
        FString PasswordSalt;
        FString PasswordDigest;
        TMap<FString, FPasswordAttemptState> PasswordAttemptsByPlayer;
    };

    TMap<FString, FRoomRecord> Rooms;
    TMap<FString, FString> PlayerRoomCodes;
    FRandomStream RoomCodeRandom;
    bool bRandomInitialized = false;

    static constexpr int32 MaxPasswordFailures = 5;
    static constexpr int32 PasswordLockSeconds = 30;
    static constexpr int32 PasswordHashRounds = 100000;

    FString GenerateUniqueRoomCode();
    static bool ValidateIdentity(const FString& PlayerId, const FString& DisplayName);
    static bool ValidatePassword(const FString& Password);
    static FString MakePasswordSalt();
    static FString HashPassword(const FString& Password, const FString& Salt);
    static bool ConstantTimeEquals(const FString& Left, const FString& Right);
    static FMahjongSeatInfo* FindSeat(FMahjongRoomState& State, const FString& PlayerId);
    static const FMahjongSeatInfo* FindSeat(const FMahjongRoomState& State, const FString& PlayerId);
    static void RefreshLifecycle(FMahjongRoomState& State);
};
