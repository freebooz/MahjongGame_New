#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Network/MahjongNetworkTypes.h"
#include "MobileRoomWidget.generated.h"

class UButton; class UTextBlock;

/** 房间页 C++ 基类。房间显示数据来自 GameState 的 OnRep_RoomState。 */
UCLASS(Abstract, BlueprintType)
class GUIYANGMAHJONG_API UMobileRoomWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual void NativeConstruct() override;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_RoomId;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_RuleSummary;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Seat_Top;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Seat_Left;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Seat_Right;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Seat_Bottom;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Btn_Ready;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Btn_LeaveRoom;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Txt_StartTip;
    UFUNCTION() void HandleReady();
    UFUNCTION() void HandleLeave();
public:
    UFUNCTION(BlueprintCallable, Category="麻将|UI") void RefreshRoomState(const FMahjongRoomState& State, int32 LocalSeat);
};
