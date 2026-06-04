#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ResourceData.generated.h"

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
	Powder
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
};