using UnrealBuildTool;
using System.Collections.Generic;

public class mechanic_testingEditorTarget : TargetRules
{
	public mechanic_testingEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
		WindowsPlatform.bWriteSarif = false;
		ExtraModuleNames.Add("mechanic_testing");
	}
}
