using UnrealBuildTool;

// Engine-object-free rule runtime. Depends on Core only so it can be unit tested
// and reasoned about without UObjects, Mass or rendering.
public class DWRulesCore : ModuleRules
{
	public DWRulesCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[] { "Core" });
	}
}
