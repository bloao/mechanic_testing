using UnrealBuildTool;
using System.Collections.Generic;

public class mechanic_testingTarget : TargetRules
{
	public mechanic_testingTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V2;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_3;
		ExtraModuleNames.Add("mechanic_testing");
	}
}
