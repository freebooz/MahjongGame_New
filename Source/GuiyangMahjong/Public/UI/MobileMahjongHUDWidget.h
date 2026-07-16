#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Network/MahjongNetworkTypes.h"
#include "MobileMahjongHUDWidget.generated.h"

class UHorizontalBox; class UOverlay; class UTextBlock; class UWrapBox;
class UMobileActionButtonPanel;
class UMobileErrorToastWidget;
class UMobileSettlementWidget;

/** 游戏主 HUD。公共数据来自 GameState，私有手牌和操作列表来自所属 PlayerController Client RPC。 */
UCLASS(Abstract, BlueprintType)
class GUIYANGMAHJONG_API UMobileMahjongHUDWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_RoomId;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_RemainingTileCount;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_CurrentPhase;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_CurrentTurnPlayer;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_Countdown;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UHorizontalBox> Panel_SelfHandTiles;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UWrapBox> Panel_SelfDiscards;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UWrapBox> Panel_TopDiscards;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UWrapBox> Panel_LeftDiscards;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UWrapBox> Panel_RightDiscards;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Seat_Top;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Seat_Left;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Seat_Right;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Seat_Self;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UMobileActionButtonPanel> ActionButtonPanel;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UOverlay> PopupLayer;
    UPROPERTY(Transient) TObjectPtr<UMobileErrorToastWidget> ErrorToastInstance;
    UPROPERTY(Transient) TObjectPtr<UMobileSettlementWidget> SettlementInstance;
    UFUNCTION() void HandlePublicTableState(const FMahjongPublicTableState& State);
    UFUNCTION() void HandlePrivateHand(const FMahjongPrivatePlayerState& State);
    UFUNCTION() void HandleAvailableActions(const TArray<FMahjongAction>& Actions);
    UFUNCTION() void HandleSettlement(const FMahjongSettlementResult& Result);
    UFUNCTION() void HandleError(const FString& Message);
public:
    UFUNCTION(BlueprintCallable, Category="麻将|UI") void RefreshTableState(const FMahjongPublicTableState& State);
    UFUNCTION(BlueprintCallable, Category="麻将|UI") void RefreshPrivateHand(const FMahjongPrivatePlayerState& State);
};
