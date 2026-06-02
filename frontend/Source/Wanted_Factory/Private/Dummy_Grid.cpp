// Fill out your copyright notice in the Description page of Project Settings.

#include "Dummy_Grid.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Conveyor.h"
#include "Engine/StaticMesh.h"
#include "MachineBase.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ADummyGrid::ADummyGrid()
{
	PrimaryActorTick.bCanEverTick = true;
	CellSize = 100.0f;
	VisualizationRange = 20;

	USceneComponent* GridRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GridRoot"));
	RootComponent = GridRoot;

	GridFloorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GridFloorMesh"));
	GridFloorMesh->SetupAttachment(RootComponent);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		GridFloorMesh->SetStaticMesh(PlaneMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GridMaterial(
		TEXT("/Game/OJJ/Materials/M_OJJ_GridFloor.M_OJJ_GridFloor"));
	if (GridMaterial.Succeeded())
	{
		GridFloorMesh->SetMaterial(0, GridMaterial.Object);
	}

	GridFloorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GridFloorMesh->SetVisibility(false);

	ValidHoverISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ValidHoverISM"));
	ValidHoverISM->SetupAttachment(RootComponent);
	ValidHoverISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ValidHoverISM->SetCastShadow(false);

	InvalidHoverISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("InvalidHoverISM"));
	InvalidHoverISM->SetupAttachment(RootComponent);
	InvalidHoverISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InvalidHoverISM->SetCastShadow(false);

	if (PlaneMesh.Succeeded())
	{
		ValidHoverISM->SetStaticMesh(PlaneMesh.Object);
		InvalidHoverISM->SetStaticMesh(PlaneMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ValidHoverMat(
		TEXT("/Game/OJJ/Materials/MI_OJJ_GridHoverValid.MI_OJJ_GridHoverValid"));
	if (ValidHoverMat.Succeeded())
	{
		ValidHoverISM->SetMaterial(0, ValidHoverMat.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> InvalidHoverMat(
		TEXT("/Game/OJJ/Materials/MI_OJJ_GridHoverInvalid.MI_OJJ_GridHoverInvalid"));
	if (InvalidHoverMat.Succeeded())
	{
		InvalidHoverISM->SetMaterial(0, InvalidHoverMat.Object);
	}
}

void ADummyGrid::BeginPlay()
{
	Super::BeginPlay();

	if (GridFloorMesh)
	{
		const float ScaleFactor = (VisualizationRange * CellSize) / 100.0f;
		GridFloorMesh->SetRelativeScale3D(FVector(ScaleFactor, ScaleFactor, 1.0f));

		const float OffsetXY = (VisualizationRange * CellSize) / 2.0f;
		GridFloorMesh->SetRelativeLocation(FVector(OffsetXY, OffsetXY, 1.0f));
	}
}

void ADummyGrid::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

FIntPoint ADummyGrid::WorldToGrid(FVector WorldPos) const
{
	const FVector Local = WorldPos - GetActorLocation();
	const int32 X = FMath::FloorToInt(Local.X / CellSize);
	const int32 Y = FMath::FloorToInt(Local.Y / CellSize);
	return FIntPoint(X, Y);
}

FVector ADummyGrid::GridToWorld(FIntPoint Coord) const
{
	const FVector Origin = GetActorLocation();
	const float WorldX = Origin.X + (Coord.X * CellSize) + (CellSize * 0.5f);
	const float WorldY = Origin.Y + (Coord.Y * CellSize) + (CellSize * 0.5f);
	return FVector(WorldX, WorldY, Origin.Z);
}

FVector ADummyGrid::GetGridCenter() const
{
	const FVector Origin = GetActorLocation();
	const float CenterX = Origin.X + (GridSize.X * CellSize * 0.5f);
	const float CenterY = Origin.Y + (GridSize.Y * CellSize * 0.5f);
	return FVector(CenterX, CenterY, Origin.Z);
}

FIntPoint ADummyGrid::EffectiveSize(FVector2D RawSize, int32 RotationSteps)
{
	const int32 X = FMath::Max(1, FMath::CeilToInt(RawSize.X));
	const int32 Y = FMath::Max(1, FMath::CeilToInt(RawSize.Y));
	return ((RotationSteps % 2) != 0) ? FIntPoint(Y, X) : FIntPoint(X, Y);
}

FVector ADummyGrid::GetMachinePlacementLocation(AMachineBase* Machine, FIntPoint Origin, int32 RotationSteps) const
{
	if (!Machine)
	{
		return GridToWorld(Origin);
	}

	const FIntPoint Size = EffectiveSize(Machine->GetMachineSize(), RotationSteps);
	const FVector LowerLeftCenter = GridToWorld(Origin);
	const float OffsetX = (Size.X - 1) * CellSize * 0.5f;
	const float OffsetY = (Size.Y - 1) * CellSize * 0.5f;
	return FVector(LowerLeftCenter.X + OffsetX, LowerLeftCenter.Y + OffsetY, LowerLeftCenter.Z);
}

bool ADummyGrid::IsValidGridCell(FIntPoint Cell) const
{
	return Cell.X >= 0 && Cell.X < GridSize.X
		&& Cell.Y >= 0 && Cell.Y < GridSize.Y;
}

TArray<FIntPoint> ADummyGrid::CalculateFootprint(AMachineBase* Machine, FIntPoint Origin, int32 RotationSteps) const
{
	TArray<FIntPoint> Cells;
	if (!Machine)
	{
		return Cells;
	}

	const FIntPoint Size = EffectiveSize(Machine->GetMachineSize(), RotationSteps);

	Cells.Reserve(Size.X * Size.Y);
	for (int32 X = 0; X < Size.X; ++X)
	{
		for (int32 Y = 0; Y < Size.Y; ++Y)
		{
			Cells.Add(Origin + FIntPoint(X, Y));
		}
	}
	return Cells;
}

bool ADummyGrid::CanPlaceCells(const TArray<FIntPoint>& Cells) const
{
	if (Cells.Num() == 0)
	{
		return false;
	}

	for (const FIntPoint& Cell : Cells)
	{
		if (!IsValidGridCell(Cell))
		{
			return false;
		}

		const TWeakObjectPtr<AActor>* Found = OccupiedCells.Find(Cell);
		if (Found && Found->IsValid())
		{
			return false;
		}
	}

	return true;
}

bool ADummyGrid::CanPlaceMachine(AMachineBase* Machine, FIntPoint Origin, int32 RotationSteps) const
{
	if (!Machine)
	{
		return false;
	}

	return CanPlaceCells(CalculateFootprint(Machine, Origin, RotationSteps));
}

void ADummyGrid::SweepStaleEntries()
{
	for (auto It = ActorToCells.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			for (const FIntPoint& Cell : It.Value())
			{
				const TWeakObjectPtr<AActor>* Found = OccupiedCells.Find(Cell);
				if (Found && !Found->IsValid())
				{
					OccupiedCells.Remove(Cell);
				}
			}
			It.RemoveCurrent();
		}
	}
}

bool ADummyGrid::RegisterActorCells(AActor* Actor, const TArray<FIntPoint>& Cells, FString& OutReason)
{
	if (!HasAuthority())
	{
		ensureMsgf(false, TEXT("Grid placement called on non-authority"));
		OutReason = TEXT("Not authority");
		return false;
	}

	SweepStaleEntries();

	if (!Actor)
	{
		OutReason = TEXT("Invalid actor");
		return false;
	}

	if (ActorToCells.Contains(Actor))
	{
		OutReason = TEXT("Actor already placed.");
		return false;
	}

	if (!CanPlaceCells(Cells))
	{
		OutReason = TEXT("Cell already occupied");
		return false;
	}

	TArray<FIntPoint> UniqueCells;
	UniqueCells.Reserve(Cells.Num());
	for (const FIntPoint& Cell : Cells)
	{
		UniqueCells.AddUnique(Cell);
	}

	for (const FIntPoint& Cell : UniqueCells)
	{
		OccupiedCells.Add(Cell, Actor);
	}
	ActorToCells.Add(Actor, MoveTemp(UniqueCells));

	OutReason.Reset();
	return true;
}

bool ADummyGrid::TryPlaceMachine(AMachineBase* Machine, FIntPoint Origin, FString& OutReason, int32 RotationSteps)
{
	if (!RegisterActorCells(Machine, CalculateFootprint(Machine, Origin, RotationSteps), OutReason))
	{
		return false;
	}

	if (!Machine->SetActorLocation(GetMachinePlacementLocation(Machine, Origin, RotationSteps)))
	{
		RemoveMachine(Machine);
		OutReason = TEXT("Failed to move machine to target location");
		return false;
	}

	return true;
}

bool ADummyGrid::RegisterExistingMachine(AMachineBase* Machine, FIntPoint Origin, FString& OutReason)
{
	if (Machine)
	{
		const FVector Expected = GetMachinePlacementLocation(Machine, Origin);
		const FVector Actual = Machine->GetActorLocation();
		const float DistXY = FVector2D(Expected.X - Actual.X, Expected.Y - Actual.Y).Size();
		const float Tolerance = 1.0f;

		if (DistXY > Tolerance)
		{
			OutReason = FString::Printf(
				TEXT("Pre-placed machine center anchor mismatch. Expected XY=(%.1f,%.1f), Actual XY=(%.1f,%.1f), Dist=%.2f, Tolerance=%.2f."),
				Expected.X, Expected.Y, Actual.X, Actual.Y, DistXY, Tolerance);
			ensureMsgf(false, TEXT("[DummyGrid] %s"), *OutReason);
			UE_LOG(LogTemp, Error, TEXT("[DummyGrid] RegisterExistingMachine refused: %s"), *OutReason);
			return false;
		}
	}

	return RegisterActorCells(Machine, CalculateFootprint(Machine, Origin), OutReason);
}

void ADummyGrid::SetVisualizationVisible(bool bVisible)
{
	if (!GridFloorMesh)
	{
		return;
	}

	GridFloorMesh->SetVisibility(bVisible);

	if (bVisible)
	{
		GridFloorMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		GridFloorMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		GridFloorMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}
	else
	{
		GridFloorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ADummyGrid::UpdateHoverPreview(AMachineBase* Machine, FIntPoint Origin, int32 RotationSteps)
{
	if (!Machine)
	{
		ClearHoverPreview();
		return;
	}

	UpdatePathHoverPreview(CalculateFootprint(Machine, Origin, RotationSteps));
}

void ADummyGrid::UpdatePathHoverPreview(const TArray<FIntPoint>& PathCells)
{
	ClearHoverPreview();

	if (PathCells.Num() == 0)
	{
		return;
	}

	const bool bCanPlace = CanPlaceCells(PathCells);
	UInstancedStaticMeshComponent* TargetISM = bCanPlace ? ValidHoverISM.Get() : InvalidHoverISM.Get();
	if (!TargetISM)
	{
		return;
	}

	for (const FIntPoint& Cell : PathCells)
	{
		const FVector CellCenter = GridToWorld(Cell);
		const FVector InstanceLocation(CellCenter.X, CellCenter.Y, CellCenter.Z + 2.0f);
		const FVector InstanceScale(CellSize / 100.0f, CellSize / 100.0f, 1.0f);
		const FTransform InstanceTransform(FRotator::ZeroRotator, InstanceLocation, InstanceScale);
		TargetISM->AddInstance(InstanceTransform, /*bWorldSpace=*/true);
	}
}

void ADummyGrid::ClearHoverPreview()
{
	if (ValidHoverISM)
	{
		ValidHoverISM->ClearInstances();
	}
	if (InvalidHoverISM)
	{
		InvalidHoverISM->ClearInstances();
	}
}

bool ADummyGrid::RemoveActor(AActor* Actor)
{
	if (!HasAuthority())
	{
		ensureMsgf(false, TEXT("RemoveActor called on non-authority"));
		return false;
	}

	if (!Actor)
	{
		return false;
	}

	const TArray<FIntPoint>* Cells = ActorToCells.Find(Actor);
	if (!Cells)
	{
		return false;
	}

	for (const FIntPoint& Cell : *Cells)
	{
		OccupiedCells.Remove(Cell);
	}
	ActorToCells.Remove(Actor);
	return true;
}

bool ADummyGrid::RemoveMachine(AMachineBase* Machine)
{
	return RemoveActor(Machine);
}

bool ADummyGrid::RemoveActorAt(FIntPoint Coord)
{
	if (!HasAuthority())
	{
		ensureMsgf(false, TEXT("RemoveActorAt called on non-authority"));
		return false;
	}

	const TWeakObjectPtr<AActor>* Found = OccupiedCells.Find(Coord);
	if (!Found || !Found->IsValid())
	{
		return false;
	}

	return RemoveActor(Found->Get());
}
