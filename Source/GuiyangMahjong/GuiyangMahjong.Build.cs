using UnrealBuildTool;

public class GuiyangMahjong : ModuleRules
{
    public GuiyangMahjong(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
            "UMG", "Slate", "SlateCore", "Networking", "Sockets", "NetCore",
            "OnlineSubsystem", "Json", "JsonUtilities", "ApplicationCore", "HTTP"
        });

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "UMGEditor", "AssetRegistry", "Kismet" });
        }

        // 服务端密码房使用 OpenSSL PBKDF2-HMAC-SHA256；密码永不进入复制状态或日志。
        AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
    }
}
