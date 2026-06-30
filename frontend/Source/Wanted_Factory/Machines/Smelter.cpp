// Fill out your copyright notice in the Description page of Project Settings.


#include "Smelter.h"

#include "RecipeManagerSubsystem.h"
#include "Wanted_Factory.h"

// Sets default values
ASmelter::ASmelter()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	MachineType = TEXT("Smelter");
	bNeedPower = true;
	bDisableWhenBroken = true;
}

// Called when the game starts or when spawned
void ASmelter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ASmelter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

bool ASmelter::AddItem(FName ItemID, int32 Count)
{
	if (!CanReceiveConveyorItem(ItemID, Count))
	{
		// LOG_SSR_W(TEXT("Smelter rejected input item: %s"), *ItemID.ToString());
		return false;
	}

	return Super::AddItem(ItemID, Count);
}

void ASmelter::AddOutputItem(FName ItemID, int32 Count)
{
	for (const TPair<FName, int32>& Output : OutputBuffer)
	{
		if (!Output.Key.IsNone() && Output.Value > 0 && Output.Key != ItemID)
		{
			MachineState = EMachineState::Blocked;

			// LOG_SSR_W(
			// 	TEXT("Smelter output buffer already contains another item: %s"),
			// 	*Output.Key.ToString()
			// );
			return;
		}
	}

	Super::AddOutputItem(ItemID, Count);
}

bool ASmelter::CanAddToOutputBuffer(const FRecipeTable& Recipe) const
{
	int32 OutputItemCount = 0;
	FName RecipeOutputItem = NAME_None;

	auto CollectOutput = [&OutputItemCount, &RecipeOutputItem](FName ItemID, int32 Qty)
	{
		if (ItemID.IsNone() || Qty <= 0)
		{
			return;
		}

		++OutputItemCount;
		RecipeOutputItem = ItemID;
	};

	CollectOutput(Recipe.OutputItem1, Recipe.OutputQty1);
	CollectOutput(Recipe.OutputItem2, Recipe.OutputQty2);

	if (OutputItemCount > 1)
	{
		return false;
	}

	for (const TPair<FName, int32>& Output : OutputBuffer)
	{
		if (!Output.Key.IsNone() && Output.Value > 0 && Output.Key != RecipeOutputItem)
		{
			return false;
		}
	}

	return Super::CanAddToOutputBuffer(Recipe);
}

bool ASmelter::CanReceiveConveyorItem(FName ItemID, int32 Count) const
{
	if (!Super::CanReceiveConveyorItem(ItemID, Count))
	{
		return false;
	}

	URecipeManagerSubsystem* RecipeManager =
		GetGameInstance() ? GetGameInstance()->GetSubsystem<URecipeManagerSubsystem>() : nullptr;

	if (!RecipeManager)
	{
		return false;
	}

	TArray<FRecipeTable> FoundRecipes;
	if (!RecipeManager->FindRecipesByInputItem(ItemID, FoundRecipes))
	{
		return false;
	}

	bool bHasSmelterRecipe = false;
	for (const FRecipeTable& Recipe : FoundRecipes)
	{
		if (Recipe.MachineType == MachineType)
		{
			bHasSmelterRecipe = true;
			break;
		}
	}

	if (!bHasSmelterRecipe)
	{
		return false;
	}

	for (const TPair<FName, int32>& Input : InputInventory)
	{
		if (!Input.Key.IsNone() && Input.Value > 0 && Input.Key != ItemID)
		{
			return false;
		}
	}

	return true;
}

