// Fill out your copyright notice in the Description page of Project Settings.


#include "ThermalPowerPlant.h"

#include "FactoryManagerSubsystem.h"

namespace
{
	const FName CoalItemName(TEXT("coal"));
	const FName PetroliumItemName(TEXT("petrolium"));
}

AThermalPowerPlant::AThermalPowerPlant()
{
	MachineType = TEXT("ThermalPowerPlant");
}

void AThermalPowerPlant::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(FuelProcessTimerHandle);

	Super::EndPlay(EndPlayReason);
}

bool AThermalPowerPlant::AddItem(FName ItemID, int32 Count)
{
	if (isBroken() && bDisableWhenBroken)
	{
		return false;
	}

	if (!IsValidFuel(ItemID) || Count <= 0 || !CanAddInputItem(ItemID, Count))
	{
		return false;
	}

	int32& ItemCount = InputInventory.FindOrAdd(ItemID);
	ItemCount += Count;

	UpdateDebugBufferText();

	if (ActiveFuelItem.IsNone())
	{
		StartNextFuelProcessing();
	}

	return true;
}

bool AThermalPowerPlant::CanReceiveConveyorItem(FName ItemID, int32 Count) const
{
	if (!IsValidFuel(ItemID) || !CanAddInputItem(ItemID, Count))
	{
		return false;
	}

	for (const TPair<FName, int32>& Input : InputInventory)
	{
		if (!Input.Key.IsNone() && Input.Value > 0 && Input.Key != ItemID)
		{
			return false;
		}
	}

	return true;
}

bool AThermalPowerPlant::CanGeneratePower() const
{
	return !ActiveFuelItem.IsNone() && Super::CanGeneratePower();
}

float AThermalPowerPlant::CalculatePowerOutput() const
{
	return ActiveFuelPowerOutput;
}

bool AThermalPowerPlant::IsValidFuel(FName ItemID) const
{
	return ItemID == CoalItemName || ItemID == PetroliumItemName;
}

void AThermalPowerPlant::StartNextFuelProcessing()
{
	const FName NextFuelItem = GetBufferedFuelItem();
	if (NextFuelItem.IsNone())
	{
		ActiveFuelItem = NAME_None;
		ActiveFuelPowerOutput = 0.0f;
		MachineState = EMachineState::Idle;
		RequestPowerGridRefresh();
		return;
	}

	ActiveFuelItem = NextFuelItem;
	ActiveFuelPowerOutput = ActiveFuelItem == CoalItemName ? CoalPowerOutput : PetroliumPowerOutput;
	MachineState = EMachineState::Working;
	UpdateDebugBufferText();
	RequestPowerGridRefresh();

	GetWorldTimerManager().SetTimer(
		FuelProcessTimerHandle,
		this,
		&AThermalPowerPlant::FinishFuelProcessing,
		FuelProcessSeconds,
		false);
}

void AThermalPowerPlant::FinishFuelProcessing()
{
	int32* FuelCount = InputInventory.Find(ActiveFuelItem);
	if (FuelCount)
	{
		--(*FuelCount);
		if (*FuelCount <= 0)
		{
			InputInventory.Remove(ActiveFuelItem);
		}
	}

	ActiveFuelItem = NAME_None;
	ActiveFuelPowerOutput = 0.0f;
	UpdateDebugBufferText();

	StartNextFuelProcessing();
}

void AThermalPowerPlant::RequestPowerGridRefresh() const
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFactoryManagerSubsystem* FactoryManager = GameInstance->GetSubsystem<UFactoryManagerSubsystem>())
		{
			FactoryManager->UpdatePowerGrid();
		}
	}
}

FName AThermalPowerPlant::GetBufferedFuelItem() const
{
	for (const TPair<FName, int32>& Input : InputInventory)
	{
		if (!Input.Key.IsNone() && Input.Value > 0)
		{
			return Input.Key;
		}
	}

	return NAME_None;
}
