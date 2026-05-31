#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FactorySpaceTypes.generated.h"

USTRUCT(BlueprintType)
struct FFactoryData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName FactoryID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UTexture2D* FactoryIcon;
};