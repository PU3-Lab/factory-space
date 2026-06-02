// Fill out your copyright notice in the Description page of Project Settings.

#include "Conveyor.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Dummy_MachineBase.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
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

AConveyor::AConveyor()
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

	DebugStateText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("DebugStateText"));
	DebugStateText->SetupAttachment(Root);
	DebugStateText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DebugStateText->SetHorizontalAlignment(EHTA_Center);
	DebugStateText->SetVerticalAlignment(EVRTA_TextCenter);
	DebugStateText->SetWorldSize(DebugTextWorldSize);
	DebugStateText->SetRelativeRotation(FRotator(60.0f, 0.0f, 0.0f));

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

void AConveyor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	RebuildVisuals();
	UpdateDebugStateText();
}

void AConveyor::BeginPlay()
{
	Super::BeginPlay();

	RestartItemMoveTimer();
}

void AConveyor::SetPath(const TArray<FIntPoint>& NewPathCells, float NewCellSize)
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
	UpdateDebugStateText();
}

void AConveyor::ConfigureTransport(
	const TArray<FIntPoint>& NewOccupiedGridCells,
	ADummyMachineBase* NewSourceMachine,
	ADummyMachineBase* NewTargetMachine)
{
	OccupiedGridCells.Reset(NewOccupiedGridCells.Num());
	for (const FIntPoint& Cell : NewOccupiedGridCells)
	{
		OccupiedGridCells.AddUnique(Cell);
	}

	SourceMachine = NewSourceMachine;
	TargetMachine = NewTargetMachine;
	ResetItemSlots();
	RestartItemMoveTimer();
	UpdateDebugStateText();

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Conveyor] Occupied grid count: %d, travel time: %.2f sec"),
		GetOccupiedGridCount(),
		GetTravelTimePerItem());
}

void AConveyor::ClearPath()
{
	PathCells.Reset();
	OccupiedGridCells.Reset();
	SourceMachine.Reset();
	TargetMachine.Reset();
	ItemSlots.Reset();
	StopItemMoveTimer();
	RebuildVisuals();
	UpdateDebugStateText();
}

bool AConveyor::IsOutputBlocked() const
{
	if (ItemSlots.Num() == 0)
	{
		return false;
	}

	const FName LastItem = ItemSlots.Last();
	return !LastItem.IsNone()
		&& (!TargetMachine.IsValid() || !TargetMachine->CanReceiveConveyorItem(LastItem, 1));
}

void AConveyor::UpdateDebugStateText()
{
	if (!DebugStateText)
	{
		return;
	}

	DebugStateText->SetVisibility(bShowDebugStateText);
	DebugStateText->SetWorldSize(DebugTextWorldSize);
	DebugStateText->SetRelativeLocation(GetDebugTextLocalLocation());
	if (!bShowDebugStateText)
	{
		return;
	}

	bool bSlotsFull = ItemSlots.Num() > 0;
	for (const FName& Item : ItemSlots)
	{
		if (Item.IsNone())
		{
			bSlotsFull = false;
			break;
		}
	}

	const FString MovingItemSummary = BuildMovingItemSummary();
	const bool bHasMovingItems = !MovingItemSummary.Equals(TEXT("None"));
	const bool bFlowBlocked = IsOutputBlocked() && bSlotsFull;
	const TCHAR* StatusText = bFlowBlocked
		? TEXT("blocked")
		: (bHasMovingItems ? TEXT("moving") : TEXT("idle"));

	const FString DebugText = FString::Printf(
		TEXT("Conveyor\nGrids: %d\nTravel: %.2fs\nStatus: %s\nItems\n%s"),
		GetOccupiedGridCount(),
		GetTravelTimePerItem(),
		StatusText,
		*MovingItemSummary);
	DebugStateText->SetText(FText::FromString(DebugText));
}

void AConveyor::RebuildVisuals()
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

void AConveyor::ResetItemSlots()
{
	ItemSlots.SetNum(OccupiedGridCells.Num());
	for (FName& ItemSlot : ItemSlots)
	{
		ItemSlot = NAME_None;
	}
}

void AConveyor::RestartItemMoveTimer()
{
	StopItemMoveTimer();

	if (!bAutoMoveItems || ItemSlots.Num() == 0 || !SourceMachine.IsValid() || !TargetMachine.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		ItemMoveTimerHandle,
		this,
		&AConveyor::MoveItemsOneGrid,
		FMath::Max(0.01f, SecondsPerGrid),
		true);
}

void AConveyor::StopItemMoveTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ItemMoveTimerHandle);
	}
}

void AConveyor::MoveItemsOneGrid()
{
	if (ItemSlots.Num() == 0 || !SourceMachine.IsValid() || !TargetMachine.IsValid())
	{
		UpdateDebugStateText();
		return;
	}

	const int32 LastIndex = ItemSlots.Num() - 1;
	const FName LastItem = ItemSlots[LastIndex];
	if (!LastItem.IsNone())
	{
		if (TargetMachine->CanReceiveConveyorItem(LastItem, 1)
			&& TargetMachine->ReceiveConveyorItem(LastItem, 1))
		{
			ItemSlots[LastIndex] = NAME_None;
		}
	}

	for (int32 Index = LastIndex; Index > 0; --Index)
	{
		if (ItemSlots[Index].IsNone() && !ItemSlots[Index - 1].IsNone())
		{
			ItemSlots[Index] = ItemSlots[Index - 1];
			ItemSlots[Index - 1] = NAME_None;
		}
	}

	if (ItemSlots[0].IsNone())
	{
		FName NewItem = NAME_None;
		if (SourceMachine->TryTakeFirstOutputItem(NewItem))
		{
			ItemSlots[0] = NewItem;
		}
	}

	UpdateDebugStateText();
}

FVector AConveyor::GetDebugTextLocalLocation() const
{
	if (PathCells.Num() == 0)
	{
		return DebugTextOffset;
	}

	FVector Center = FVector::ZeroVector;
	for (const FIntPoint& Cell : PathCells)
	{
		Center.X += (Cell.X * CellSize) + (CellSize * 0.5f);
		Center.Y += (Cell.Y * CellSize) + (CellSize * 0.5f);
	}

	Center /= static_cast<float>(PathCells.Num());
	return Center + DebugTextOffset;
}

FString AConveyor::BuildMovingItemSummary() const
{
	TMap<FName, int32> MovingItems;
	for (const FName& Item : ItemSlots)
	{
		if (!Item.IsNone())
		{
			MovingItems.FindOrAdd(Item)++;
		}
	}

	if (MovingItems.Num() == 0)
	{
		return TEXT("None");
	}

	FString Result;
	for (const TPair<FName, int32>& Item : MovingItems)
	{
		if (!Result.IsEmpty())
		{
			Result += TEXT("\n");
		}
		Result += FString::Printf(TEXT("%s x%d"), *Item.Key.ToString(), Item.Value);
	}

	return Result;
}
