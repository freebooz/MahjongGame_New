using UnrealBuildTool;

public class GuiyangMahjongEditorTools : ModuleRules
{
    public GuiyangMahjongEditorTools(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "GuiyangMahjong" });
        PrivateDependencyModuleNames.AddRange(new[] { "UMG", "Slate", "SlateCore", "UnrealEd", "UMGEditor", "AssetRegistry", "Kismet" });
    }
}
