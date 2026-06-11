// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Wanted_Factory : ModuleRules
{
	public Wanted_Factory(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"Niagara",
			"UMG",
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Json",
			"JsonUtilities",
			"WebSockets",
			// 호버 표면 게이트의 ALandscapeProxy 판별(F2-1' — 지형 위 호버 사각지대 해소)
			"Landscape"
		});

		PublicIncludePaths.AddRange(new string[] {
			"Wanted_Factory"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
