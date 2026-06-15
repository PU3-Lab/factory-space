// Fill out your copyright notice in the Description page of Project Settings.


#include "NuclearPowerPlant.h"

#include "FactoryManagerSubsystem.h"

namespace
{
	const FName UraniumItemName(TEXT("uranium"));
}

ANuclearPowerPlant::ANuclearPowerPlant()
{
	MachineType = TEXT("NuclearPowerPlant");
	BasePowerOutput = 30.0f;
}

void ANuclearPowerPlant::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(FuelProcessTimerHandle);

	Super::EndPlay(EndPlayReason);
}

bool ANuclearPowerPlant::AddItem(FName ItemID, int32 Count)
{
	if (isBroken() && bDisableWhenBroken)
	{
		return false;
	}

	if (ItemID != UraniumItemName || Count <= 0 || !CanAddInputItem(ItemID, Count))
	{
		return false;
	}

	int32& ItemCount = InputInventory.FindOrAdd(ItemID);
	ItemCount += Count;

	for (int32 Index = 0; Index < Count; ++Index)
	{
		QueuedFuelItems.Add(ItemID);
	}

	UpdateDebugBufferText();

	if (!bIsProcessingFuel)
	{
		StartNextFuelProcessing();
	}

	return true;
}

bool ANuclearPowerPlant::CanReceiveConveyorItem(FName ItemID, int32 Count) const
{
	return ItemID == UraniumItemName && CanAddInputItem(ItemID, Count);
}

bool ANuclearPowerPlant::CanGeneratePower() const
{
	return bIsProcessingFuel && Super::CanGeneratePower();
}

float ANuclearPowerPlant::CalculatePowerOutput() const
{
	return UraniumPowerOutput;
}

void ANuclearPowerPlant::StartNextFuelProcessing()
{
	if (QueuedFuelItems.Num() <= 0)
	{
		bIsProcessingFuel = false;
		MachineState = EMachineState::Idle;
		RequestPowerGridRefresh();
		return;
	}

	QueuedFuelItems.RemoveAt(0);

	int32* FuelCount = InputInventory.Find(UraniumItemName);
	if (!FuelCount || *FuelCount <= 0)
	{
		InputInventory.Remove(UraniumItemName);
		bIsProcessingFuel = false;
		StartNextFuelProcessing();
		return;
	}

	--(*FuelCount);
	if (*FuelCount <= 0)
	{
		InputInventory.Remove(UraniumItemName);
	}

	bIsProcessingFuel = true;
	MachineState = EMachineState::Working;
	UpdateDebugBufferText();
	RequestPowerGridRefresh();

	GetWorldTimerManager().SetTimer(
		FuelProcessTimerHandle,
		this,
		&ANuclearPowerPlant::FinishFuelProcessing,
		FuelProcessSeconds,
		false);
}

void ANuclearPowerPlant::FinishFuelProcessing()
{
	bIsProcessingFuel = false;

	StartNextFuelProcessing();
}

void ANuclearPowerPlant::RequestPowerGridRefresh() const
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFactoryManagerSubsystem* FactoryManager = GameInstance->GetSubsystem<UFactoryManagerSubsystem>())
		{
			FactoryManager->UpdatePowerGrid();
		}
	}
}
