using UnrealBuildTool;

public class ClaudePCG : ModuleRules
{
	public ClaudePCG(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"PhysicsCore",
			"NavigationSystem",
			"PCG"
		});
		if (Target.bBuildEditor) PrivateDependencyModuleNames.AddRange(new[] {"UnrealEd", "BSPUtils"});
	}
}
