// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MachineBase.h"
#include "PowerGridNode.generated.h"

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

	UFUNCTION(BlueprintPure, Category = "Machine | Power Grid")
	float GetConnectionRadius() const { return ConnectionRadius; }

	UFUNCTION(BlueprintPure, Category = "Machine | Power Grid")
	float GetSupplyRadius() const { return SupplyRadius; }

	UFUNCTION(BlueprintPure, Category = "Machine | Power Grid")
	bool IsPowerGridActive() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine | Power Grid", meta = (ClampMin = "0.0"))
	float ConnectionRadius = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine | Power Grid", meta = (ClampMin = "0.0"))
	float SupplyRadius = 700.0f;

private:
	void RegisterToPowerGrid();
	void UnregisterFromPowerGrid();
};
