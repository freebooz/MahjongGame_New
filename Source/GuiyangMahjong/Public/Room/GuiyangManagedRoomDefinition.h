#pragma once

#include "CoreMinimal.h"
#include "Rules/GuiyangRuleSnapshot.h"

class FJsonObject;

/** Lobby 在 GameServer 注册成功时下发的单房间权威启动定义。 */
struct GUIYANGMAHJONG_API FGuiyangManagedRoomDefinition
{
    FString BackendRoomId;
    FString RoomCode;
    FString MatchId;
    FString OwnerPlayerId;
    int32 RoundCount = 0;
    int32 MaximumPlayers = 0;
    bool bPublicRoom = false;
    bool bAutoStart = false;
    bool bPasswordProtected = false;
    FGuiyangRuleSnapshot RuleSnapshot;

    static bool TryParse(const TSharedPtr<FJsonObject>& Bootstrap,
        const FString& ExpectedRoomId, const FString& ExpectedMatchId,
        FGuiyangManagedRoomDefinition& OutDefinition, FString& OutError);
};
