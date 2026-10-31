using UnrealBuildTool;

public class FCS3CAnimGraph : ModuleRules
{
    public FCS3CAnimGraph(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicIncludePaths.Add(ModuleDirectory);
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "AnimGraph", "BlueprintGraph", "FCS3C" });
    }
}
