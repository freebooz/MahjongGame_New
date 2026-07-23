using UnrealBuildTool;

public class GuiyangMahjongEditorTarget : TargetRules
{
    public GuiyangMahjongEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.AddRange([
            "GuiyangMahjong", "GuiyangMahjongOnline", "GuiyangMahjongClient",
            "GuiyangMahjongServer", "GuiyangMahjongEditorTools"
        ]);
        DisablePlugins.AddRange(["Landmass", "Water", "Volumetrics"]);
    }
}
