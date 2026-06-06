// Fill out your copyright notice in the Description page of Project Settings.


#include "PowerGridNode.h"

#include "FactoryManagerSubsystem.h"
#include "Wanted_Factory.h"

APowerGridNode::APowerGridNode()
{
	PrimaryActorTick.bCanEverTick = true;

	MachineType = TEXT("PowerGridNode");
	InputPortCount = 0;
	OutputPortCount = 0;
	bNeedPower = false;
	PowerConsumption = 0.f;
	MaxDurability = 1000.f;
	CurrentDurability = MaxDurability;
	bDisableWhenBroken = true;

	InputPorts.Reset();
	OutputPorts.Reset();
}

void APowerGridNode::ApplyMachineData(const FMachineTableRow& MachineData)
{
	Super::ApplyMachineData(MachineData);

	PowerConsumption = 0.f;
	bNeedPower = false;
}

void APowerGridNode::BeginPlay()
{
	Super::BeginPlay();
	RegisterToPowerGrid();
}

void APowerGridNode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromPowerGrid();
	Super::EndPlay(EndPlayReason);
}

bool APowerGridNode::AddItem(FName ItemID, int32 Count)
{
	LOG_SSR_W(TEXT("PowerGridNode has no input port and cannot receive item: %s"), *ItemID.ToString());
	return false;
}

bool APowerGridNode::CanReceiveConveyorItem(FName ItemID, int32 Count) const
{
	return false;
}

bool APowerGridNode::IsPowerGridActive() const
{
	return !(isBroken() && bDisableWhenBroken);
}

void APowerGridNode::RegisterToPowerGrid()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFactoryManagerSubsystem* FactoryManager = GameInstance->GetSubsystem<UFactoryManagerSubsystem>())
		{
			FactoryManager->RegisterPowerGridNode(this);
		}
	}
}

void APowerGridNode::UnregisterFromPowerGrid()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFactoryManagerSubsystem* FactoryManager = GameInstance->GetSubsystem<UFactoryManagerSubsystem>())
		{
			FactoryManager->UnregisterPowerGridNode(this);
		}
	}
}
