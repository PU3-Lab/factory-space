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
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Json",
			"JsonUtilities",
			"WebSockets"
		});

		PublicIncludePaths.AddRange(new string[] {
			"Wanted_Factory",
			"Wanted_Factory/Variant_Strategy",
			"Wanted_Factory/Variant_Strategy/UI",
			"Wanted_Factory/Variant_TwinStick",
			"Wanted_Factory/Variant_TwinStick/AI",
			"Wanted_Factory/Variant_TwinStick/Gameplay",
			"Wanted_Factory/Variant_TwinStick/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
