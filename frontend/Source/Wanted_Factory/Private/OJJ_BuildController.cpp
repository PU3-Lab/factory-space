// Fill out your copyright notice in the Description page of Project Settings.


#include "OJJ_BuildController.h"

#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "MachineBase.h"
#include "OJJ_Grid.h"
#include "Conveyor.h"

AOJJ_BuildController::AOJJ_BuildController()
{
	// 빌드모드 동안만 호버를 갱신하면 되므로 Tick은 켜두되 기본 비활성.
	// Enter/ExitBuildMode에서 SetActorTickEnabled로 on/off → 빌드모드 밖 0비용.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 컨베이어 모드 기본 클래스(BP 미지정 시). Dummy와 동일 패턴.
	ConveyorClass = AConveyor::StaticClass();
}

void AOJJ_BuildController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Enter/Exit에서 Tick을 on/off하지만, 방어적으로 모드 가드도 유지(UpdateMouseHover 내부에도 가드 있음).
	if (bIsBuildMode)
	{
		UpdateMouseHover();
	}
}

void AOJJ_BuildController::EnterBuildMode()
{
	if (bIsBuildMode)
	{
		return;
	}

	if (!TargetGrid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] TargetGrid 미설정 — EnterBuildMode 중단"));
		return;
	}

	// 모드별 클래스 미설정 가드 — 머신 모드는 MachineClass, 컨베이어 모드는 ConveyorClass 필요.
	if (PlacementMode == EOJJ_BuildPlacementMode::Machine && !MachineClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] MachineClass 미설정 — EnterBuildMode 중단"));
		return;
	}
	if (PlacementMode == EOJJ_BuildPlacementMode::Conveyor && !ConveyorClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] ConveyorClass 미설정 — EnterBuildMode 중단"));
		return;
	}

	TargetGrid->SetVisualizationVisible(true);

	bIsBuildMode = true;

	// 빌드 세션은 항상 회전 0(미회전)으로 시작 — 예측 가능한 기본 방향.
	HoverRotationSteps = 0;

	// 컨베이어 드래그 상태 초기화(이전 세션 잔여 방지).
	bIsDraggingConveyor = false;
	ConveyorDragCells.Reset();

	// 빌드모드 동안에만 호버 Tick 가동
	SetActorTickEnabled(true);

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->bShowMouseCursor = true;
		PC->bEnableClickEvents = true;
		PC->bEnableMouseOverEvents = true;
	}

	// 첫 UpdateMouseHover 호출이 무조건 갱신을 트리거하도록 sentinel로 초기화
	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
}

void AOJJ_BuildController::ExitBuildMode()
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

	// 컨베이어 드래그 상태 정리.
	bIsDraggingConveyor = false;
	ConveyorDragCells.Reset();

	// 호버 Tick 정지 (빌드모드 밖 0비용)
	SetActorTickEnabled(false);

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->bShowMouseCursor = false;
	}

	// 재진입 시 같은 셀에 정지해 있어도 첫 갱신이 동작하도록 sentinel로 리셋
	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);

	// 회전 상태도 리셋 — 다음 진입은 미회전(0)으로 시작(EnterBuildMode 초기화와 일관).
	HoverRotationSteps = 0;
}

void AOJJ_BuildController::ToggleBuildMode()
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

void AOJJ_BuildController::RotateHoverClockwise()
{
	// R은 IMC_Build 전용이라 빌드모드에서만 발동하지만, 방어적으로 가드.
	// 회전은 머신 호버 전용 — 컨베이어 모드에서는 무시(Dummy parity).
	if (!bIsBuildMode || PlacementMode != EOJJ_BuildPlacementMode::Machine)
	{
		return;
	}

	HoverRotationSteps = (HoverRotationSteps + 1) % 4;

	// 마우스가 같은 셀에 멈춰 있어도 회전이 즉시 미리보기에 반영되도록 sentinel 리셋 후 강제 갱신.
	// (UpdateMouseHover는 CursorCell==CurrentHoverCell이면 rebuild를 스킵하므로 sentinel이 필요.)
	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
	UpdateMouseHover();
}

FIntPoint AOJJ_BuildController::ComputeOriginFromCursorCell(FIntPoint CursorCell, AMachineBase* Machine, int32 RotationSteps) const
{
	if (!Machine)
	{
		return CursorCell;
	}

	// AOJJ_Grid::CalculateFootprint / GetMachinePlacementLocation과 동일한 정수화·회전 규칙(EffectiveSize).
	// 입력(cursor → origin)과 시각 보정(origin → footprint center)이 반대 방향이지만
	// 같은 size 가정에서 동작해야 호버/배치와 occupancy/메시 위치가 어긋나지 않음. step 0이면 기존과 동일.
	const FIntPoint Size = AOJJ_Grid::EffectiveSize(Machine->GetMachineSize(), RotationSteps);

	// (Size-1)/2 정수 나눗셈 → lower-left bias. 1x1 offset 0 (회귀 없음).
	return FIntPoint(CursorCell.X - (Size.X - 1) / 2, CursorCell.Y - (Size.Y - 1) / 2);
}

void AOJJ_BuildController::UpdateMouseHover()
{
	if (!bIsBuildMode)
	{
		return;
	}

	if (!TargetGrid)
	{
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return;
	}

	FHitResult Hit;
	const bool bHit = PC->GetHitResultUnderCursorByChannel(
		UEngineTypes::ConvertToTraceType(ECC_Visibility),
		/*bTraceComplex=*/ false,
		Hit);

	if (!bHit)
	{
		// 트레이스 실패 → stale 미리보기/캐시가 다음 클릭에 잘못 적용되지 않도록 명시적 리셋
		TargetGrid->ClearHoverPreview();
		CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
		return;
	}

	const FIntPoint CursorCell = TargetGrid->WorldToGrid(Hit.Location);

	// Conveyor 모드: 드래그/단일 셀 미리보기로 분기 (머신 경로와 독립).
	if (PlacementMode == EOJJ_BuildPlacementMode::Conveyor)
	{
		UpdateConveyorHover(CursorCell);
		return;
	}

	// === Machine 모드 (기존 동작 무변경) ===
	if (!MachineClass)
	{
		return;
	}

	// floor 또는 이미 배치된 머신 위에서 hover를 유지.
	// 머신 Cube mesh가 Visibility 채널을 Block해서 trace를 가로채도, 머신 위 XY는
	// 점유된 셀에 정확히 매핑되므로 CanPlaceMachine 검증을 거치게 그대로 통과시킨다
	// → 점유 셀과 겹친 풋프린트가 빨강으로 표시됨. 그 외 표면(캐릭터/벽 등)은
	// off-grid이므로 기존처럼 ClearHoverPreview로 차단.
	UPrimitiveComponent* HitComp = Hit.GetComponent();
	AActor* HitActor = Hit.GetActor();
	const bool bHitFloor = (HitComp == TargetGrid->GetGridFloorMesh());
	const bool bHitMachine = HitActor && HitActor->IsA<AMachineBase>();
	if (!bHitFloor && !bHitMachine)
	{
		TargetGrid->ClearHoverPreview();
		CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
		return;
	}

	// Tick마다 호출되는 경로라 동일 셀이면 ISM 리빌드 스킵 (CursorCell은 위에서 계산됨)
	if (CursorCell == CurrentHoverCell)
	{
		return;
	}

	AMachineBase* DefaultMachine = MachineClass.GetDefaultObject();
	if (!DefaultMachine)
	{
		return;
	}

	// cursor cell → lower-left origin (마우스 = 풋프린트 중심 정책).
	// 예전엔 IsValidGridCell(cursor)로 anchor 음수/초과를 사전 차단했으나, 이 차단이
	// 왼쪽/위 경계 비대칭을 만들었음 (오른쪽/아래는 anchor가 valid한 상태에서 풋프린트가
	// +X,+Y로 새서 빨강 표시되는데, 왼쪽/위는 anchor 자체가 음수가 되어 hover 사라짐).
	// 이제 origin이 그리드 음수/초과여도 그대로 넘김 → CanPlaceMachine이 풋프린트 셀별
	// IsValidGridCell 검사로 false → UpdateHoverPreview가 풋프린트 전체 빨강 (대칭).
	const FIntPoint Origin = ComputeOriginFromCursorCell(CursorCell, DefaultMachine, HoverRotationSteps);

	TargetGrid->UpdateHoverPreview(DefaultMachine, Origin, HoverRotationSteps);
	CurrentHoverCell = CursorCell;
}

void AOJJ_BuildController::OnLeftClickPressed()
{
	// SP-only contract 강제 (헤더의 MULTIPLAYER LIMITATION 명시와 일치).
	// 클라이언트에서 호출되면 TryPlaceMachine의 HasAuthority ensure가 트리거되고
	// spawn된 머신은 orphan으로 남음 → 진입부에서 차단.
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BuildController] OnLeftClickPressed called on non-authority — SP-only contract"));
		return;
	}

	if (!bIsBuildMode)
	{
		return;
	}

	// Conveyor 모드: 좌클릭 누름 = 드래그 시작. (커밋은 OnLeftClickReleased.)
	if (PlacementMode == EOJJ_BuildPlacementMode::Conveyor)
	{
		FIntPoint CursorCell;
		if (GetCursorCell(CursorCell))
		{
			BeginConveyorDrag(CursorCell);
		}
		return;
	}

	// === Machine 모드 (기존 동작 무변경) ===
	if (!TargetGrid || !MachineClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] TargetGrid 또는 MachineClass 미설정"));
		return;
	}

	// 마우스가 floor 밖이라 호버 갱신이 한 번도 안 됐으면 클릭 무시
	if (CurrentHoverCell.X == INT_MIN || CurrentHoverCell.Y == INT_MIN)
	{
		return;
	}

	AMachineBase* DefaultMachine = MachineClass.GetDefaultObject();
	if (!DefaultMachine)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] MachineClass CDO 없음"));
		return;
	}

	// cursor cell → lower-left origin — UpdateMouseHover와 같은 변환을 사용해야 호버
	// 미리보기와 실제 배치 위치가 어긋나지 않음. CanPlaceMachine이 IsValidGridCell +
	// OccupiedCells 통합 판정하므로 anchor 음수/초과도 자연 거부됨 → 사전 bounds
	// 차단(IsValidGridCell)은 더 이상 필요 없음.
	// 배치도 회전 반영(단계 4). origin/CanPlace/TryPlace + 메시 yaw 모두 같은 HoverRotationSteps를
	// 써야 점유·중심·메시가 일치(Codex 지적 핵심). 호버 미리보기(UpdateMouseHover)와도 동일 step이라
	// "미리보기 = 실제 배치" 정합.
	const FIntPoint Origin = ComputeOriginFromCursorCell(CurrentHoverCell, DefaultMachine, HoverRotationSteps);

	if (!TargetGrid->CanPlaceMachine(DefaultMachine, Origin, HoverRotationSteps))
	{
		UE_LOG(LogTemp, Log, TEXT("[BuildController] origin %s 배치 불가 (bounds/점유)"),
			*Origin.ToString());
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
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] SpawnActor 실패"));
		return;
	}

	FString OutReason;
	if (!TargetGrid->TryPlaceMachine(NewMachine, Origin, OutReason, HoverRotationSteps))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] TryPlaceMachine 실패: %s"), *OutReason);
		NewMachine->Destroy();
		return;
	}

	// 메시 yaw 회전 — TryPlaceMachine이 회전 footprint 중심(GetMachinePlacementLocation(.., step))에
	// 액터를 놓았으므로, 그 중심을 기준으로 yaw만 돌리면 center-anchor 메시가 회전 footprint와 정렬.
	// 시계방향 90°×step (R 방향). 부호가 R 의도와 반대면 -90.f로.
	NewMachine->SetActorRotation(FRotator(0.f, 90.f * HoverRotationSteps, 0.f));

	UE_LOG(LogTemp, Log, TEXT("[BuildController] origin %s 머신 배치 성공"),
		*Origin.ToString());

	// 직전 origin이 이제 점유됨 → 다음 UpdateMouseHover에서 빨강으로 강제 재표시
	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
}

// === Conveyor 입력 (Step 6 — Dummy ADummyBuildController 이식, parity) ===

void AOJJ_BuildController::OnLeftClickReleased()
{
	if (PlacementMode == EOJJ_BuildPlacementMode::Conveyor)
	{
		CommitConveyorDrag();
	}
}

void AOJJ_BuildController::SetPlacementMode(EOJJ_BuildPlacementMode NewMode)
{
	if (PlacementMode == NewMode)
	{
		return;
	}

	// 모드 전환 시 진행 중 드래그는 취소(잔여 상태 방지).
	CancelConveyorDrag();
	PlacementMode = NewMode;
	const TCHAR* ModeName = PlacementMode == EOJJ_BuildPlacementMode::Machine
		? TEXT("Machine")
		: TEXT("Conveyor");
	UE_LOG(LogTemp, Log, TEXT("[BuildController] Placement mode changed to %s"), ModeName);

	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
	UpdateMouseHover();
}

bool AOJJ_BuildController::GetCursorCell(FIntPoint& OutCell) const
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

void AOJJ_BuildController::BeginConveyorDrag(FIntPoint StartCell)
{
	bIsDraggingConveyor = true;
	ConveyorDragCells.Reset();
	ConveyorDragCells.Add(StartCell);
	CurrentHoverCell = StartCell;
	TargetGrid->OJJ_UpdateConveyorPathHoverPreview(ConveyorDragCells);
}

void AOJJ_BuildController::UpdateConveyorDrag(FIntPoint CursorCell)
{
	AppendConveyorPathTo(CursorCell);
	TargetGrid->OJJ_UpdateConveyorPathHoverPreview(ConveyorDragCells);
	CurrentHoverCell = CursorCell;
}

void AOJJ_BuildController::CancelConveyorDrag()
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

void AOJJ_BuildController::CommitConveyorDrag()
{
	if (!bIsDraggingConveyor)
	{
		return;
	}

	bIsDraggingConveyor = false;

	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] Conveyor placement called on non-authority"));
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
	if (!TargetGrid->OJJ_BuildConveyorPlacementPath(ConveyorDragCells, PlacementCells, OutReason))
	{
		UE_LOG(LogTemp, Log, TEXT("[BuildController] Conveyor path cannot be placed: %s"), *OutReason);
		ConveyorDragCells.Reset();
		TargetGrid->ClearHoverPreview();
		CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		ConveyorDragCells.Reset();
		TargetGrid->ClearHoverPreview();
		CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = this;

	AConveyor* Conveyor = World->SpawnActor<AConveyor>(
		ConveyorClass,
		TargetGrid->GetActorLocation(),
		FRotator::ZeroRotator,
		SpawnParams);

	if (!Conveyor)
	{
		ConveyorDragCells.Reset();
		TargetGrid->ClearHoverPreview();
		CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
		return;
	}

	if (!TargetGrid->OJJ_TryPlaceConveyor(Conveyor, PlacementCells, OutReason))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] OJJ_TryPlaceConveyor failed: %s"), *OutReason);
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

void AOJJ_BuildController::AppendConveyorPathTo(FIntPoint TargetCell)
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

void AOJJ_BuildController::AddConveyorPathCell(FIntPoint Cell)
{
	int32 ExistingIndex = INDEX_NONE;
	if (ConveyorDragCells.Find(Cell, ExistingIndex))
	{
		ConveyorDragCells.SetNum(ExistingIndex + 1);
		return;
	}

	ConveyorDragCells.Add(Cell);
}

void AOJJ_BuildController::UpdateConveyorHover(FIntPoint CursorCell)
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
	TargetGrid->OJJ_UpdateConveyorPathHoverPreview(PreviewCells);
	CurrentHoverCell = CursorCell;
}
