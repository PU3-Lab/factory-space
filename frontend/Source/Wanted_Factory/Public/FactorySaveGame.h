#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "PlanetEventManagerSubsystem.h"
#include "QuestManagerSubsystem.h"
#include "Recipe/RecipeTable.h"
#include "FactorySaveGame.generated.h"

USTRUCT(BlueprintType)
struct FFactorySavedMachineData
{
	GENERATED_BODY()

	UPROPERTY()
	int32 InstanceId = INDEX_NONE;

	UPROPERTY()
	FString ClassPath;

	UPROPERTY()
	FIntPoint Origin = FIntPoint::ZeroValue;

	UPROPERTY()
	int32 RotationSteps = 0;

	UPROPERTY()
	FTransform Transform = FTransform::Identity;

	UPROPERTY()
	bool bOccupancyOnly = false;

	UPROPERTY()
	TMap<FName, int32> InputInventory;

	UPROPERTY()
	TMap<FName, int32> OutputBuffer;

	UPROPERTY()
	float CurrentDurability = 0.0f;

	UPROPERTY()
	FName SelectedOutputItemId = NAME_None;
};

USTRUCT(BlueprintType)
struct FFactorySavedConveyorData
{
	GENERATED_BODY()

	UPROPERTY()
	FString ClassPath;

	UPROPERTY()
	TArray<FIntPoint> PathCells;

	UPROPERTY()
	TArray<FName> ItemSlots;
};

USTRUCT(BlueprintType)
struct FFactorySavedPipeSlot
{
	GENERATED_BODY()

	UPROPERTY()
	FName LiquidId = NAME_None;

	UPROPERTY()
	int32 Amount = 0;
};

USTRUCT(BlueprintType)
struct FFactorySavedPipeData
{
	GENERATED_BODY()

	UPROPERTY()
	FString ClassPath;

	UPROPERTY()
	TArray<FIntPoint> PathCells;

	UPROPERTY()
	TArray<FFactorySavedPipeSlot> LiquidSlots;
};

USTRUCT(BlueprintType)
struct FFactorySavedPowerLineData
{
	GENERATED_BODY()

	UPROPERTY()
	FString ClassPath;

	UPROPERTY()
	int32 SourceMachineId = INDEX_NONE;

	UPROPERTY()
	int32 TargetMachineId = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct FFactorySavedFoundationData
{
	GENERATED_BODY()

	UPROPERTY()
	FString ClassPath;

	UPROPERTY()
	FIntPoint Origin = FIntPoint::ZeroValue;

	UPROPERTY()
	FIntPoint Size = FIntPoint(1, 1);

	UPROPERTY()
	int32 RotationSteps = 0;

	UPROPERTY()
	FTransform Transform = FTransform::Identity;

	UPROPERTY()
	float SurfaceZ = 0.0f;

	UPROPERTY()
	int32 RiseSteps = 0;

	UPROPERTY()
	bool bOneSideGroundRamp = false;

	UPROPERTY()
	float LoEndLowestGroundRaw = 0.0f;

	UPROPERTY()
	bool bLoEndLowestValid = false;

	UPROPERTY()
	TArray<float> CellSurfaceZs;
};

USTRUCT(BlueprintType)
struct FFactorySavedLadderData
{
	GENERATED_BODY()

	UPROPERTY()
	FString ClassPath;

	UPROPERTY()
	FTransform Transform = FTransform::Identity;

	UPROPERTY()
	float ClimbHeight = 300.0f;
};

UCLASS()
class WANTED_FACTORY_API UFactorySaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FTransform PlayerTransform = FTransform::Identity;

	UPROPERTY()
	TMap<FName, int32> WarehouseItems;

	UPROPERTY()
	TMap<FName, int32> MachineLevels;

	UPROPERTY()
	int32 CurrentMainQuestIndex = 0;

	UPROPERTY()
	TArray<FQuestState> MainQuestSequence;

	UPROPERTY()
	TArray<FQuestState> SubQuests;

	UPROPERTY()
	TArray<FString> SubQuestTitles;

	UPROPERTY()
	bool bTutorialQuestTestActive = false;

	UPROPERTY()
	FString CurrentTutorialQuestId;

	UPROPERTY()
	bool bPendingTutorialStartDialogueReveal = false;

	UPROPERTY()
	FString LastTutorialDialogueQuestId;

	UPROPERTY()
	FString LastTutorialDialogueTriggerType;

	UPROPERTY()
	TArray<FTutorialQuestDialogueLine> LastTutorialDialogueLines;

	UPROPERTY()
	TMap<FString, int32> TutorialProgressCounts;

	UPROPERTY()
	FPlanetTimeState TimeState;

	UPROPERTY()
	FPlanetWeatherState WeatherState;

	UPROPERTY()
	FPlanetEventState EventState;

	UPROPERTY()
	TArray<FFactorySavedFoundationData> Foundations;

	UPROPERTY()
	TArray<FFactorySavedMachineData> Machines;

	UPROPERTY()
	TArray<FFactorySavedConveyorData> Conveyors;

	UPROPERTY()
	TArray<FFactorySavedPipeData> Pipes;

	UPROPERTY()
	TArray<FFactorySavedPowerLineData> PowerLines;

	UPROPERTY()
	TArray<FFactorySavedLadderData> Ladders;
};
