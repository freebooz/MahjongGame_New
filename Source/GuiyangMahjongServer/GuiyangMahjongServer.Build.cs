using UnrealBuildTool;

public class GuiyangMahjongServer : ModuleRules
{
    public GuiyangMahjongServer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "Engine", "GuiyangMahjongCore", "GuiyangMahjong",
            "Networking", "Sockets", "NetCore", "HTTP", "Json", "JsonUtilities", "Agones"
        });
        AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
    }
}
