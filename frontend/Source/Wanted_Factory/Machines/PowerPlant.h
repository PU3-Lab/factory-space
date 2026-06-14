// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MachineBase.h"
#include "PowerPlant.generated.h"

UCLASS()
class WANTED_FACTORY_API APowerPlant : public AMachineBase
{
	GENERATED_BODY()

public:
	APowerPlant();

	virtual void ApplyMachineData(const FMachineTableRow& MachineData) override;
	virtual bool AddItem(FName ItemID, int32 Count) override;
	virtual bool CanReceiveConveyorItem(FName ItemID, int32 Count = 1) const override;

	UFUNCTION(BlueprintPure, Category = "Machine | Power Plant")
	virtual bool CanGeneratePower() const;

	UFUNCTION(BlueprintPure, Category = "Machine | Power Plant")
	float GetCurrentPowerOutput() const;

	UFUNCTION(BlueprintPure, Category = "Machine | Power Plant")
	float GetBasePowerOutput() const { return BasePowerOutput; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Machine | Power Plant", meta = (ClampMin = "0.0"))
	float BasePowerOutput = 20.0f;

	virtual float CalculatePowerOutput() const;
};
