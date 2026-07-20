using UnrealBuildTool;

public class GuiyangMahjongCore : ModuleRules
{
    public GuiyangMahjongCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
    }
}
