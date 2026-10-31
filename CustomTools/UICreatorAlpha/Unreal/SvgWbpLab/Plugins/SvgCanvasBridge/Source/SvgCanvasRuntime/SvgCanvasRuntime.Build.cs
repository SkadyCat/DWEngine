using UnrealBuildTool;
public class SvgCanvasRuntime : ModuleRules
{
    public SvgCanvasRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UMG", "Slate", "SlateCore", "InputCore" });
        PrivateDependencyModuleNames.AddRange(new[] { "Json" });
    }
}
