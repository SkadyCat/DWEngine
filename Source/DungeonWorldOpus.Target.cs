using UnrealBuildTool;
public class DungeonWorldOpusTarget : TargetRules
{
 public DungeonWorldOpusTarget(TargetInfo Target) : base(Target)
 {
  Type=TargetType.Game;
  DefaultBuildSettings=BuildSettingsVersion.V7;
  IncludeOrderVersion=EngineIncludeOrderVersion.Unreal5_8;
  ExtraModuleNames.Add("DungeonWorldOpus");
 }
}
