using UnrealBuildTool;

public class mechanic_testing : ModuleRules
{
	public mechanic_testing(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
				"EnhancedInput",
			"AIModule",
			"NavigationSystem"
		});
	}
}
