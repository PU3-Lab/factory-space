// Fill out your copyright notice in the Description page of Project Settings.


#include "HydroPowerPlant.h"

#include "OJJ_Grid.h"
#include "Wanted_Factory.h"

namespace
{
	const FName HydroLiquidFormName(TEXT("liquid"));
}

AHydroPowerPlant::AHydroPowerPlant()
{
	MachineType = TEXT("HydroPowerPlant");
	BasePowerOutput = 10.0f;
}

bool AHydroPowerPlant::CanPlaceAdditional(const AOJJ_Grid* Grid, FIntPoint Origin, int32 RotationSteps) const
{
	return FindAdjacentWater(Grid, Origin, RotationSteps) != nullptr;
}

void AHydroPowerPlant::OnPlacedOnGrid(AOJJ_Grid* Grid, FIntPoint Origin, int32 RotationSteps)
{
	Super::OnPlacedOnGrid(Grid, Origin, RotationSteps);

	LinkedResource = FindAdjacentWater(Grid, Origin, RotationSteps);
	if (!LinkedResource)
	{
		// LOG_SSR_W(TEXT("OnPlacedOnGrid: no adjacent liquid water. HydroPowerPlant=%s"), *GetName());
	}
}

void AHydroPowerPlant::OnRemovedFromGrid()
{
	Super::OnRemovedFromGrid();

	LinkedResource = nullptr;
}

bool AHydroPowerPlant::CanGeneratePower() const
{
	return LinkedResource != nullptr && Super::CanGeneratePower();
}

AResourceBase* AHydroPowerPlant::FindAdjacentWater(const AOJJ_Grid* Grid, FIntPoint Origin, int32 RotationSteps) const
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
			if (Resource && Resource->HasForm(HydroLiquidFormName))
			{
				return Resource;
			}
		}
	}

	return nullptr;
}
