#include "Auth/GuiyangLoginTypes.h"

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
