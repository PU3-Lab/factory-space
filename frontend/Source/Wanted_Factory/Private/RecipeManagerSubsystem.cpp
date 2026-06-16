#include "RecipeManagerSubsystem.h"

#include "Wanted_Factory.h"
#include "Engine/DataTable.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr TCHAR RecipeCsvRelativePath[] = TEXT("Source/Wanted_Factory/Data/RecipeTable.csv");
}

URecipeManagerSubsystem::URecipeManagerSubsystem()
{
	static ConstructorHelpers::FObjectFinder<UDataTable> RecipeTableFinder(
		TEXT("/Game/DataTable/DT_RecipeTable.DT_RecipeTable")
	);

	if (RecipeTableFinder.Succeeded())
	{
		RecipeTable = RecipeTableFinder.Object;
	}
}

void URecipeManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (!RecipeTable)
	{
		RecipeTable = NewObject<UDataTable>(this, TEXT("RecipeTable"));
		if (RecipeTable)
		{
			RecipeTable->RowStruct = FRecipeTable::StaticStruct();
		}
	}

	if (!RecipeTable)
	{
		LOG_SSR_W(TEXT("RecipeTable Load Failed"));
		return;
	}

	LOG_SSR_W(TEXT("RecipeTable Loaded"));

	const FString CsvPath = FPaths::Combine(FPaths::ProjectDir(), RecipeCsvRelativePath);
	FString CsvContent;
	if (FFileHelper::LoadFileToString(CsvContent, *CsvPath))
	{
		RecipeTable->EmptyTable();
		const TArray<FString> ImportProblems = RecipeTable->CreateTableFromCSVString(CsvContent);
		for (const FString& Problem : ImportProblems)
		{
			LOG_SSR_W(TEXT("RecipeTable CSV import warning: %s"), *Problem);
		}
	}
	else
	{
		LOG_SSR_W(TEXT("RecipeTable CSV Load Failed: %s"), *CsvPath);
	}

	BuildRecipeIndex();
}

void URecipeManagerSubsystem::BuildRecipeIndex()
{
	if (!RecipeTable)
	{
		LOG_SSR_W(TEXT("RecipeTable is NULL"));
		return;
	}

	InputToRecipeMap.Empty();

	TArray<FName> RowNames = RecipeTable->GetRowNames();

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

		auto AddInputIndex = [&](FName InputItem)
		{
			if (InputItem.IsNone())
			{
				return;
			}

			TArray<FName>& RecipeList =
				InputToRecipeMap.FindOrAdd(InputItem);

			RecipeList.AddUnique(RowName);

			LOG_SSR_W(
				TEXT("Recipe Indexed: %s -> %s"),
				*InputItem.ToString(),
				*RowName.ToString()
			);
		};

		AddInputIndex(Recipe->InputItem1);
		AddInputIndex(Recipe->InputItem2);
		AddInputIndex(Recipe->InputItem3);
	}
}

bool URecipeManagerSubsystem::FindRecipeByInputItem(
	FName InputItem,
	FRecipeTable& OutRecipe)
{
	TArray<FRecipeTable> FoundRecipes;

	if (!FindRecipesByInputItem(InputItem, FoundRecipes))
	{
		return false;
	}

	OutRecipe = FoundRecipes[0];
	return true;
}

bool URecipeManagerSubsystem::FindRecipesByInputItem(
	FName InputItem,
	TArray<FRecipeTable>& OutRecipes)
{
	OutRecipes.Empty();

	if (!RecipeTable)
	{
		return false;
	}

	const TArray<FName>* FoundRecipeNames =
		InputToRecipeMap.Find(InputItem);

	if (!FoundRecipeNames || FoundRecipeNames->Num() <= 0)
	{
		return false;
	}

	for (const FName& RecipeRowName : *FoundRecipeNames)
	{
		FRecipeTable* Recipe =
			RecipeTable->FindRow<FRecipeTable>(
				RecipeRowName,
				TEXT("")
			);

		if (!Recipe)
		{
			continue;
		}

		OutRecipes.Add(*Recipe);
	}

	return OutRecipes.Num() > 0;
}
