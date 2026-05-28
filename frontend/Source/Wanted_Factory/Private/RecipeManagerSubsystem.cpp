// Fill out your copyright notice in the Description page of Project Settings.


#include "RecipeManagerSubsystem.h"

#include "Wanted_Factory.h"


void URecipeManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (!RecipeTable)
	{
		RecipeTable = LoadObject<UDataTable>(
			nullptr,
			TEXT("/Game/DataTable/DT_RecipeTable.DT_RecipeTable")
		);

		if (RecipeTable)
		{
			LOG_SSR_W(TEXT("RecipeTable Loaded"));
		}
		else
		{
			LOG_SSR_W(TEXT("RecipeTable Load Failed"));
			return;
		}
	}


	BuildRecipeIndex();
}

void URecipeManagerSubsystem::BuildRecipeIndex()
{
	if (!RecipeTable)
	{
		LOG_SSR_W(
			TEXT("RecipeTable is NULL"));
		return;
	}

	InputToRecipeMap.Empty();

	TArray<FName> RowNames =
		RecipeTable->GetRowNames();

	for (const FName& RowName : RowNames)
	{
		FRecipeTable* Recipe =
			RecipeTable->FindRow<FRecipeTable>(
				RowName,
				TEXT("")
			);

		if (!Recipe)
		{
			continue;
		}

		InputToRecipeMap.Add(
			Recipe->InputItem,
			RowName
		);
	}
}

bool URecipeManagerSubsystem::FindRecipeByInputItem(FName InputItem, FRecipeTable& OutRecipe)
{
	if (!RecipeTable)
	{
		return false;
	}

	FName* FoundRecipeName =
		InputToRecipeMap.Find(InputItem);

	if (!FoundRecipeName)
	{
		return false;
	}

	FRecipeTable* Recipe =
		RecipeTable->FindRow<FRecipeTable>(
			*FoundRecipeName,
			TEXT("")
		);

	if (!Recipe)
	{
		return false;
	}

	OutRecipe = *Recipe;
	return true;
}
