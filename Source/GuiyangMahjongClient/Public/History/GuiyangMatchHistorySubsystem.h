#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "History/MahjongMatchHistoryTypes.h"
#include "GuiyangMatchHistorySubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMahjongMatchHistoryChanged);

UCLASS()
class GUIYANGMAHJONGCLIENT_API UGuiyangMatchHistorySubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable, Category="麻将|战绩") FMahjongMatchHistoryChanged OnHistoryChanged;

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category="麻将|战绩")
    bool RecordFinalSettlement(const FMahjongFinalSettlementResult& Result);

    UFUNCTION(BlueprintPure, Category="麻将|战绩")
    TArray<FMahjongMatchHistoryRecord> GetRecords() const;

    UFUNCTION(BlueprintCallable, Category="麻将|战绩")
    void ClearHistory();

private:
    static constexpr const TCHAR* SaveSlotName = TEXT("GuiyangMatchHistory");
    static constexpr int32 MaxHistoryRecords = 50;
    UPROPERTY() TObjectPtr<class UMahjongMatchHistorySaveGame> HistorySave;
    bool SaveHistory() const;
};
