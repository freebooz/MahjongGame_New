#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Auth/GuiyangLoginTypes.h"
#include "GuiyangLoginSaveGame.generated.h"

/**
 * 本地自动登录偏好。这里只保存匿名账号标识和 Provider，绝不持久化会话令牌或第三方访问令牌。
 */
UCLASS()
class GUIYANGMAHJONG_API UGuiyangLoginSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY() bool bAutoLogin = true;
    UPROPERTY() FString SavedPlayerId;
    UPROPERTY() FString SavedDisplayName;
    UPROPERTY() EGuiyangLoginProvider SavedProvider = EGuiyangLoginProvider::None;
};
