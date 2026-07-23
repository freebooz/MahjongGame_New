using UnrealBuildTool;

public class GuiyangMahjongClient : ModuleRules
{
    public GuiyangMahjongClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "Engine", "GuiyangMahjongCore", "GuiyangMahjongOnline", "GuiyangMahjong"
        });
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "InputCore", "EnhancedInput", "UMG", "Slate", "SlateCore", "ApplicationCore",
            "HTTP", "Json", "JsonUtilities", "CinematicCamera"
        });
    }
}
