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
			"SlateCore",
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Json",
			"JsonUtilities",
			"WebSockets",
			// 호버 표면 게이트의 ALandscapeProxy 판별(F2-1' — 지형 위 호버 사각지대 해소)
			"Landscape",
			// 램프 쐐기 비주얼 코드 생성(F3.8 — AOJJ_RampFoundation::OJJ_BuildWedgeVisual)
			"ProceduralMeshComponent",
			// ① 물 셀 인식 — AWaterBody/UWaterBodyComponent 질의(TryQueryWaterInfoClosestToWorldLocation).
			// 베이크 분류·수면Z가 WaterBodyRiver를 직접 인식(런타임 의존 — 베이크가 BeginPlay서도 돈다).
			"Water"
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
			// [#3 점진 건설] M_Hologram_BuildUp 그래프 코드 생성(UMaterialEditingLibrary) — 에디터 전용.
			PrivateDependencyModuleNames.Add("MaterialEditor");
			PrivateDependencyModuleNames.Add("AssetRegistry");
		}

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
