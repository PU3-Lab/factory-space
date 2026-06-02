// Fill out your copyright notice in the Description page of Project Settings.

#include "Dummy_BuildController.h"

#include "Dummy_Grid.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "MachineBase.h"
#include "dummy_converyor.h"

ADummyBuildController::ADummyBuildController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	ConveyorClass = ADummyConveyor::StaticClass();
}

void ADummyBuildController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bIsBuildMode)
	{
		UpdateMouseHover();
	}
}

void ADummyBuildController::EnterBuildMode()
{
	if (bIsBuildMode)
	{
		return;
	}

	if (!TargetGrid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DummyBuildController] TargetGrid is not set"));
		return;
	}

	if (PlacementMode == EDummyBuildPlacementMode::Machine && !MachineClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DummyBuildController] MachineClass is not set"));
		return;
	}

	if (PlacementMode == EDummyBuildPlacementMode::Conveyor && !ConveyorClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DummyBuildController] ConveyorClass is not set"));
		return;
	}

	TargetGrid->SetVisualizationVisible(true);

	bIsBuildMode = true;
	HoverRotationSteps = 0;
	bIsDraggingConveyor = false;
	ConveyorDragCells.Reset();
	SetActorTickEnabled(true);

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->bShowMouseCursor = true;
		PC->bEnableClickEvents = true;
		PC->bEnableMouseOverEvents = true;
	}

	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
}

void ADummyBuildController::ExitBuildMode()
{
	if (!bIsBuildMode)
	{
		return;
	}

	if (TargetGrid)
	{
		TargetGrid->SetVisualizationVisible(false);
		TargetGrid->ClearHoverPreview();
	}

	bIsBuildMode = false;
	bIsDraggingConveyor = false;
	ConveyorDragCells.Reset();
	SetActorTickEnabled(false);

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->bShowMouseCursor = false;
	}

	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
	HoverRotationSteps = 0;
}

void ADummyBuildController::SetPlacementMode(EDummyBuildPlacementMode NewMode)
{
	if (PlacementMode == NewMode)
	{
		return;
	}

	CancelConveyorDrag();
	PlacementMode = NewMode;
	const TCHAR* ModeName = PlacementMode == EDummyBuildPlacementMode::Machine
		? TEXT("Machine")
		: TEXT("Conveyor");
	UE_LOG(LogTemp, Log, TEXT("[DummyBuildController] Placement mode changed to %s"), ModeName);

	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
	UpdateMouseHover();
}

void ADummyBuildController::ToggleBuildMode()
{
	if (bIsBuildMode)
	{
		ExitBuildMode();
	}
	else
	{
		EnterBuildMode();
	}
}

void ADummyBuildController::RotateHoverClockwise()
{
	if (!bIsBuildMode || PlacementMode != EDummyBuildPlacementMode::Machine)
	{
		return;
	}

	HoverRotationSteps = (HoverRotationSteps + 1) % 4;
	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
	UpdateMouseHover();
}

bool ADummyBuildController::GetCursorCell(FIntPoint& OutCell) const
{
	if (!TargetGrid)
	{
		return false;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return false;
	}

	FHitResult Hit;
	const bool bHit = PC->GetHitResultUnderCursorByChannel(
		UEngineTypes::ConvertToTraceType(ECC_Visibility),
		/*bTraceComplex=*/false,
		Hit);

	if (!bHit)
	{
		return false;
	}

	OutCell = TargetGrid->WorldToGrid(Hit.Location);
	return true;
}

FIntPoint ADummyBuildController::ComputeOriginFromCursorCell(
	FIntPoint CursorCell,
	AMachineBase* Machine,
	int32 RotationSteps) const
{
	if (!Machine)
	{
		return CursorCell;
	}

	const FIntPoint Size = ADummyGrid::EffectiveSize(Machine->GetMachineSize(), RotationSteps);
	return FIntPoint(CursorCell.X - (Size.X - 1) / 2, CursorCell.Y - (Size.Y - 1) / 2);
}

void ADummyBuildController::UpdateMouseHover()
{
	if (!bIsBuildMode || !TargetGrid)
	{
		return;
	}

	FIntPoint CursorCell;
	if (!GetCursorCell(CursorCell))
	{
		TargetGrid->ClearHoverPreview();
		CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
		return;
	}

	if (PlacementMode == EDummyBuildPlacementMode::Conveyor)
	{
		UpdateConveyorHover(CursorCell);
		return;
	}

	UpdateMachineHover(CursorCell);
}

void ADummyBuildController::UpdateMachineHover(FIntPoint CursorCell)
{
	if (!MachineClass || CursorCell == CurrentHoverCell)
	{
		return;
	}

	AMachineBase* DefaultMachine = MachineClass.GetDefaultObject();
	if (!DefaultMachine)
	{
		return;
	}

	const FIntPoint Origin = ComputeOriginFromCursorCell(CursorCell, DefaultMachine, HoverRotationSteps);
	TargetGrid->UpdateHoverPreview(DefaultMachine, Origin, HoverRotationSteps);
	CurrentHoverCell = CursorCell;
}

void ADummyBuildController::UpdateConveyorHover(FIntPoint CursorCell)
{
	if (bIsDraggingConveyor)
	{
		UpdateConveyorDrag(CursorCell);
		return;
	}

	if (CursorCell == CurrentHoverCell)
	{
		return;
	}

	TArray<FIntPoint> PreviewCells;
	PreviewCells.Add(CursorCell);
	TargetGrid->UpdateConveyorPathHoverPreview(PreviewCells);
	CurrentHoverCell = CursorCell;
}

void ADummyBuildController::OnLeftClickPressed()
{
	if (!bIsBuildMode)
	{
		return;
	}

	if (PlacementMode == EDummyBuildPlacementMode::Conveyor)
	{
		FIntPoint CursorCell;
		if (GetCursorCell(CursorCell))
		{
			BeginConveyorDrag(CursorCell);
		}
		return;
	}

	PlaceMachineAtHover();
}

void ADummyBuildController::OnLeftClickReleased()
{
	if (PlacementMode == EDummyBuildPlacementMode::Conveyor)
	{
		CommitConveyorDrag();
	}
}

void ADummyBuildController::BeginConveyorDrag(FIntPoint StartCell)
{
	bIsDraggingConveyor = true;
	ConveyorDragCells.Reset();
	ConveyorDragCells.Add(StartCell);
	CurrentHoverCell = StartCell;
	TargetGrid->UpdateConveyorPathHoverPreview(ConveyorDragCells);
}

void ADummyBuildController::UpdateConveyorDrag(FIntPoint CursorCell)
{
	AppendConveyorPathTo(CursorCell);
	TargetGrid->UpdateConveyorPathHoverPreview(ConveyorDragCells);
	CurrentHoverCell = CursorCell;
}

void ADummyBuildController::CancelConveyorDrag()
{
	if (!bIsDraggingConveyor)
	{
		return;
	}

	bIsDraggingConveyor = false;
	ConveyorDragCells.Reset();
	if (TargetGrid)
	{
		TargetGrid->ClearHoverPreview();
	}
	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
}

void ADummyBuildController::CommitConveyorDrag()
{
	if (!bIsDraggingConveyor)
	{
		return;
	}

	bIsDraggingConveyor = false;

	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[DummyBuildController] Conveyor placement called on non-authority"));
		ConveyorDragCells.Reset();
		return;
	}

	if (!TargetGrid || !ConveyorClass || ConveyorDragCells.Num() == 0)
	{
		ConveyorDragCells.Reset();
		return;
	}

	TArray<FIntPoint> PlacementCells;
	FString OutReason;
	if (!TargetGrid->BuildConveyorPlacementPath(ConveyorDragCells, PlacementCells, OutReason))
	{
		UE_LOG(LogTemp, Log, TEXT("[DummyBuildController] Conveyor path cannot be placed: %s"), *OutReason);
		ConveyorDragCells.Reset();
		TargetGrid->ClearHoverPreview();
		CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		ConveyorDragCells.Reset();
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = this;

	ADummyConveyor* Conveyor = World->SpawnActor<ADummyConveyor>(
		ConveyorClass,
		TargetGrid->GetActorLocation(),
		FRotator::ZeroRotator,
		SpawnParams);

	if (!Conveyor)
	{
		ConveyorDragCells.Reset();
		return;
	}

	if (!TargetGrid->TryPlaceConveyor(Conveyor, PlacementCells, OutReason))
	{
		UE_LOG(LogTemp, Warning, TEXT("[DummyBuildController] TryPlaceConveyor failed: %s"), *OutReason);
		Conveyor->Destroy();
		ConveyorDragCells.Reset();
		TargetGrid->ClearHoverPreview();
		CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
		return;
	}

	ConveyorDragCells.Reset();
	TargetGrid->ClearHoverPreview();
	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
}

void ADummyBuildController::AppendConveyorPathTo(FIntPoint TargetCell)
{
	if (ConveyorDragCells.Num() == 0)
	{
		ConveyorDragCells.Add(TargetCell);
		return;
	}

	FIntPoint LastCell = ConveyorDragCells.Last();
	if (LastCell == TargetCell)
	{
		return;
	}

	const int32 StepX = TargetCell.X > LastCell.X ? 1 : -1;
	while (LastCell.X != TargetCell.X)
	{
		LastCell.X += StepX;
		AddConveyorPathCell(LastCell);
	}

	const int32 StepY = TargetCell.Y > LastCell.Y ? 1 : -1;
	while (LastCell.Y != TargetCell.Y)
	{
		LastCell.Y += StepY;
		AddConveyorPathCell(LastCell);
	}
}

void ADummyBuildController::AddConveyorPathCell(FIntPoint Cell)
{
	int32 ExistingIndex = INDEX_NONE;
	if (ConveyorDragCells.Find(Cell, ExistingIndex))
	{
		ConveyorDragCells.SetNum(ExistingIndex + 1);
		return;
	}

	ConveyorDragCells.Add(Cell);
}

void ADummyBuildController::PlaceMachineAtHover()
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[DummyBuildController] Machine placement called on non-authority"));
		return;
	}

	if (!TargetGrid || !MachineClass)
	{
		return;
	}

	if (CurrentHoverCell.X == INT_MIN || CurrentHoverCell.Y == INT_MIN)
	{
		return;
	}

	AMachineBase* DefaultMachine = MachineClass.GetDefaultObject();
	if (!DefaultMachine)
	{
		return;
	}

	const FIntPoint Origin = ComputeOriginFromCursorCell(CurrentHoverCell, DefaultMachine, HoverRotationSteps);
	if (!TargetGrid->CanPlaceMachine(DefaultMachine, Origin, HoverRotationSteps))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = this;

	AMachineBase* NewMachine = World->SpawnActor<AMachineBase>(
		MachineClass,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams);

	if (!NewMachine)
	{
		return;
	}

	FString OutReason;
	if (!TargetGrid->TryPlaceMachine(NewMachine, Origin, OutReason, HoverRotationSteps))
	{
		UE_LOG(LogTemp, Warning, TEXT("[DummyBuildController] TryPlaceMachine failed: %s"), *OutReason);
		NewMachine->Destroy();
		return;
	}

	NewMachine->SetActorRotation(FRotator(0.f, 90.f * HoverRotationSteps, 0.f));
	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
}
