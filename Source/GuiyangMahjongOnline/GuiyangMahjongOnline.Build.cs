using UnrealBuildTool;

public class GuiyangMahjongOnline : ModuleRules
{
    public GuiyangMahjongOnline(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "Engine", "GuiyangMahjongCore", "HTTP", "Json", "JsonUtilities"
        });
    }
}
