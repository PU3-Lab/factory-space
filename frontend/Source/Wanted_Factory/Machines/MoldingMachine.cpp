// Fill out your copyright notice in the Description page of Project Settings.


#include "MoldingMachine.h"

#include "Engine/DataTable.h"
#include "RecipeManagerSubsystem.h"
#include "Resource/ResourceData.h"
#include "Wanted_Factory.h"

namespace
{
	constexpr TCHAR ResourceTablePath[] = TEXT("/Game/DataTable/DT_ResourceData.DT_ResourceData");

	EResourceShape ResolveMoldingShape(const FString& ShapeText)
	{
		if (ShapeText == TEXT("판"))
		{
			return EResourceShape::plate;
		}

		if (ShapeText == TEXT("봉"))
		{
			return EResourceShape::bar;
		}

		if (ShapeText == TEXT("선"))
		{
			return EResourceShape::wire;
		}

		return EResourceShape::None;
	}
}

// Sets default values
AMoldingMachine::AMoldingMachine()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	MachineType = TEXT("MoldingMachine");
	bNeedPower = true;
	bDisableWhenBroken = true;
}

void AMoldingMachine::ApplyMachineData(const FMachineTableRow& MachineData)
{
	FMachineTableRow MoldingMachineData = MachineData;
	MoldingMachineData.InputPortCnt = 1;
	MoldingMachineData.OutputPortCnt = 1;
	Super::ApplyMachineData(MoldingMachineData);
}

bool AMoldingMachine::AddItem(FName ItemID, int32 Count)
{
	if (!CanReceiveConveyorItem(ItemID, Count))
	{
		// LOG_SSR_W(TEXT("MoldingMachine rejected input item: %s"), *ItemID.ToString());
		return false;
	}

	return Super::AddItem(ItemID, Count);
}

void AMoldingMachine::TryStartProcess()
{
	RefreshMachineState();

	if (MachineState == EMachineState::Working)
	{
		return;
	}

	if (MachineState == EMachineState::Disabled ||
		MachineState == EMachineState::NoPower ||
		MachineState == EMachineState::Blocked)
	{
		return;
	}

	if (InputInventory.Num() <= 0)
	{
		return;
	}

	URecipeManagerSubsystem* RecipeManager =
		GetGameInstance() ? GetGameInstance()->GetSubsystem<URecipeManagerSubsystem>() : nullptr;

	if (!RecipeManager)
	{
		// LOG_SSR_W(TEXT("RecipeManagerSubSystem is NULL"));
		return;
	}

	bool bHasBlockedCraftableRecipe = false;

	for (const TPair<FName, int32>& InputPair : InputInventory)
	{
		TArray<FRecipeTable> FoundRecipes;
		if (!RecipeManager->FindRecipesByInputItem(InputPair.Key, FoundRecipes))
		{
			continue;
		}

		for (const FRecipeTable& Recipe : FoundRecipes)
		{
			if (Recipe.MachineType != MachineType || !DoesRecipeMatchCurrentShape(Recipe))
			{
				continue;
			}

			if (!HasEnoughIngredients(Recipe))
			{
				continue;
			}

			if (!CanAddToOutputBuffer(Recipe))
			{
				bHasBlockedCraftableRecipe = true;
				continue;
			}

			CurrentRecipe = Recipe;
			ProcessTime = CurrentRecipe.CraftingTime;

			StartProcess();
			return;
		}
	}

	if (bHasBlockedCraftableRecipe)
	{
		MachineState = EMachineState::Blocked;
		// LOG_SSR_W(TEXT("Cannot start process. Output Buffer Blocked."));
		return;
	}

	// LOG_SSR_W(TEXT("No craftable molding recipe found for shape: %s"), *CurrentShape);
}

void AMoldingMachine::AddOutputItem(FName ItemID, int32 Count)
{
	for (const TPair<FName, int32>& Output : OutputBuffer)
	{
		if (!Output.Key.IsNone() && Output.Value > 0 && Output.Key != ItemID)
		{
			MachineState = EMachineState::Blocked;

			// LOG_SSR_W(
			// 	TEXT("MoldingMachine output buffer already contains another item: %s"),
			// 	*Output.Key.ToString()
			// );
			return;
		}
	}

	Super::AddOutputItem(ItemID, Count);
}

bool AMoldingMachine::CanAddToOutputBuffer(const FRecipeTable& Recipe) const
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

bool AMoldingMachine::CanReceiveConveyorItem(FName ItemID, int32 Count) const
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

	bool bHasMoldingMachineRecipe = false;
	for (const FRecipeTable& Recipe : FoundRecipes)
	{
		if (Recipe.MachineType == MachineType && DoesRecipeMatchCurrentShape(Recipe))
		{
			bHasMoldingMachineRecipe = true;
			break;
		}
	}

	if (!bHasMoldingMachineRecipe)
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

bool AMoldingMachine::DoesRecipeMatchCurrentShape(const FRecipeTable& Recipe) const
{
	const EResourceShape DesiredShape = ResolveMoldingShape(CurrentShape);
	if (DesiredShape == EResourceShape::None || Recipe.OutputItem1.IsNone())
	{
		return false;
	}

	const UDataTable* ResourceTable = LoadObject<UDataTable>(nullptr, ResourceTablePath);
	if (!ResourceTable)
	{
		return false;
	}

	const FResourceData* OutputResource =
		ResourceTable->FindRow<FResourceData>(Recipe.OutputItem1, TEXT("MoldingMachine.DoesRecipeMatchCurrentShape"));
	return OutputResource && OutputResource->shape == DesiredShape;
}
