// Fill out your copyright notice in the Description page of Project Settings.

#include "Dummy_Grid.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "MachineBase.h"
#include "dummy_converyor.h"

namespace
{
constexpr float PortDotThreshold = 0.01f;

const FIntPoint NeighborSteps[] = {
	FIntPoint(1, 0),
	FIntPoint(-1, 0),
	FIntPoint(0, 1),
	FIntPoint(0, -1)
};

int32 ManhattanDistance(FIntPoint A, FIntPoint B)
{
	return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y);
}

AMachineBase* GetMachineAtCell(
	const TMap<FIntPoint, TWeakObjectPtr<AActor>>& OccupiedCells,
	FIntPoint Cell)
{
	const TWeakObjectPtr<AActor>* Found = OccupiedCells.Find(Cell);
	return Found && Found->IsValid() ? Cast<AMachineBase>(Found->Get()) : nullptr;
}

FIntPoint GetMachineBackStep(const AMachineBase* Machine)
{
	const FVector Forward = Machine ? Machine->GetActorForwardVector() : FVector::ForwardVector;
	if (FMath::Abs(Forward.X) >= FMath::Abs(Forward.Y))
	{
		return FIntPoint(Forward.X >= 0.f ? -1 : 1, 0);
	}

	return FIntPoint(0, Forward.Y >= 0.f ? -1 : 1);
}

FIntPoint GetMachineFrontStep(const AMachineBase* Machine)
{
	const FIntPoint BackStep = GetMachineBackStep(Machine);
	return FIntPoint(-BackStep.X, -BackStep.Y);
}

float GetMachineForwardDotToCell(const ADummyGrid* Grid, const AMachineBase* Machine, FIntPoint Cell)
{
	if (!Grid || !Machine)
	{
		return 0.f;
	}

	const FVector Forward3D = Machine->GetActorForwardVector();
	const FVector2D Forward(Forward3D.X, Forward3D.Y);
	if (Forward.IsNearlyZero())
	{
		return 0.f;
	}

	const FVector CellWorld = Grid->GridToWorld(Cell);
	const FVector MachineWorld = Machine->GetActorLocation();
	const FVector2D ToCell(CellWorld.X - MachineWorld.X, CellWorld.Y - MachineWorld.Y);
	if (ToCell.IsNearlyZero())
	{
		return 0.f;
	}

	return FVector2D::DotProduct(ToCell.GetSafeNormal(), Forward.GetSafeNormal());
}

bool IsBehindMachine(const ADummyGrid* Grid, const AMachineBase* Machine, FIntPoint Cell)
{
	return GetMachineForwardDotToCell(Grid, Machine, Cell) < -PortDotThreshold;
}

bool IsInFrontOfMachine(const ADummyGrid* Grid, const AMachineBase* Machine, FIntPoint Cell)
{
	return GetMachineForwardDotToCell(Grid, Machine, Cell) > PortDotThreshold;
}

bool IsMachineBackOutputPair(
	const ADummyGrid* Grid,
	const AMachineBase* Machine,
	FIntPoint MachineCell,
	FIntPoint ConveyorCell,
	const TArray<FIntPoint>& MachineCells)
{
	if (!MachineCells.Contains(MachineCell))
	{
		return false;
	}

	const FIntPoint BackStep = GetMachineBackStep(Machine);
	if (MachineCell + BackStep != ConveyorCell)
	{
		return false;
	}

	if (MachineCells.Contains(ConveyorCell) || !Grid->IsValidGridCell(ConveyorCell))
	{
		return false;
	}

	return IsBehindMachine(Grid, Machine, ConveyorCell);
}

bool IsMachineFrontInputPair(
	const ADummyGrid* Grid,
	const AMachineBase* Machine,
	FIntPoint MachineCell,
	FIntPoint ConveyorCell,
	const TArray<FIntPoint>& MachineCells)
{
	if (!MachineCells.Contains(MachineCell))
	{
		return false;
	}

	const FIntPoint FrontStep = GetMachineFrontStep(Machine);
	if (MachineCell + FrontStep != ConveyorCell)
	{
		return false;
	}

	if (MachineCells.Contains(ConveyorCell) || !Grid->IsValidGridCell(ConveyorCell))
	{
		return false;
	}

	return IsInFrontOfMachine(Grid, Machine, ConveyorCell);
}

bool FindInputMachineAtPathEnd(
	const ADummyGrid* Grid,
	const TMap<FIntPoint, TWeakObjectPtr<AActor>>& OccupiedCells,
	const TMap<TWeakObjectPtr<AActor>, TArray<FIntPoint>>& ActorToCells,
	const TArray<FIntPoint>& PathCells,
	AMachineBase* StartMachine,
	AMachineBase*& OutEndMachine,
	bool& bOutEndsOnMachine,
	FString& OutReason)
{
	OutEndMachine = nullptr;
	bOutEndsOnMachine = false;

	if (PathCells.Num() < 2)
	{
		OutReason = TEXT("Conveyor path must reach another machine input port.");
		return false;
	}

	const FIntPoint EndCell = PathCells.Last();
	const FIntPoint PreviousCell = PathCells[PathCells.Num() - 2];

	const TWeakObjectPtr<AActor>* EndOccupant = OccupiedCells.Find(EndCell);
	if (EndOccupant && EndOccupant->IsValid())
	{
		AMachineBase* EndMachine = Cast<AMachineBase>(EndOccupant->Get());
		const TArray<FIntPoint>* EndMachineCells = EndMachine ? ActorToCells.Find(EndMachine) : nullptr;
		if (!EndMachine || EndMachine == StartMachine || !EndMachineCells
			|| !IsMachineFrontInputPair(Grid, EndMachine, EndCell, PreviousCell, *EndMachineCells))
		{
			OutReason = TEXT("Conveyor must end at another machine input port.");
			return false;
		}

		const TWeakObjectPtr<AActor>* PreviousOccupant = OccupiedCells.Find(PreviousCell);
		if (PreviousOccupant && PreviousOccupant->IsValid())
		{
			OutReason = TEXT("The cell before a machine input port must be empty.");
			return false;
		}

		OutEndMachine = EndMachine;
		bOutEndsOnMachine = true;
		return true;
	}

	bool bSawAdjacentMachine = false;
	for (const FIntPoint& Step : NeighborSteps)
	{
		const FIntPoint MachineCell = EndCell - Step;
		AMachineBase* AdjacentMachine = GetMachineAtCell(OccupiedCells, MachineCell);
		if (!AdjacentMachine || AdjacentMachine == StartMachine)
		{
			continue;
		}

		bSawAdjacentMachine = true;
		const TArray<FIntPoint>* MachineCells = ActorToCells.Find(AdjacentMachine);
		if (MachineCells && IsMachineFrontInputPair(Grid, AdjacentMachine, MachineCell, EndCell, *MachineCells))
		{
			OutEndMachine = AdjacentMachine;
			return true;
		}
	}

	OutReason = bSawAdjacentMachine
		? TEXT("Conveyor end is near a machine, but not at its input side.")
		: TEXT("Conveyor must end at or in front of another machine input port.");
	return false;
}

bool CollectConveyorReservedCells(
	const ADummyGrid* Grid,
	const TMap<FIntPoint, TWeakObjectPtr<AActor>>& OccupiedCells,
	const TMap<TWeakObjectPtr<AActor>, TArray<FIntPoint>>& ActorToCells,
	const TArray<FIntPoint>& PathCells,
	TArray<FIntPoint>& OutReservedCells,
	FString& OutReason)
{
	OutReservedCells.Reset();

	if (PathCells.Num() < 2)
	{
		OutReason = TEXT("Conveyor path must include the machine output and at least one outside cell.");
		return false;
	}

	AMachineBase* StartMachine = GetMachineAtCell(OccupiedCells, PathCells[0]);
	const TArray<FIntPoint>* StartMachineCells = StartMachine ? ActorToCells.Find(StartMachine) : nullptr;
	if (!StartMachine || !StartMachineCells
		|| !IsMachineBackOutputPair(Grid, StartMachine, PathCells[0], PathCells[1], *StartMachineCells))
	{
		OutReason = TEXT("Conveyor must start from a machine output port.");
		return false;
	}

	AMachineBase* EndMachine = nullptr;
	bool bEndsOnMachine = false;
	if (!FindInputMachineAtPathEnd(
		Grid,
		OccupiedCells,
		ActorToCells,
		PathCells,
		StartMachine,
		EndMachine,
		bEndsOnMachine,
		OutReason))
	{
		return false;
	}

	for (int32 Index = 0; Index < PathCells.Num(); ++Index)
	{
		const FIntPoint Cell = PathCells[Index];
		if (!Grid->IsValidGridCell(Cell))
		{
			OutReason = TEXT("Conveyor path is outside the grid.");
			return false;
		}

		if (Index > 0 && ManhattanDistance(PathCells[Index - 1], Cell) != 1)
		{
			OutReason = TEXT("Conveyor path must be contiguous.");
			return false;
		}

		const TWeakObjectPtr<AActor>* Occupant = OccupiedCells.Find(Cell);
		if (Occupant && Occupant->IsValid())
		{
			const bool bAllowedOutputCell = Index == 0 && Occupant->Get() == StartMachine;
			const bool bAllowedInputCell = bEndsOnMachine
				&& Index == PathCells.Num() - 1
				&& Occupant->Get() == EndMachine;
			if (!bAllowedOutputCell && !bAllowedInputCell)
			{
				OutReason = TEXT("Conveyor path is blocked by an occupied cell.");
				return false;
			}
			continue;
		}

		OutReservedCells.AddUnique(Cell);
	}

	OutReason.Reset();
	return true;
}
}

bool ADummyGrid::BuildConveyorPlacementPath(
	const TArray<FIntPoint>& DragCells,
	TArray<FIntPoint>& OutPathCells,
	FString& OutReason) const
{
	OutPathCells.Reset();
	if (DragCells.Num() == 0)
	{
		OutReason = TEXT("Conveyor drag path is empty.");
		return false;
	}

	const FIntPoint StartCell = DragCells[0];
	if (!IsValidGridCell(StartCell))
	{
		OutReason = TEXT("Conveyor start cell is outside the grid.");
		return false;
	}

	if (AMachineBase* StartMachine = GetMachineAtCell(OccupiedCells, StartCell))
	{
		const TArray<FIntPoint>* MachineCells = ActorToCells.Find(StartMachine);
		const FIntPoint OutsideCell = StartCell + GetMachineBackStep(StartMachine);
		if (!MachineCells || !IsMachineBackOutputPair(this, StartMachine, StartCell, OutsideCell, *MachineCells))
		{
			OutReason = TEXT("Conveyor on a machine must be placed on the back outer output cell.");
			return false;
		}

		OutPathCells = DragCells;
		if (OutPathCells.Num() == 1)
		{
			OutPathCells.Add(OutsideCell);
		}
		else if (OutPathCells[1] != OutsideCell)
		{
			OutReason = TEXT("Conveyor must leave the machine through its back output cell.");
			return false;
		}
	}
	else
	{
		bool bSawAdjacentMachine = false;
		for (const FIntPoint& Step : NeighborSteps)
		{
			const FIntPoint MachineCell = StartCell - Step;
			AMachineBase* AdjacentMachine = GetMachineAtCell(OccupiedCells, MachineCell);
			if (!AdjacentMachine)
			{
				continue;
			}

			bSawAdjacentMachine = true;
			const TArray<FIntPoint>* MachineCells = ActorToCells.Find(AdjacentMachine);
			if (MachineCells
				&& IsMachineBackOutputPair(this, AdjacentMachine, MachineCell, StartCell, *MachineCells))
			{
				OutPathCells = DragCells;
				OutPathCells.Insert(MachineCell, 0);
				break;
			}
		}

		if (OutPathCells.Num() == 0)
		{
			OutReason = bSawAdjacentMachine
				? TEXT("Adjacent machine cell is not its back output port.")
				: TEXT("Conveyor must start on or next to a machine output port.");
			return false;
		}
	}

	TArray<FIntPoint> ReservedCells;
	return CollectConveyorReservedCells(this, OccupiedCells, ActorToCells, OutPathCells, ReservedCells, OutReason);
}

bool ADummyGrid::CanPlaceConveyorPath(const TArray<FIntPoint>& PathCells) const
{
	TArray<FIntPoint> ReservedCells;
	FString OutReason;
	return CollectConveyorReservedCells(this, OccupiedCells, ActorToCells, PathCells, ReservedCells, OutReason);
}

bool ADummyGrid::TryPlaceConveyor(ADummyConveyor* Conveyor, const TArray<FIntPoint>& PathCells, FString& OutReason)
{
	TArray<FIntPoint> PlacementCells;
	if (!BuildConveyorPlacementPath(PathCells, PlacementCells, OutReason))
	{
		return false;
	}

	TArray<FIntPoint> ReservedCells;
	if (!CollectConveyorReservedCells(this, OccupiedCells, ActorToCells, PlacementCells, ReservedCells, OutReason))
	{
		return false;
	}

	if (!RegisterActorCells(Conveyor, ReservedCells, OutReason))
	{
		return false;
	}

	Conveyor->SetActorLocation(GetActorLocation());
	Conveyor->SetPath(PlacementCells, CellSize);
	return true;
}

void ADummyGrid::UpdateConveyorPathHoverPreview(const TArray<FIntPoint>& PathCells)
{
	ClearHoverPreview();

	if (PathCells.Num() == 0)
	{
		return;
	}

	TArray<FIntPoint> PreviewCells;
	FString OutReason;
	const bool bCanPlace = BuildConveyorPlacementPath(PathCells, PreviewCells, OutReason);
	if (!bCanPlace)
	{
		PreviewCells = PathCells;
	}

	UInstancedStaticMeshComponent* TargetISM = bCanPlace ? ValidHoverISM.Get() : InvalidHoverISM.Get();
	if (!TargetISM)
	{
		return;
	}

	for (const FIntPoint& Cell : PreviewCells)
	{
		const FVector CellCenter = GridToWorld(Cell);
		const FVector InstanceLocation(CellCenter.X, CellCenter.Y, CellCenter.Z + 2.0f);
		const FVector InstanceScale(CellSize / 100.0f, CellSize / 100.0f, 1.0f);
		const FTransform InstanceTransform(FRotator::ZeroRotator, InstanceLocation, InstanceScale);
		TargetISM->AddInstance(InstanceTransform, /*bWorldSpace=*/true);
	}
}
