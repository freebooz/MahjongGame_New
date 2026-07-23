using UnrealBuildTool;

public class GuiyangMahjong : ModuleRules
{
    public GuiyangMahjong(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "Engine", "GuiyangMahjongCore", "Networking", "Sockets", "NetCore"
        });
    }
}
