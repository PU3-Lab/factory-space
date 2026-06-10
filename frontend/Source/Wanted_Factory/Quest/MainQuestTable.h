#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MainQuestTable.generated.h"

UENUM(BlueprintType)
enum class EQuestObjectiveType : uint8
{
	None,
	InputAction,
	BuildPlacementMode,
	PlaceMachine,
	WarehouseStoreItem
};

USTRUCT(BlueprintType)
struct FQuestRewardItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest", meta = (ClampMin = "1"))
	int32 Quantity = 1;
};

USTRUCT(BlueprintType)
struct FMainQuestTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Order = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString QuestId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EQuestObjectiveType ObjectiveType1 = EQuestObjectiveType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName TargetId1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 RequiredCount1 = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EQuestObjectiveType ObjectiveType2 = EQuestObjectiveType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName TargetId2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 RequiredCount2 = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName RewardItemId1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 RewardQuantity1 = 0;
};
