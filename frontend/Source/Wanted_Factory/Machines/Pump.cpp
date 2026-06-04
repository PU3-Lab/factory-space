// Fill out your copyright notice in the Description page of Project Settings.


#include "Pump.h"

#include "Wanted_Factory.h"

namespace
{
	const FName LiquidFormName(TEXT("liquid"));
}

// Sets default values
APump::APump()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	MachineType = TEXT("Pump");
	InputPortCount = 1;
	OutputPortCount = 1;
	bNeedPower = true;
	PowerConsumption = 10.f;
	MaxDurability = 1000.f;
	CurrentDurability = MaxDurability;
	bDisableWhenBroken = true;

	InputPorts.Reset();
	FMachinePortData InputPort;
	InputPort.PortIndex = 0;
	InputPort.PortType = EPortType::Input;
	InputPorts.Add(InputPort);

	OutputPorts.Reset();
	FMachinePortData OutputPort;
	OutputPort.PortIndex = 0;
	OutputPort.PortType = EPortType::Output;
	OutputPorts.Add(OutputPort);
}

// Called when the game starts or when spawned
void APump::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void APump::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APump::SetLinkedResource(AResourceBase* NewResource)
{
	LinkedResource = NewResource;

	if (LinkedResource)
	{
		LOG_SSR_W(TEXT("Pump linked to resource: %s"), *LinkedResource->GetName());
	}
	else
	{
		LOG_SSR_W(TEXT("Pump linked resource is null."));
	}
}

bool APump::CanPump() const
{
	if (PumpAmount <= 0)
	{
		LOG_SSR_W(TEXT("CanPump failed: Invalid PumpAmount=%d."), PumpAmount);
		return false;
	}

	if (isBroken() && bDisableWhenBroken)
	{
		LOG_SSR_W(TEXT("CanPump failed: Pump is broken."));
		return false;
	}

	if (MachineState == EMachineState::Disabled || MachineState == EMachineState::NoPower)
	{
		LOG_SSR_W(TEXT("CanPump failed: Machine is not available."));
		return false;
	}

	if (!HasEnoughPower())
	{
		LOG_SSR_W(TEXT("CanPump failed: Not enough power."));
		return false;
	}

	if (!LinkedResource)
	{
		LOG_SSR_W(TEXT("CanPump failed: LinkedResource is null."));
		return false;
	}

	FResourceData Data;
	if (!LinkedResource->GetResourceData(Data))
	{
		LOG_SSR_W(
			TEXT("CanPump failed: ResourceData is invalid. Resource=%s RowName=%s"),
			*LinkedResource->GetName(),
			*LinkedResource->GetResourceRowName().ToString()
		);
		return false;
	}

	if (Data.form != LiquidFormName)
	{
		LOG_SSR_W(
			TEXT("CanPump failed: Resource form is not liquid. Resource=%s RowName=%s Form=%s"),
			*LinkedResource->GetName(),
			*LinkedResource->GetResourceRowName().ToString(),
			*Data.form.ToString()
		);
		return false;
	}

	const FName ResourceID = LinkedResource->GetResourceRowName();
	if (ResourceID.IsNone())
	{
		LOG_SSR_W(TEXT("CanPump failed: Resource row name is None."));
		return false;
	}

	const int32 CurrentOutputCount = OutputBuffer.FindRef(ResourceID);
	if (CurrentOutputCount + PumpAmount > MaxBufferPerItem)
	{
		LOG_SSR_W(
			TEXT("CanPump failed: Output buffer full. Item=%s Count=%d Max=%d"),
			*ResourceID.ToString(),
			CurrentOutputCount,
			MaxBufferPerItem
		);
		return false;
	}

	return true;
}

void APump::StartPumping()
{
	if (MachineState == EMachineState::Working)
	{
		return;
	}

	if (PumpAmount <= 0 || PumpInterval <= 0.f)
	{
		LOG_SSR_W(
			TEXT("Cannot start pumping. Invalid pump settings. PumpAmount=%d PumpInterval=%.2f"),
			PumpAmount,
			PumpInterval
		);
		return;
	}

	if (!CanPump())
	{
		LOG_SSR_W(TEXT("Cannot start pumping."));
		return;
	}

	MachineState = EMachineState::Working;
	GetWorldTimerManager().SetTimer(
		PumpTimerHandle,
		this,
		&APump::PumpResource,
		GetEffectiveProcessTime(PumpInterval),
		true
	);

	LOG_SSR_W(TEXT("Pumping started."));
}

void APump::StopPumping()
{
	GetWorldTimerManager().ClearTimer(PumpTimerHandle);
	StopProcess();

	LOG_SSR_W(TEXT("Pumping stopped."));
}

void APump::PumpResource()
{
	if (!CanPump())
	{
		StopPumping();
		return;
	}

	const FName ResourceID = LinkedResource->GetResourceRowName();
	if (!LinkedResource->ConsumeResource(PumpAmount))
	{
		LOG_SSR_W(TEXT("Liquid resource depleted."));
		StopPumping();
		return;
	}

	AddOutputItem(ResourceID, PumpAmount);

	LOG_SSR_W(TEXT("Pumped Liquid Resource: %s x %d"),
		*ResourceID.ToString(),
		PumpAmount);
}
