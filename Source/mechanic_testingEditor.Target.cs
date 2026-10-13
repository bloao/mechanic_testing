using UnrealBuildTool;
using System.Collections.Generic;

public class mechanic_testingEditorTarget : TargetRules
{
	public mechanic_testingEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V2;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_3;
		ExtraModuleNames.Add("mechanic_testing");
	}
}
