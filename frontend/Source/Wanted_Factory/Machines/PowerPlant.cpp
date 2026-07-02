// Fill out your copyright notice in the Description page of Project Settings.


#include "PowerPlant.h"

#include "Wanted_Factory.h"

APowerPlant::APowerPlant()
{
	PrimaryActorTick.bCanEverTick = true;

	MachineType = TEXT("PowerPlant");
	bNeedPower = false;
	bDisableWhenBroken = true;
}

void APowerPlant::ApplyMachineData(const FMachineTableRow& MachineData)
{
	Super::ApplyMachineData(MachineData);

	BasePowerOutput = FMath::Max(0.f, MachineData.Power);
	PowerConsumption = 0.f;
	bNeedPower = false;
	RefreshPowerPlantState();
}

bool APowerPlant::AddItem(FName ItemID, int32 Count)
{
	// LOG_SSR_W(TEXT("PowerPlant has no input port and cannot receive item: %s"), *ItemID.ToString());
	return false;
}

bool APowerPlant::CanReceiveConveyorItem(FName ItemID, int32 Count) const
{
	return false;
}

bool APowerPlant::CanGeneratePower() const
{
	if (isBroken() && bDisableWhenBroken)
	{
		return false;
	}

	if (MachineState == EMachineState::Disabled)
	{
		return false;
	}

	return BasePowerOutput > 0.f;
}

float APowerPlant::GetCurrentPowerOutput() const
{
	return CanGeneratePower() ? CalculatePowerOutput() : 0.f;
}

float APowerPlant::CalculatePowerOutput() const
{
	return BasePowerOutput;
}

void APowerPlant::Tick(float DeltaSeconds)
{
	RefreshPowerPlantState();
	Super::Tick(DeltaSeconds);
}

void APowerPlant::RefreshPowerPlantState()
{
	EMachineState NewState = EMachineState::Idle;
	if (isBroken() && bDisableWhenBroken)
	{
		NewState = EMachineState::Disabled;
	}
	else if (CanGeneratePower() && GetCurrentPowerOutput() > 0.0f)
	{
		NewState = EMachineState::Working;
	}

	if (MachineState == NewState)
	{
		return;
	}

	MachineState = NewState;
	UpdateStateIndicator();
}
