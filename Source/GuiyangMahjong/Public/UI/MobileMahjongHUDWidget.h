#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Network/MahjongNetworkTypes.h"
#include "MobileMahjongHUDWidget.generated.h"

class UHorizontalBox; class UOverlay; class UTextBlock; class UVerticalBox; class UViewport; class UWrapBox;
class AMahjong3DTableActor;
class UMobileActionButtonPanel;
class UMobileHandTileWidget;
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
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_FlippedJiTile;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_JiEvents;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UViewport> Table3DViewport;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UHorizontalBox> Panel_SelfHandTiles;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UHorizontalBox> Panel_TopHandTiles;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UVerticalBox> Panel_LeftHandTiles;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UVerticalBox> Panel_RightHandTiles;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UWrapBox> Panel_SelfDiscards;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UWrapBox> Panel_TopDiscards;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UWrapBox> Panel_LeftDiscards;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UWrapBox> Panel_RightDiscards;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UVerticalBox> Panel_SelfMelds;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UVerticalBox> Panel_TopMelds;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UVerticalBox> Panel_LeftMelds;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UVerticalBox> Panel_RightMelds;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Seat_Top;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Seat_Left;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Seat_Right;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Seat_Self;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UMobileActionButtonPanel> ActionButtonPanel;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UOverlay> PopupLayer;
    UPROPERTY(Transient) TObjectPtr<UMobileErrorToastWidget> ErrorToastInstance;
    UPROPERTY(Transient) TObjectPtr<UMobileSettlementWidget> SettlementInstance;
    UPROPERTY(Transient) TObjectPtr<UMobileHandTileWidget> SelectedHandTile;
    UPROPERTY(Transient) TObjectPtr<AMahjong3DTableActor> Table3DActor;
    UPROPERTY() FMahjongPublicTableState CachedPublicState;
    UPROPERTY() FMahjongPrivatePlayerState CachedPrivateState;
    bool bHasPrivateState = false;
    bool bVisualReviewMode = false;
    UFUNCTION() void HandlePublicTableState(const FMahjongPublicTableState& State);
    UFUNCTION() void HandlePrivateHand(const FMahjongPrivatePlayerState& State);
    UFUNCTION() void HandleAvailableActions(const TArray<FMahjongAction>& Actions);
    UFUNCTION() void HandleSettlement(const FMahjongSettlementResult& Result);
    UFUNCTION() void HandleFinalSettlement(const FMahjongFinalSettlementResult& Result);
    UFUNCTION() void HandleError(const FString& Message);
    UFUNCTION() void HandleTileSelected(UMobileHandTileWidget* TileWidget);
public:
    UFUNCTION(BlueprintCallable, Category="麻将|UI") void RefreshTableState(const FMahjongPublicTableState& State);
    UFUNCTION(BlueprintCallable, Category="麻将|UI") void RefreshPrivateHand(const FMahjongPrivatePlayerState& State);
    /** 注入只读的本地截图预览数据，不发送任何牌局请求。 */
    void ApplyVisualReviewState(const FMahjongPublicTableState& PublicState,
        const FMahjongPrivatePlayerState& PrivateState, const TArray<FMahjongAction>& Actions);
    static int32 GetRelativeSeatIndex(int32 AbsoluteSeat, int32 LocalSeat);
    static FString GetPhaseDisplayText(EMahjongTablePhase Phase);

private:
    int32 ResolveLocalSeat() const;
    void RebuildPrivateHand();
    void RefreshOpponentHands(int32 LocalSeat);
    void RefreshDiscards(int32 LocalSeat);
    void RefreshMelds(int32 LocalSeat);
    void RefreshJiDisplay();
    void Refresh3DTable();
};
