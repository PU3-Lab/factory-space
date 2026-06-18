#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ResourceData.generated.h"

class UStaticMesh;

UENUM(BlueprintType)
enum class EResourceType : uint8
{
	None,
	Organism,
	Fuel,
	Metal,
	Nonmetal
};

UENUM(BlueprintType)
enum class EResourceShape : uint8
{
	None,
	Ore,
	Ingot,
	Powder,
	bar,
	wire,
	plate
};

USTRUCT(BlueprintType)
struct FResourceData : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName form;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName substance;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EResourceType type = EResourceType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EResourceShape shape = EResourceShape::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor VisualColor = FLinearColor::White;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UTexture2D> ImgAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UStaticMesh> StaticMeshAsset;
};
