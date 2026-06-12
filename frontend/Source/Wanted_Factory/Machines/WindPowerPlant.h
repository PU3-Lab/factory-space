// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PowerPlant.h"
#include "WindPowerPlant.generated.h"

UCLASS()
class WANTED_FACTORY_API AWindPowerPlant : public APowerPlant
{
	GENERATED_BODY()

public:
	AWindPowerPlant();

protected:
	virtual float CalculatePowerOutput() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Machine | Wind Power Plant", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinWindSpeed = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Machine | Wind Power Plant", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxWindSpeed = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Machine | Wind Power Plant", meta = (ClampMin = "0.0"))
	float MaxWindPowerOutput = 20.0f;
};
