#pragma once

#include "CoreMinimal.h"
#include "MaterialGenerationRuntimeTypes.generated.h"

USTRUCT(BlueprintType)
struct FFactoryDynamicMaterialRecord
{
	GENERATED_BODY()

	UPROPERTY()
	FName MaterialId = NAME_None;

	UPROPERTY()
	FString Name;

	UPROPERTY()
	FString State;

	UPROPERTY()
	FString RowName;

	UPROPERTY()
	FString Form;

	UPROPERTY()
	FString Substance;

	UPROPERTY()
	FString MaterialType;

	UPROPERTY()
	FString Shape;

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	FString VisualColor;

	UPROPERTY()
	FString VisualAssetKey;

	UPROPERTY()
	FString TextureAssetKey;

	UPROPERTY()
	FString ThumbnailAssetKey;

	UPROPERTY()
	FString FallbackIcon;

	UPROPERTY()
	FString Message;
};

USTRUCT(BlueprintType)
struct FFactoryDynamicRecipeRecord
{
	GENERATED_BODY()

	UPROPERTY()
	FString RecipeKey;

	UPROPERTY()
	FName MachineType = NAME_None;

	UPROPERTY()
	FName InputItem1 = NAME_None;

	UPROPERTY()
	int32 InputQty1 = 0;

	UPROPERTY()
	FName InputItem2 = NAME_None;

	UPROPERTY()
	int32 InputQty2 = 0;

	UPROPERTY()
	FName InputItem3 = NAME_None;

	UPROPERTY()
	int32 InputQty3 = 0;

	UPROPERTY()
	FName OutputItem1 = NAME_None;

	UPROPERTY()
	int32 OutputQty1 = 0;

	UPROPERTY()
	FName OutputItem2 = NAME_None;

	UPROPERTY()
	int32 OutputQty2 = 0;

	UPROPERTY()
	float CraftingTime = 1.0f;
};
