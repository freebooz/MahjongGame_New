#include "History/GuiyangMatchHistorySubsystem.h"

#include "History/MahjongMatchHistorySaveGame.h"
#include "Kismet/GameplayStatics.h"

void UGuiyangMatchHistorySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
        HistorySave = Cast<UMahjongMatchHistorySaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));
    if (!HistorySave)
        HistorySave = Cast<UMahjongMatchHistorySaveGame>(
            UGameplayStatics::CreateSaveGameObject(UMahjongMatchHistorySaveGame::StaticClass()));
}

void UGuiyangMatchHistorySubsystem::Deinitialize()
{
    HistorySave = nullptr;
    Super::Deinitialize();
}

bool UGuiyangMatchHistorySubsystem::RecordFinalSettlement(const FMahjongFinalSettlementResult& Result)
{
    if (!HistorySave || Result.MatchId.IsEmpty() || Result.Players.IsEmpty()) return false;
    if (HistorySave->Records.ContainsByPredicate([&Result](const FMahjongMatchHistoryRecord& Record)
    {
        return Record.MatchId == Result.MatchId;
    })) return false;

    const TArray<FMahjongMatchHistoryRecord> PreviousRecords = HistorySave->Records;
    FMahjongMatchHistoryRecord Record;
    Record.MatchId = Result.MatchId;
    Record.RecordedAtUtc = FDateTime::UtcNow().ToIso8601();
    Record.FinalResult = Result;
    HistorySave->Records.Insert(MoveTemp(Record), 0);
    if (HistorySave->Records.Num() > MaxHistoryRecords)
        HistorySave->Records.SetNum(MaxHistoryRecords);
    if (!SaveHistory())
    {
        HistorySave->Records = PreviousRecords;
        return false;
    }
    OnHistoryChanged.Broadcast();
    return true;
}

TArray<FMahjongMatchHistoryRecord> UGuiyangMatchHistorySubsystem::GetRecords() const
{
    return HistorySave ? HistorySave->Records : TArray<FMahjongMatchHistoryRecord>();
}

void UGuiyangMatchHistorySubsystem::ClearHistory()
{
    if (!HistorySave) return;
    HistorySave->Records.Reset();
    SaveHistory();
    OnHistoryChanged.Broadcast();
}

bool UGuiyangMatchHistorySubsystem::SaveHistory() const
{
    return HistorySave && UGameplayStatics::SaveGameToSlot(HistorySave, SaveSlotName, 0);
}
