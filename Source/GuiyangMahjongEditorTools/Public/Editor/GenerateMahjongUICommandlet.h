#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GenerateMahjongUICommandlet.generated.h"

/** 仅编辑器使用：从 C++ 规范生成组件化 P0 Widget Blueprint 资产。 */
UCLASS()
class GUIYANGMAHJONGEDITORTOOLS_API UGenerateMahjongUICommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UGenerateMahjongUICommandlet();
    virtual int32 Main(const FString& Params) override;
};
