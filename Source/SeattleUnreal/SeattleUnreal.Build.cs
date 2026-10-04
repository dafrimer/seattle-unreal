using UnrealBuildTool;
public class SeattleUnreal : ModuleRules
{
    public SeattleUnreal(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Json", "JsonUtilities", "RenderCore", "RHI", "ImageWrapper", "Slate", "SlateCore" });
    }
}
