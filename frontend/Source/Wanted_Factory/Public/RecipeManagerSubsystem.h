// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Recipe/RecipeTable.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RecipeManagerSubsystem.generated.h"

UCLASS()
class WANTED_FACTORY_API URecipeManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	URecipeManagerSubsystem();

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	UDataTable* RecipeTable;

	// Key : 재료 ID
	// Value : 해당 재료가 포함된 레시피 RowName 목록
	
	TMap<FName, TArray<FName>> InputToRecipeMap;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	void BuildRecipeIndex();

	bool FindRecipeByInputItem(FName InputItem, FRecipeTable& OutRecipe);
	
	bool FindRecipesByInputItem(FName InputItem, TArray<FRecipeTable>& OutRecipes);
};
