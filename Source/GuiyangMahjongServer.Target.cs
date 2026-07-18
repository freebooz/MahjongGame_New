using UnrealBuildTool;

public class GuiyangMahjongServerTarget : TargetRules
{
    public GuiyangMahjongServerTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Server;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("GuiyangMahjong");
        DisablePlugins.AddRange(["Landmass", "Water", "Volumetrics"]);
    }
}
