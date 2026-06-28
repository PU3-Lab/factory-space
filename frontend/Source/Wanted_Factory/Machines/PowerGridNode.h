// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MachineBase.h"
#include "PowerGridNode.generated.h"

class UPointLightComponent;

UCLASS()
class WANTED_FACTORY_API APowerGridNode : public AMachineBase
{
	GENERATED_BODY()

public:
	APowerGridNode();

	virtual void ApplyMachineData(const FMachineTableRow& MachineData) override;
	virtual bool AddItem(FName ItemID, int32 Count) override;
	virtual bool CanReceiveConveyorItem(FName ItemID, int32 Count = 1) const override;
	virtual bool CanPlayerInteract() const override { return false; }
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Machine | Power Grid")
	float GetConnectionRadius() const { return ConnectionRadius; }

	UFUNCTION(BlueprintPure, Category = "Machine | Power Grid")
	float GetSupplyRadius() const { return SupplyRadius; }

	UFUNCTION(BlueprintPure, Category = "Machine | Power Grid")
	bool IsPowerGridActive() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine | Power Grid")
	TObjectPtr<UPointLightComponent> NightIndicatorLight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine | Power Grid", meta = (ClampMin = "0.0"))
	float ConnectionRadius = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine | Power Grid", meta = (ClampMin = "0.0"))
	float SupplyRadius = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine | Power Grid|Light", meta = (ClampMin = "0", ClampMax = "23"))
	int32 NightLightStartHour24 = 18;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine | Power Grid|Light", meta = (ClampMin = "0", ClampMax = "23"))
	int32 NightLightEndHour24 = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine | Power Grid|Light")
	FVector NightIndicatorLightOffset = FVector(0.0f, 0.0f, 420.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine | Power Grid|Light", meta = (ClampMin = "0.0"))
	float NightIndicatorLightIntensity = 2200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine | Power Grid|Light", meta = (ClampMin = "0.0"))
	float NightIndicatorLightRadius = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine | Power Grid|Light")
	FLinearColor NightIndicatorLightColor = FLinearColor(1.0f, 0.72f, 0.18f, 1.0f);

private:
	void UpdateNightIndicatorLight();
	bool ShouldEnableNightIndicatorLight() const;
	void RegisterToPowerGrid();
	void UnregisterFromPowerGrid();
};
