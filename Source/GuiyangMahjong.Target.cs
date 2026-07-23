using UnrealBuildTool;

public class GuiyangMahjongTarget : TargetRules
{
    public GuiyangMahjongTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.AddRange(["GuiyangMahjong", "GuiyangMahjongOnline", "GuiyangMahjongClient"]);
        DisablePlugins.AddRange(["Agones", "Landmass", "Water", "Volumetrics"]);
    }
}
