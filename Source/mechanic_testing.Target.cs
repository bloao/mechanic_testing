using UnrealBuildTool;
using System.Collections.Generic;

public class mechanic_testingTarget : TargetRules
{
	public mechanic_testingTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
		WindowsPlatform.bWriteSarif = false;
		ExtraModuleNames.Add("mechanic_testing");
	}
}
