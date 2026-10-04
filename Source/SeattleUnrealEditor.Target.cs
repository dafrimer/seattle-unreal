using UnrealBuildTool;
public class SeattleUnrealEditorTarget : TargetRules
{
    public SeattleUnrealEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("SeattleUnreal");
    }
}
