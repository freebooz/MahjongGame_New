#pragma once

#include "CoreMinimal.h"
#include "Core/MahjongTypes.h"
#include "UObject/Object.h"
#include "MahjongDeckManager.generated.h"

/** 服务端牌墙管理器。负责创建、洗牌、摸牌和初始发牌；客户端不得实例化它决定牌序。 */
UCLASS(BlueprintType)
class GUIYANGMAHJONGCORE_API UMahjongDeckManager : public UObject
{
    GENERATED_BODY()
public:
    /** 按规则配置重建牌墙。默认配置为贵阳三门数牌 108 张。执行端：服务端。 */
    UFUNCTION(BlueprintCallable, Category="麻将|牌墙") void InitializeDeck(const FMahjongRuleConfig& RuleConfig);
    /** 重建标准 136 张牌墙并把摸牌位置归零。执行端：服务端。 */
    UFUNCTION(BlueprintCallable, Category="麻将|牌墙") void InitializeStandardDeck();
    /** 使用服务端种子进行 Fisher-Yates 洗牌。测试可传固定种子以复现牌局。 */
    UFUNCTION(BlueprintCallable, Category="麻将|牌墙") void ShuffleDeck(int32 Seed);
    /** 从牌墙头部摸一张牌；牌墙为空时返回 false，且不修改输出。 */
    UFUNCTION(BlueprintCallable, Category="麻将|牌墙") bool DrawTile(FMahjongTile& OutTile);

    bool DealInitialHands(TArray<FMahjongHand>& OutHands, int32 DealerSeat);
    int32 GetRemainingCount() const { return Deck.Num() - DrawIndex; }
    const TArray<FMahjongTile>& GetDeckForServerTest() const { return Deck; }

private:
    UPROPERTY() TArray<FMahjongTile> Deck;
    UPROPERTY() int32 DrawIndex = 0;
};
