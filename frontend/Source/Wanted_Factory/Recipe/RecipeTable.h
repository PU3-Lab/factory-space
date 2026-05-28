#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "RecipeTable.generated.h"

USTRUCT(BlueprintType)
struct FRecipeTable : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName MachineType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName InputItem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 InputQty = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName OutputItem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 OutputQty = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CraftingTime = 1.f;
};
