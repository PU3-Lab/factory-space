// Fill out your copyright notice in the Description page of Project Settings.


#include "MinerMachine.h"

#include "Wanted_Factory.h"


// Sets default values
AMinerMachine::AMinerMachine()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
}

// Called when the game starts or when spawned
void AMinerMachine::BeginPlay()
{
	Super::BeginPlay();
	
}

void AMinerMachine::SetLinkedResource(AResourceBase* NewResource)
{
	LinkedResource = NewResource;

	if (LinkedResource)
	{
		LOG_SSR_W(TEXT("Miner linked to resource: %s"), *LinkedResource->GetName());
	}
	else
	{
		LOG_SSR_W(TEXT("Miner linked resource is null."));
	}
}

bool AMinerMachine::CanMine() const
{
	if (!LinkedResource)
	{
		LOG_SSR_W(TEXT("CanMine failed: LinkedResource is null."));
		return false;
	}
	
	FResourceData Data;
	if (!LinkedResource->GetResourceData(Data))
	{
		LOG_SSR_W(
			TEXT("CanMine failed: ResourceData is invalid. Resource=%s RowName=%s"),
			*LinkedResource->GetName(),
			*LinkedResource->GetResourceRowName().ToString()
		);
		return false;
	}

	if (Data.shape != EResourceShape::Ore)
	{
		LOG_SSR_W(
			TEXT("CanMine failed: Resource shape is not Ore. Resource=%s RowName=%s Shape=%s"),
			*LinkedResource->GetName(),
			*LinkedResource->GetResourceRowName().ToString(),
			*UEnum::GetValueAsString(Data.shape)
		);
		return false;
	}

	return true;
}

void AMinerMachine::StartMining()
{
	if (MineAmount <= 0 || MineInterval <= 0.f)
	{
		LOG_SSR_W(
			TEXT("Cannot start mining. Invalid mining settings. MineAmount=%d MineInterval=%.2f"),
			MineAmount,
			MineInterval
		);
		return;
	}

	if (!CanMine())
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot start mining."));
		return;
	}

	GetWorldTimerManager().SetTimer(
		MineTimerHandle,
		this,
		&AMinerMachine::MineResource,
		MineInterval,
		true
	);

	UE_LOG(LogTemp, Warning, TEXT("Mining started."));
}

void AMinerMachine::StopMining()
{
	GetWorldTimerManager().ClearTimer(MineTimerHandle);
	
	LOG_SSR_W(TEXT("Mining stopped."));
}

void AMinerMachine::MineResource()
{
	if (!CanMine())
	{
		StopMining();
		return;
	}
	
	// RowName이 아이템 ID 역할
	const FName ResourceID =
		LinkedResource->GetResourceRowName();

	if (ResourceID.IsNone())
	{
		LOG_SSR_W(TEXT("MineResource failed: Resource row name is None."));
		StopMining();
		return;
	}

	const int32 CurrentOutputCount = OutputBuffer.FindRef(ResourceID);

	if (CurrentOutputCount + MineAmount > MaxBufferPerItem)
	{
		LOG_SSR_W(
			TEXT("MineResource failed: Output buffer full. Item=%s Count=%d Max=%d"),
			*ResourceID.ToString(),
			CurrentOutputCount,
			MaxBufferPerItem
		);
		StopMining();
		return;
	}

	if (!LinkedResource->ConsumeResource(MineAmount))
	{
		LOG_SSR_W(TEXT("Resource depleted"));
		StopMining();
		return;
	}
	
	AddOutputItem(ResourceID, MineAmount);

	UE_LOG(LogTemp, Warning, TEXT("Mined Resource: %s x %d"),
		*ResourceID.ToString(),
		MineAmount);
	
}



