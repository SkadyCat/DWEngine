using UnrealBuildTool;
public class SvgWbpLab : ModuleRules
{
    public SvgWbpLab(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UMG", "SvgCanvasRuntime" });
        PrivateDependencyModuleNames.AddRange(new[] { "Json", "SlateCore", "RenderCore" });
    }
}
