// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PowerPlant.h"
#include "ThermalPowerPlant.generated.h"

UCLASS()
class WANTED_FACTORY_API AThermalPowerPlant : public APowerPlant
{
	GENERATED_BODY()

public:
	AThermalPowerPlant();
	virtual bool AddItem(FName ItemID, int32 Count) override;
	virtual bool CanReceiveConveyorItem(FName ItemID, int32 Count = 1) const override;
	virtual bool CanGeneratePower() const override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual float CalculatePowerOutput() const override;

	bool IsValidFuel(FName ItemID) const;
	void StartNextFuelProcessing();
	void FinishFuelProcessing();
	void RequestPowerGridRefresh() const;
	FName GetBufferedFuelItem() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Machine | Thermal Power Plant")
	float CoalPowerOutput = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Machine | Thermal Power Plant")
	float PetroliumPowerOutput = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Machine | Thermal Power Plant", meta = (ClampMin = "0.01"))
	float FuelProcessSeconds = 3.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine | Thermal Power Plant")
	FName ActiveFuelItem = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine | Thermal Power Plant")
	float ActiveFuelPowerOutput = 0.0f;

	FTimerHandle FuelProcessTimerHandle;
};
