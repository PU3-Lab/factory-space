// Fill out your copyright notice in the Description page of Project Settings.


#include "AirCompressor.h"

#include "Wanted_Factory.h"
#include "OJJ_Grid.h"

namespace
{
	const FName GasFormName(TEXT("gas"));
}

// Sets default values
AAirCompressor::AAirCompressor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	MachineType = TEXT("AirCompressor");
	bNeedPower = true;
	bDisableWhenBroken = true;
}

// Called when the game starts or when spawned
void AAirCompressor::BeginPlay()
{
	Super::BeginPlay();
	
}

void AAirCompressor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopCompressing();
	LinkedResource = nullptr;

	Super::EndPlay(EndPlayReason);
}

bool AAirCompressor::CanPlaceAdditional(const AOJJ_Grid* Grid, FIntPoint Origin, int32 RotationSteps) const
{
	return FindAdjacentGas(Grid, Origin, RotationSteps) != nullptr;
}

void AAirCompressor::OnPlacedOnGrid(AOJJ_Grid* Grid, FIntPoint Origin, int32 RotationSteps)
{
	Super::OnPlacedOnGrid(Grid, Origin, RotationSteps);

	AResourceBase* Gas = FindAdjacentGas(Grid, Origin, RotationSteps);
	if (!Gas)
	{
		// LOG_SSR_W(TEXT("OnPlacedOnGrid: no adjacent gas resource. AirCompressor=%s"), *GetName());
		return;
	}

	SetLinkedResource(Gas);
	StartCompressing();
}

void AAirCompressor::OnRemovedFromGrid()
{
	Super::OnRemovedFromGrid();

	StopCompressing();
	LinkedResource = nullptr;
}

AResourceBase* AAirCompressor::FindAdjacentGas(const AOJJ_Grid* Grid, FIntPoint Origin, int32 RotationSteps) const
{
	if (!Grid)
	{
		return nullptr;
	}

	const FIntPoint Size = AOJJ_Grid::EffectiveSize(GetMachineSize(), RotationSteps);
	TSet<FIntPoint> Footprint;
	Footprint.Reserve(Size.X * Size.Y);
	for (int32 X = 0; X < Size.X; ++X)
	{
		for (int32 Y = 0; Y < Size.Y; ++Y)
		{
			Footprint.Add(Origin + FIntPoint(X, Y));
		}
	}

	static const FIntPoint Dirs[] = {
		FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1)
	};

	TSet<FIntPoint> Visited;
	for (const FIntPoint& Cell : Footprint)
	{
		for (const FIntPoint& Dir : Dirs)
		{
			const FIntPoint Neighbor = Cell + Dir;
			if (Footprint.Contains(Neighbor) || Visited.Contains(Neighbor))
			{
				continue;
			}
			Visited.Add(Neighbor);

			AResourceBase* Resource = Cast<AResourceBase>(Grid->GetActorAtCell(Neighbor));
			if (Resource && Resource->HasForm(GasFormName))
			{
				return Resource;
			}
		}
	}
	return nullptr;
}

// Called every frame
void AAirCompressor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AAirCompressor::SetLinkedResource(AResourceBase* NewResource)
{
	LinkedResource = NewResource;

	if (LinkedResource)
	{
		// LOG_SSR_W(TEXT("AirCompressor linked to resource: %s"), *LinkedResource->GetName());
	}
	else
	{
		// LOG_SSR_W(TEXT("AirCompressor linked resource is null."));
	}
}

bool AAirCompressor::CanCompress() const
{
	if (CompressAmount <= 0)
	{
		// LOG_SSR_W(TEXT("CanCompress failed: Invalid CompressAmount=%d."), CompressAmount);
		return false;
	}

	if (isBroken() && bDisableWhenBroken)
	{
		// LOG_SSR_W(TEXT("CanCompress failed: AirCompressor is broken."));
		return false;
	}

	if (MachineState == EMachineState::Disabled || MachineState == EMachineState::NoPower)
	{
		// LOG_SSR_W(TEXT("CanCompress failed: Machine is not available."));
		return false;
	}

	if (!HasEnoughPower())
	{
		// LOG_SSR_W(TEXT("CanCompress failed: Not enough power."));
		return false;
	}

	if (!LinkedResource)
	{
		// LOG_SSR_W(TEXT("CanCompress failed: LinkedResource is null."));
		return false;
	}

	FResourceData Data;
	if (!LinkedResource->GetResourceData(Data))
	{
		// LOG_SSR_W(
		// 	TEXT("CanCompress failed: ResourceData is invalid. Resource=%s RowName=%s"),
		// 	*LinkedResource->GetName(),
		// 	*LinkedResource->GetResourceRowName().ToString()
		// );
		return false;
	}

	if (Data.form != GasFormName)
	{
		// LOG_SSR_W(
		// 	TEXT("CanCompress failed: Resource form is not gas. Resource=%s RowName=%s Form=%s"),
		// 	*LinkedResource->GetName(),
		// 	*LinkedResource->GetResourceRowName().ToString(),
		// 	*Data.form.ToString()
		// );
		return false;
	}

	const FName ResourceID = LinkedResource->GetResourceRowName();
	if (ResourceID.IsNone())
	{
		// LOG_SSR_W(TEXT("CanCompress failed: Resource row name is None."));
		return false;
	}

	const int32 CurrentOutputCount = OutputBuffer.FindRef(ResourceID);
	if (CurrentOutputCount + CompressAmount > MaxBufferPerItem)
	{
		// LOG_SSR_W(
		// 	TEXT("CanCompress failed: Output buffer full. Item=%s Count=%d Max=%d"),
		// 	*ResourceID.ToString(),
		// 	CurrentOutputCount,
		// 	MaxBufferPerItem
		// );
		return false;
	}

	return true;
}

void AAirCompressor::StartCompressing()
{
	if (MachineState == EMachineState::Working || GetWorldTimerManager().IsTimerActive(CompressTimerHandle))
	{
		return;
	}

	if (CompressAmount <= 0 || CompressInterval <= 0.f)
	{
		// LOG_SSR_W(
		// 	TEXT("Cannot start compressing. Invalid compress settings. CompressAmount=%d CompressInterval=%.2f"),
		// 	CompressAmount,
		// 	CompressInterval
		// );
		return;
	}

	if (!CanCompress())
	{
		// LOG_SSR_W(TEXT("Cannot start compressing."));
		return;
	}

	MachineState = EMachineState::Working;
	GetWorldTimerManager().SetTimer(
		CompressTimerHandle,
		this,
		&AAirCompressor::CompressResource,
		GetEffectiveProcessTime(CompressInterval),
		true
	);

	// LOG_SSR_W(TEXT("Compressing started."));
}

void AAirCompressor::StopCompressing()
{
	GetWorldTimerManager().ClearTimer(CompressTimerHandle);
	StopProcess();

	// LOG_SSR_W(TEXT("Compressing stopped."));
}

void AAirCompressor::CompressResource()
{
	if (!CanCompress())
	{
		StopCompressing();
		return;
	}

	const FName ResourceID = LinkedResource->GetResourceRowName();
	if (!LinkedResource->ConsumeResource(CompressAmount))
	{
		// LOG_SSR_W(TEXT("Gas resource depleted."));
		StopCompressing();
		return;
	}

	AddOutputItem(ResourceID, CompressAmount);

	// LOG_SSR_W(TEXT("Compressed Gas Resource: %s x %d"),
	// 	*ResourceID.ToString(),
	// 	CompressAmount);
}

