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
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UDataTable* RecipeTable;

	TMap<FName, FName> InputToRecipeMap;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	void BuildRecipeIndex();

	bool FindRecipeByInputItem(FName InputItem, FRecipeTable& OutRecipe);
};
