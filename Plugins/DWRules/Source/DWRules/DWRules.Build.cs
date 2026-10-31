using UnrealBuildTool;

// Engine integration of DWRulesCore: assets, world subsystem, Mass target export/frame driver,
// instanced rendering and the UnLua/Blueprint entry points.
public class DWRules : ModuleRules
{
	public DWRules(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DWRulesCore",
			"MassCore",
			"MassEntity",
			"MassCommon",
			"MassSpawner"
		});
		PrivateDependencyModuleNames.AddRange(new[] { "Json", "Niagara" });
	}
}
