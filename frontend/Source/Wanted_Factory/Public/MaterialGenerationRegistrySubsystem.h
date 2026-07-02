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

	bool FindDynamicMaterialRecord(FName MaterialId, FFactoryDynamicMaterialRecord& OutRecord) const;

	FText GetMaterialDisplayText(FName MaterialId) const;

	UTexture2D* GetMaterialThumbnailTexture(FName MaterialId);

	bool FindFirstRuntimeRecipe(
		const FName MachineType,
		const TMap<FName, int32>& InputInventory,
		FRecipeTable& OutRecipe) const;

private:
	UPROPERTY()
	TArray<FFactoryDynamicMaterialRecord> DynamicMaterials;

	UPROPERTY()
	TArray<FFactoryDynamicRecipeRecord> DynamicRecipes;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTexture2D>> ThumbnailTextureCache;

	UFUNCTION()
	void HandleMaterialGenerationResponse(const FFactoryMaterialGenerationResponse& Response);

	void RegisterDynamicMaterial(const FFactoryMaterialGenerationResponse& Response);
	void RegisterDynamicRecipe(
		const FFactoryPendingMaterialGenerationRequest& Request,
		const FFactoryMaterialGenerationResponse& Response);
	FLinearColor ResolveMaterialPreviewColor(const FFactoryDynamicMaterialRecord& MaterialRecord) const;
	UTexture2D* CreateGeneratedThumbnailTexture(FName MaterialId, const FFactoryDynamicMaterialRecord& MaterialRecord);
	UTexture2D* LoadTextureFromMaterialRecord(const FFactoryDynamicMaterialRecord& MaterialRecord);
	static FString BuildRecipeKey(const FName MachineType, const TArray<FFactoryMaterialRequestInput>& Inputs);
};
