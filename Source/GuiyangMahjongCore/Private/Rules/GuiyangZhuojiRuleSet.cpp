#include "Rules/GuiyangZhuojiRuleSet.h"

#include "Rules/MahjongHuChecker.h"

bool UGuiyangZhuojiRuleSet::CanHu(const FMahjongHand& Hand) const
{
    return UMahjongHuChecker::CanHu(Hand, Config.bEnableQiDui);
}
