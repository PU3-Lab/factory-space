// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PowerPlant.h"
#include "NuclearPowerPlant.generated.h"

UCLASS()
class WANTED_FACTORY_API ANuclearPowerPlant : public APowerPlant
{
	GENERATED_BODY()

public:
	ANuclearPowerPlant();
	virtual bool AddItem(FName ItemID, int32 Count) override;
	virtual bool CanReceiveConveyorItem(FName ItemID, int32 Count = 1) const override;
	virtual bool CanGeneratePower() const override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual float CalculatePowerOutput() const override;
	virtual void HandlePostRepair() override;

	void StartNextFuelProcessing();
	void FinishFuelProcessing();
	void RequestPowerGridRefresh() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Machine | Nuclear Power Plant")
	float UraniumPowerOutput = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Machine | Nuclear Power Plant", meta = (ClampMin = "0.01"))
	float FuelProcessSeconds = 3.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine | Nuclear Power Plant")
	bool bIsProcessingFuel = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine | Nuclear Power Plant")
	TArray<FName> QueuedFuelItems;

	FTimerHandle FuelProcessTimerHandle;
};
