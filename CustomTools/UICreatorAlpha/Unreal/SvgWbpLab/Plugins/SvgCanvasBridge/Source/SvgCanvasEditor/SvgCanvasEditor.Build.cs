using UnrealBuildTool;
public class SvgCanvasEditor : ModuleRules
{
    public SvgCanvasEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateIncludePathModuleNames.Add("SlateRHIRenderer");
        DynamicallyLoadedModuleNames.Add("SlateRHIRenderer");
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UMG", "SvgCanvasRuntime" });
        PrivateDependencyModuleNames.AddRange(new[] { "Slate", "SlateCore", "InputCore", "UnrealEd", "AudioEditor", "UMGEditor", "Kismet", "KismetCompiler", "AssetRegistry", "Json", "ImageWrapper", "ImageCore", "RenderCore", "RHI", "Projects" });
    }
}
