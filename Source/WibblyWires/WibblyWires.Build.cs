// Copyright 2022 Geordie Hall. All rights reserved.

using UnrealBuildTool;

public class WibblyWires : ModuleRules
{
	public WibblyWires(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(new string[] { });

		PrivateIncludePaths.AddRange(new string[] { });

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"GraphEditor",
			"UnrealEd",
			"BlueprintGraph"
		});

		DynamicallyLoadedModuleNames.AddRange(new string[] { });
	}
}