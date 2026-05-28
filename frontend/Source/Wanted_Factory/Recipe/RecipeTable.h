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
	FName InputItem1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName InputItem2;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName InputItem3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 InputQty1 = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 InputQty2 = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 InputQty3 = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName OutputItem1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName OutputItem2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 OutputQty1 = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 OutputQty2 = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CraftingTime = 1.f;
};
