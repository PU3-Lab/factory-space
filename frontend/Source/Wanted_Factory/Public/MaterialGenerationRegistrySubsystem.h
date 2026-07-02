#pragma once

#include "CoreMinimal.h"
#include "FactoryAgentClientSubsystem.h"
#include "MaterialGenerationRuntimeTypes.h"
#include "Recipe/RecipeTable.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MaterialGenerationRegistrySubsystem.generated.h"

UCLASS()
class WANTED_FACTORY_API UMaterialGenerationRegistrySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void ExportSaveData(
		TArray<FFactoryDynamicMaterialRecord>& OutMaterials,
		TArray<FFactoryDynamicRecipeRecord>& OutRecipes) const;

	void ImportSaveData(
		const TArray<FFactoryDynamicMaterialRecord>& InMaterials,
		const TArray<FFactoryDynamicRecipeRecord>& InRecipes);

	bool FindFirstRuntimeRecipe(
		const FName MachineType,
		const TMap<FName, int32>& InputInventory,
		FRecipeTable& OutRecipe) const;

private:
	UPROPERTY()
	TArray<FFactoryDynamicMaterialRecord> DynamicMaterials;

	UPROPERTY()
	TArray<FFactoryDynamicRecipeRecord> DynamicRecipes;

	UFUNCTION()
	void HandleMaterialGenerationResponse(const FFactoryMaterialGenerationResponse& Response);

	void RegisterDynamicMaterial(const FFactoryMaterialGenerationResponse& Response);
	void RegisterDynamicRecipe(
		const FFactoryPendingMaterialGenerationRequest& Request,
		const FFactoryMaterialGenerationResponse& Response);
	static FString BuildRecipeKey(const FName MachineType, const TArray<FFactoryMaterialRequestInput>& Inputs);
};
