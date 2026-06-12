// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PowerPlant.h"
#include "Resource/ResourceBase.h"
#include "HydroPowerPlant.generated.h"

class AOJJ_Grid;

UCLASS()
class WANTED_FACTORY_API AHydroPowerPlant : public APowerPlant
{
	GENERATED_BODY()

public:
	AHydroPowerPlant();

protected:
	virtual bool CanPlaceAdditional(const AOJJ_Grid* Grid, FIntPoint Origin, int32 RotationSteps) const override;
	virtual void OnPlacedOnGrid(AOJJ_Grid* Grid, FIntPoint Origin, int32 RotationSteps) override;
	virtual void OnRemovedFromGrid() override;
	virtual bool CanGeneratePower() const override;

	AResourceBase* FindAdjacentWater(const AOJJ_Grid* Grid, FIntPoint Origin, int32 RotationSteps) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine | Hydro Power Plant")
	AResourceBase* LinkedResource = nullptr;
};
