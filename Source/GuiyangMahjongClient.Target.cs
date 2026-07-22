using UnrealBuildTool;

public class GuiyangMahjongClientTarget : TargetRules
{
    public GuiyangMahjongClientTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Client;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("GuiyangMahjong");
        DisablePlugins.AddRange(["Landmass", "Water", "Volumetrics"]);
    }
}
