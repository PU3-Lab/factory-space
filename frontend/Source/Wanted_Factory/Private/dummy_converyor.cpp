// Fill out your copyright notice in the Description page of Project Settings.

#include "dummy_converyor.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
FIntPoint GetStepDirection(FIntPoint From, FIntPoint To)
{
	const int32 DeltaX = To.X - From.X;
	const int32 DeltaY = To.Y - From.Y;

	if (FMath::Abs(DeltaX) >= FMath::Abs(DeltaY) && DeltaX != 0)
	{
		return FIntPoint(FMath::Clamp(DeltaX, -1, 1), 0);
	}

	if (DeltaY != 0)
	{
		return FIntPoint(0, FMath::Clamp(DeltaY, -1, 1));
	}

	return FIntPoint::ZeroValue;
}

float DirectionToYaw(FIntPoint Direction)
{
	if (Direction.X > 0)
	{
		return 0.0f;
	}
	if (Direction.X < 0)
	{
		return 180.0f;
	}
	if (Direction.Y > 0)
	{
		return 90.0f;
	}
	if (Direction.Y < 0)
	{
		return -90.0f;
	}

	return 0.0f;
}

float CornerToYaw(FIntPoint PreviousDirection, FIntPoint NextDirection)
{
	const FVector2D CornerDirection(
		static_cast<float>(PreviousDirection.X + NextDirection.X),
		static_cast<float>(PreviousDirection.Y + NextDirection.Y));

	if (CornerDirection.IsNearlyZero())
	{
		return DirectionToYaw(NextDirection);
	}

	return FMath::RadiansToDegrees(FMath::Atan2(CornerDirection.Y, CornerDirection.X));
}
}

ADummyConveyor::ADummyConveyor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	StraightSegmentInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("StraightSegmentInstances"));
	StraightSegmentInstances->SetupAttachment(Root);
	StraightSegmentInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	CornerSegmentInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CornerSegmentInstances"));
	CornerSegmentInstances->SetupAttachment(Root);
	CornerSegmentInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		StraightSegmentInstances->SetStaticMesh(CubeMesh.Object);
		CornerSegmentInstances->SetStaticMesh(CubeMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialAsset(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MaterialAsset.Succeeded())
	{
		StraightSegmentInstances->SetMaterial(0, MaterialAsset.Object);
		CornerSegmentInstances->SetMaterial(0, MaterialAsset.Object);
	}
}

void ADummyConveyor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	RebuildVisuals();
}

void ADummyConveyor::SetPath(const TArray<FIntPoint>& NewPathCells, float NewCellSize)
{
	CellSize = FMath::Max(1.0f, NewCellSize);
	PathCells.Reset(NewPathCells.Num());

	for (const FIntPoint& Cell : NewPathCells)
	{
		if (PathCells.Num() == 0 || PathCells.Last() != Cell)
		{
			PathCells.Add(Cell);
		}
	}

	RebuildVisuals();
}

void ADummyConveyor::ClearPath()
{
	PathCells.Reset();
	RebuildVisuals();
}

void ADummyConveyor::RebuildVisuals()
{
	if (StraightSegmentInstances)
	{
		StraightSegmentInstances->ClearInstances();
	}
	if (CornerSegmentInstances)
	{
		CornerSegmentInstances->ClearInstances();
	}

	if (!StraightSegmentInstances || !CornerSegmentInstances || PathCells.Num() == 0)
	{
		return;
	}

	const float Width = CellSize * FMath::Clamp(SegmentWidthRatio, 0.0f, 1.0f);
	const FVector StraightScaleX(CellSize / 100.0f, Width / 100.0f, SegmentHeight / 100.0f);
	const FVector StraightScaleY(Width / 100.0f, CellSize / 100.0f, SegmentHeight / 100.0f);
	const FVector CornerScale(Width / 100.0f, Width / 100.0f, SegmentHeight / 100.0f);

	for (int32 Index = 0; Index < PathCells.Num(); ++Index)
	{
		const FIntPoint CurrentCell = PathCells[Index];
		const FIntPoint PreviousDirection = Index > 0
			? GetStepDirection(PathCells[Index - 1], CurrentCell)
			: FIntPoint::ZeroValue;
		const FIntPoint NextDirection = Index + 1 < PathCells.Num()
			? GetStepDirection(CurrentCell, PathCells[Index + 1])
			: FIntPoint::ZeroValue;

		const bool bHasPrevious = PreviousDirection != FIntPoint::ZeroValue;
		const bool bHasNext = NextDirection != FIntPoint::ZeroValue;
		const bool bIsCorner = bHasPrevious && bHasNext && PreviousDirection != NextDirection;
		const FIntPoint VisualDirection = bHasNext ? NextDirection : PreviousDirection;

		const FVector LocalLocation(
			(CurrentCell.X * CellSize) + (CellSize * 0.5f),
			(CurrentCell.Y * CellSize) + (CellSize * 0.5f),
			ZOffset);

		if (bIsCorner)
		{
			const FRotator Rotation(0.0f, CornerToYaw(PreviousDirection, NextDirection), 0.0f);
			CornerSegmentInstances->AddInstance(FTransform(Rotation, LocalLocation, CornerScale));
			continue;
		}

		const bool bHorizontal = VisualDirection.X != 0;
		const FRotator Rotation(0.0f, DirectionToYaw(VisualDirection), 0.0f);
		const FVector Scale = bHorizontal ? StraightScaleX : StraightScaleY;
		StraightSegmentInstances->AddInstance(FTransform(Rotation, LocalLocation, Scale));
	}
}
