// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PowerPlant.h"
#include "SolarPowerPlant.generated.h"

UCLASS()
class WANTED_FACTORY_API ASolarPowerPlant : public APowerPlant
{
	GENERATED_BODY()

public:
	ASolarPowerPlant();

protected:
	virtual bool CanGeneratePower() const override;
	virtual float CalculatePowerOutput() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Machine | Solar Power Plant", meta = (ClampMin = "0.0"))
	float MaxSolarPowerOutput = 20.0f;
};
