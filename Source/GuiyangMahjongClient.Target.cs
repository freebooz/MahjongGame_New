using UnrealBuildTool;

public class GuiyangMahjongClientTarget : TargetRules
{
    public GuiyangMahjongClientTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Client;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.AddRange(["GuiyangMahjong", "GuiyangMahjongOnline", "GuiyangMahjongClient"]);
        DisablePlugins.AddRange(["Agones", "Landmass", "Water", "Volumetrics", "NNERuntimeORT", "NNEDenoiser"]);
    }
}
