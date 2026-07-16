#include "Rules/GuiyangRuleSnapshot.h"

#include "Misc/SecureHash.h"

int32 FGuiyangRuleSnapshot::GetTileCount() const
{
    return Config.TileSetMode == EMahjongTileSetMode::Standard136 ? 136 : 108;
}

FGuiyangRuleSnapshot UGuiyangRuleSnapshotLibrary::CreateSnapshot(const FMahjongRuleConfig& RequestedConfig)
{
    FGuiyangRuleSnapshot Snapshot;
    Snapshot.Config = NormalizeConfig(RequestedConfig);
    Snapshot.CanonicalDefinition = BuildCanonicalDefinition(Snapshot.Config);
    Snapshot.RuleHash = CalculateHash(Snapshot.CanonicalDefinition);
    return Snapshot;
}

bool UGuiyangRuleSnapshotLibrary::VerifySnapshot(const FGuiyangRuleSnapshot& Snapshot)
{
    if (Snapshot.RuleHash.IsEmpty() || Snapshot.CanonicalDefinition.IsEmpty())
    {
        return false;
    }

    const FMahjongRuleConfig Normalized = NormalizeConfig(Snapshot.Config);
    const FString ExpectedDefinition = BuildCanonicalDefinition(Normalized);
    return Snapshot.CanonicalDefinition == ExpectedDefinition
        && Snapshot.RuleHash == CalculateHash(ExpectedDefinition);
}

FMahjongRuleConfig UGuiyangRuleSnapshotLibrary::NormalizeConfig(const FMahjongRuleConfig& RequestedConfig)
{
    FMahjongRuleConfig Result = RequestedConfig;
    if (Result.RuleId.IsNone())
    {
        Result.RuleId = TEXT("GuiyangMainstreamV1");
    }
    Result.RuleVersion = FMath::Max(1, Result.RuleVersion);
    Result.BaseScore = FMath::Clamp(Result.BaseScore, 1, 100);
    Result.JiScore = FMath::Clamp(Result.JiScore, 0, 100);
    Result.GangScore = FMath::Clamp(Result.GangScore, 0, 100);
    Result.ZiMoMultiplier = FMath::Clamp(Result.ZiMoMultiplier, 1, 16);
    Result.DianPaoMultiplier = FMath::Clamp(Result.DianPaoMultiplier, 1, 16);
    Result.ReconnectTimeoutSeconds = FMath::Clamp(Result.ReconnectTimeoutSeconds, 15, 600);
    return Result;
}

FString UGuiyangRuleSnapshotLibrary::BuildCanonicalDefinition(const FMahjongRuleConfig& Config)
{
    // 固定字段顺序可确保不同服务端进程生成相同哈希；新增字段时应提升规则版本。
    return FString::Printf(
        TEXT("RuleId=%s|RuleVersion=%d|TileSet=%d|ChongFengJi=%d|ZeRenJi=%d|WuGuJi=%d|")
        TEXT("QiangGangHu=%d|YiPaoDuoXiang=%d|QiDui=%d|BaseScore=%d|JiScore=%d|GangScore=%d|")
        TEXT("ZiMoMultiplier=%d|DianPaoMultiplier=%d|ReconnectTimeoutSeconds=%d"),
        *Config.RuleId.ToString(), Config.RuleVersion, static_cast<int32>(Config.TileSetMode),
        Config.bEnableChongFengJi ? 1 : 0, Config.bEnableZeRenJi ? 1 : 0,
        Config.bEnableWuGuJi ? 1 : 0, Config.bEnableQiangGangHu ? 1 : 0,
        Config.bEnableYiPaoDuoXiang ? 1 : 0, Config.bEnableQiDui ? 1 : 0,
        Config.BaseScore, Config.JiScore, Config.GangScore, Config.ZiMoMultiplier,
        Config.DianPaoMultiplier, Config.ReconnectTimeoutSeconds);
}

FString UGuiyangRuleSnapshotLibrary::CalculateHash(const FString& CanonicalDefinition)
{
    FTCHARToUTF8 Utf8(*CanonicalDefinition);
    uint8 Digest[FSHA1::DigestSize];
    FSHA1::HashBuffer(Utf8.Get(), Utf8.Length(), Digest);
    return BytesToHex(Digest, UE_ARRAY_COUNT(Digest)).ToLower();
}
