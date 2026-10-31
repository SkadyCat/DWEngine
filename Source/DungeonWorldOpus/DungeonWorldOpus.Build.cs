using UnrealBuildTool;
public class DungeonWorldOpus : ModuleRules
{
 public DungeonWorldOpus(ReadOnlyTargetRules Target) : base(Target)
 {
  PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;
  bUseUnity=false; // UI adapters have file-local helpers; keep their translation units isolated.
  PublicDependencyModuleNames.AddRange(new[]{"Core","CoreUObject","Engine","DWRules","DWRulesCore","UMG","SvgCanvasRuntime","FCS3C","ClaudePCG","NavigationSystem"});
  PrivateDependencyModuleNames.AddRange(new[]{"Json","JsonUtilities","UnLua","SlateCore","Slate","HTTP","InputCore"});
  if(Target.bBuildEditor)PrivateDependencyModuleNames.AddRange(new[]{"UMGEditor","UnrealEd","DesktopPlatform"});
  RuntimeDependencies.Add("$(ProjectDir)/Content/UICreatorTalents/contract.json", StagedFileType.NonUFS);
  RuntimeDependencies.Add("$(ProjectDir)/Content/UICreatorEffects/contract.json", StagedFileType.NonUFS);
  RuntimeDependencies.Add("$(ProjectDir)/Content/UICreatorRecent/contract.json", StagedFileType.NonUFS);
  RuntimeDependencies.Add("$(ProjectDir)/Content/UICreatorMaps/contract.json", StagedFileType.NonUFS);
  RuntimeDependencies.Add("$(ProjectDir)/Content/UICreatorEvents/contract.json", StagedFileType.NonUFS);
        RuntimeDependencies.Add("$(ProjectDir)/Content/Rules/Talents/constellation.json", StagedFileType.NonUFS);
        RuntimeDependencies.Add("$(ProjectDir)/Content/UICreatorLoot/contract.json", StagedFileType.NonUFS);
 }
}

