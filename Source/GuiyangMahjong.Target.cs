using UnrealBuildTool;

public class GuiyangMahjongTarget : TargetRules
{
    public GuiyangMahjongTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("GuiyangMahjong");
    }
}
