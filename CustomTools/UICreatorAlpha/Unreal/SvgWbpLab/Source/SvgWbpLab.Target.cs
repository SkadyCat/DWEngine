using UnrealBuildTool;
public class SvgWbpLabTarget : TargetRules
{
    public SvgWbpLabTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("SvgWbpLab");
    }
}
