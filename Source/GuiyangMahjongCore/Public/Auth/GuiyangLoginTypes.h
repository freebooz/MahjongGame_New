#pragma once

#include "CoreMinimal.h"
#include "GuiyangLoginTypes.generated.h"

/** 当前客户端采用的登录方式。正式微信登录与 PC 模拟授权必须明确区分。 */
UENUM(BlueprintType)
enum class EGuiyangLoginProvider : uint8
{
    None UMETA(DisplayName="未登录"),
    Guest UMETA(DisplayName="游客"),
    SimulatedWechat UMETA(DisplayName="模拟微信"),
    Wechat UMETA(DisplayName="微信")
};

/** 登录状态机，UI 通过事件响应状态变化，不在 Tick 中轮询。 */
UENUM(BlueprintType)
enum class EGuiyangLoginState : uint8
{
    LoggedOut UMETA(DisplayName="未登录"),
    LoggingIn UMETA(DisplayName="登录中"),
    LoggedIn UMETA(DisplayName="已登录"),
    Expired UMETA(DisplayName="会话已过期")
};

/** 可以安全展示给本地 UI 的账号资料，不包含 SessionToken。 */
USTRUCT(BlueprintType)
struct GUIYANGMAHJONGCORE_API FGuiyangLoginProfile
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="登录") FString PlayerId;
    UPROPERTY(BlueprintReadOnly, Category="登录") FString DisplayName;
    UPROPERTY(BlueprintReadOnly, Category="登录") EGuiyangLoginProvider Provider = EGuiyangLoginProvider::None;

    bool IsValid() const
    {
        return !PlayerId.IsEmpty() && !DisplayName.IsEmpty() && Provider != EGuiyangLoginProvider::None;
    }

    FString GetProviderDisplayName() const;
};
