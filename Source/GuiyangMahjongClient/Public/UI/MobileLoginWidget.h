#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Auth/GuiyangLoginTypes.h"
#include "MobileLoginWidget.generated.h"

class UButton;
class UCheckBox;
class UCircularThrobber;
class UImage;
class UTextBlock;

/** 登录页面。只调用登录子系统，不直接创建账号、Session 或修改网络权威状态。 */
UCLASS(Abstract, BlueprintType)
class GUIYANGMAHJONGCLIENT_API UMobileLoginWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(meta=(BindWidget)) TObjectPtr<UImage> Img_Background;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UImage> Img_GameLogo;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Btn_WechatLogin;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Btn_GuestLogin;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UCheckBox> Chk_AgreeTerms;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Btn_UserAgreement;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Btn_PrivacyPolicy;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_LoginStatus;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_Version;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UCircularThrobber> Loading_Login;

    UFUNCTION() void HandleGuestLogin();
    UFUNCTION() void HandleWechatLogin();
    UFUNCTION() void HandleUserAgreement();
    UFUNCTION() void HandlePrivacyPolicy();
    UFUNCTION() void HandleLoginStateChanged(EGuiyangLoginState State, const FGuiyangLoginProfile& Profile);
    UFUNCTION() void HandleLoginFailed(const FString& ChineseReason);

public:
    UFUNCTION(BlueprintCallable, Category="麻将|UI") void SetLoginStatus(const FString& ChineseStatus, bool bLoading);

private:
    bool ValidateAgreement();
};
