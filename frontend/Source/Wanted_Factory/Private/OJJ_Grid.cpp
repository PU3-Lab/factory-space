// Fill out your copyright notice in the Description page of Project Settings.


#include "OJJ_Grid.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "MachineBase.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AOJJ_Grid::AOJJ_Grid()
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

	// 기본은 collision 없음. 빌드 모드 진입 시 SetVisualizationVisible(true)에서 필요한
	// 채널만 활성화하여 hidden plane이 다른 trace 시스템에 끼어들지 않도록 격리.
	GridFloorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GridFloorMesh->SetVisibility(false);

	// 호버 미리보기 ISM (Plane은 위에서 로드한 정적 변수 재사용)
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

void AOJJ_Grid::BeginPlay()
{
	Super::BeginPlay();
	// 자동 스캔 의도적으로 제거: 멀티 그리드 환경에서 cross-grid contamination
	// 위험이 있어 그리드 ownership contract 합의 전까지 명시적 등록만 지원.
	// 레벨에 미리 배치된 머신은 RegisterExistingMachine으로 명시 등록 필요.

	if (GridFloorMesh)
	{
		// Plane 기본 크기 100x100 → CellSize 단위로 스케일
		const float ScaleFactor = (VisualizationRange * CellSize) / 100.0f;
		GridFloorMesh->SetRelativeScale3D(FVector(ScaleFactor, ScaleFactor, 1.0f));

		// Plane은 액터 중심에 위치 → 그리드 lower-left 원점에 맞추려면 절반만큼 +XY 오프셋
		// Z=1로 Z-fighting 방지
		const float OffsetXY = (VisualizationRange * CellSize) / 2.0f;
		GridFloorMesh->SetRelativeLocation(FVector(OffsetXY, OffsetXY, 1.0f));
	}
}

void AOJJ_Grid::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

FIntPoint AOJJ_Grid::WorldToGrid(FVector WorldPos) const
{
	const FVector Local = WorldPos - GetActorLocation();
	const int32 X = FMath::FloorToInt(Local.X / CellSize);
	const int32 Y = FMath::FloorToInt(Local.Y / CellSize);
	return FIntPoint(X, Y);
}

FVector AOJJ_Grid::GridToWorld(FIntPoint Coord) const
{
	const FVector Origin = GetActorLocation();
	const float WorldX = Origin.X + (Coord.X * CellSize) + (CellSize * 0.5f);
	const float WorldY = Origin.Y + (Coord.Y * CellSize) + (CellSize * 0.5f);
	return FVector(WorldX, WorldY, Origin.Z);
}

FVector AOJJ_Grid::GetGridCenter() const
{
	// 원점(액터 위치)은 그리드 좌하단. placement extent(GridSize)의 정중앙.
	const FVector Origin = GetActorLocation();
	const float CenterX = Origin.X + (GridSize.X * CellSize * 0.5f);
	const float CenterY = Origin.Y + (GridSize.Y * CellSize * 0.5f);
	return FVector(CenterX, CenterY, Origin.Z);
}

FIntPoint AOJJ_Grid::EffectiveSize(FVector2D RawSize, int32 RotationSteps)
{
	// CalculateFootprint / GetMachinePlacementLocation과 동일한 정수화 규칙(CeilToInt + Max 1).
	const int32 X = FMath::Max(1, FMath::CeilToInt(RawSize.X));
	const int32 Y = FMath::Max(1, FMath::CeilToInt(RawSize.Y));

	// 90°/270°(홀수 step)에서 치수 swap. 음수 step도 parity로 정상 동작.
	return ((RotationSteps % 2) != 0) ? FIntPoint(Y, X) : FIntPoint(X, Y);
}

FVector AOJJ_Grid::GetMachinePlacementLocation(AMachineBase* Machine, FIntPoint Origin, int32 RotationSteps) const
{
	// 방어층: 머신 없으면 lower-left 셀 중심 반환 (호출자가 잘못 부른 경우 안전한 fallback).
	if (!Machine)
	{
		return GridToWorld(Origin);
	}

	// CalculateFootprint와 동일한 정수화·회전 규칙(EffectiveSize). 두 경로가 같은 size
	// 가정에서 동작해야 occupancy 셀과 visual 위치가 정확히 일치. step 0이면 기존과 동일.
	const FIntPoint Size = EffectiveSize(Machine->GetMachineSize(), RotationSteps);

	// lower-left cell 중심에서 footprint 전체 center로 이동. 1x1이면 offset 0 (회귀 없음).
	const FVector LowerLeftCenter = GridToWorld(Origin);
	const float OffsetX = (Size.X - 1) * CellSize * 0.5f;
	const float OffsetY = (Size.Y - 1) * CellSize * 0.5f;
	return FVector(LowerLeftCenter.X + OffsetX, LowerLeftCenter.Y + OffsetY, LowerLeftCenter.Z);
}

bool AOJJ_Grid::IsValidGridCell(FIntPoint Cell) const
{
	return Cell.X >= 0 && Cell.X < GridSize.X
		&& Cell.Y >= 0 && Cell.Y < GridSize.Y;
}

// === Grid Query (GridManager/컨베이어용 읽기 전용 조회) — 순수 추가, write 경로 미변경 ===

AMachineBase* AOJJ_Grid::GetMachineAtCell(FIntPoint Cell) const
{
	if (const TWeakObjectPtr<AMachineBase>* Found = OccupiedCells.Find(Cell))
	{
		// Get()은 유효하면 ptr, GC됐으면 nullptr 반환 → stale 셀 자체 방어
		return Found->Get();
	}
	return nullptr;
}

bool AOJJ_Grid::IsCellOccupied(FIntPoint Cell) const
{
	// Contains 대신 GetMachineAtCell 위임 → 파괴된 머신 셀을 "비점유"로 일관 처리
	return GetMachineAtCell(Cell) != nullptr;
}

const TArray<FIntPoint>* AOJJ_Grid::GetMachineCells(AMachineBase* Machine) const
{
	// IsValid: nullptr + pending-kill/garbage 모두 거부.
	// GetMachineAtCell이 weak Get()으로 stale을 nullptr 처리하는 것과 일관되게,
	// 죽은(=곧 GC될) 머신의 footprint/origin 메타데이터가 새지 않도록 차단 (Codex 지적: 양방향 일관성).
	if (!IsValid(Machine))
	{
		return nullptr;
	}
	// MachineToCells는 weak-key 맵 — raw ptr로 조회 가능(암시적 TWeakObjectPtr 변환)
	return MachineToCells.Find(Machine);
}

FIntPoint AOJJ_Grid::GetMachineOrigin(AMachineBase* Machine) const
{
	const TArray<FIntPoint>* Cells = GetMachineCells(Machine);
	if (!Cells || Cells->Num() == 0)
	{
		// 미등록 머신 센티넬 (BuildController의 INT_MIN 컨벤션과 일치)
		return FIntPoint(INT_MIN, INT_MIN);
	}

	// 풋프린트는 Origin부터 비음수 offset → min corner == 등록 시 Origin (회전 무관)
	FIntPoint Origin = (*Cells)[0];
	for (const FIntPoint& Cell : *Cells)
	{
		Origin.X = FMath::Min(Origin.X, Cell.X);
		Origin.Y = FMath::Min(Origin.Y, Cell.Y);
	}
	return Origin;
}

TArray<FIntPoint> AOJJ_Grid::CalculateFootprint(AMachineBase* Machine, FIntPoint Origin, int32 RotationSteps) const
{
	TArray<FIntPoint> Cells;
	if (!Machine)
	{
		return Cells;
	}

	// 회전·정수화 규칙은 EffectiveSize로 통일. step 0이면 기존 (CeilToInt+Max1) 동일.
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

bool AOJJ_Grid::CanPlaceMachine(AMachineBase* Machine, FIntPoint Origin, int32 RotationSteps) const
{
	if (!Machine)
	{
		return false;
	}

	// 모든 placement entry point가 같은 invariant 따르도록 풋프린트 전체 셀에 대해
	// bounds + 점유를 동시에 검사 (단일 패스).
	for (const FIntPoint& Cell : CalculateFootprint(Machine, Origin, RotationSteps))
	{
		if (!IsValidGridCell(Cell))
		{
			return false;
		}

		const TWeakObjectPtr<AMachineBase>* Found = OccupiedCells.Find(Cell);
		if (Found && Found->IsValid())
		{
			return false;
		}
	}

	return true;
}

void AOJJ_Grid::SweepStaleEntries()
{
	for (auto It = MachineToCells.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			for (const FIntPoint& Cell : It.Value())
			{
				const TWeakObjectPtr<AMachineBase>* Found = OccupiedCells.Find(Cell);
				if (Found && !Found->IsValid())
				{
					OccupiedCells.Remove(Cell);
				}
			}
			It.RemoveCurrent();
		}
	}
}

bool AOJJ_Grid::RegisterMachineInternal(AMachineBase* Machine, FIntPoint Origin, FString& OutReason, int32 RotationSteps)
{
	if (!HasAuthority())
	{
		ensureMsgf(false, TEXT("Grid placement called on non-authority"));
		OutReason = TEXT("Not authority");
		return false;
	}

	SweepStaleEntries();

	if (!Machine)
	{
		OutReason = TEXT("Invalid machine");
		return false;
	}

	if (MachineToCells.Contains(Machine))
	{
		OutReason = TEXT("Machine already placed. Use TryMoveMachine for repositioning.");
		return false;
	}

	if (!CanPlaceMachine(Machine, Origin, RotationSteps))
	{
		OutReason = TEXT("Cell already occupied");
		return false;
	}

	TArray<FIntPoint> Footprint = CalculateFootprint(Machine, Origin, RotationSteps);
	for (const FIntPoint& Cell : Footprint)
	{
		OccupiedCells.Add(Cell, Machine);
	}
	MachineToCells.Add(Machine, MoveTemp(Footprint));

	OutReason.Reset();
	return true;
}

bool AOJJ_Grid::TryPlaceMachine(AMachineBase* Machine, FIntPoint Origin, FString& OutReason, int32 RotationSteps)
{
	if (!RegisterMachineInternal(Machine, Origin, OutReason, RotationSteps))
	{
		return false;
	}

	// center anchor 보정 (헬퍼 안에 합의 contract 명시). 회전 시 회전된 footprint center로 정렬.
	if (!Machine->SetActorLocation(GetMachinePlacementLocation(Machine, Origin, RotationSteps)))
	{
		RemoveMachine(Machine);
		OutReason = TEXT("Failed to move machine to target location");
		return false;
	}

	return true;
}

bool AOJJ_Grid::RegisterExistingMachine(AMachineBase* Machine, FIntPoint Origin, FString& OutReason)
{
	// Center anchor 검증 — 머신 팀 합의 contract를 양쪽 placement 경로에서 동일하게 강제.
	// TryPlaceMachine은 GetMachinePlacementLocation으로 spawn 위치를 보정하지만, 사전 배치
	// 머신은 디자이너가 의도적으로 놓은 위치이므로 코드가 snap하지 않는다. 대신 lower-left
	// Origin이 가리키는 풋프린트 center와 실제 액터 XY가 일치하는지 검사하고, 어긋나면
	// loud fail → 데이터(OccupiedCells)와 시각(actor transform) invariant 보장.
	if (Machine)
	{
		const FVector Expected = GetMachinePlacementLocation(Machine, Origin);
		const FVector Actual = Machine->GetActorLocation();
		// Z는 머신 메시 높이 차이 허용 — 그리드 평면 정합만 검증.
		const float DistXY = FVector2D(Expected.X - Actual.X, Expected.Y - Actual.Y).Size();
		const float Tolerance = 1.0f; // 1uu — floating-point 노이즈 흡수 + 의도적 misplacement 차단

		if (DistXY > Tolerance)
		{
			OutReason = FString::Printf(
				TEXT("Pre-placed machine center anchor mismatch — Expected XY=(%.1f,%.1f), Actual XY=(%.1f,%.1f), Dist=%.2f, Tolerance=%.2f. Move machine to expected XY or pass correct Origin."),
				Expected.X, Expected.Y, Actual.X, Actual.Y, DistXY, Tolerance);
			ensureMsgf(false, TEXT("[OJJ_Grid] %s"), *OutReason);
			UE_LOG(LogTemp, Error, TEXT("[OJJ_Grid] RegisterExistingMachine refused: %s"), *OutReason);
			return false;
		}
	}

	return RegisterMachineInternal(Machine, Origin, OutReason);
}

void AOJJ_Grid::SetVisualizationVisible(bool bVisible)
{
	if (!GridFloorMesh)
	{
		return;
	}

	GridFloorMesh->SetVisibility(bVisible);

	if (bVisible)
	{
		// 빌드 모드 진입: cursor 라인 트레이스만 받도록 Visibility 채널만 Block.
		// Pawn/Camera/기타 trace는 Ignore로 두어 게임플레이 trace 시스템과 격리.
		GridFloorMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		GridFloorMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		GridFloorMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}
	else
	{
		// 빌드 모드 종료: 어떤 trace에도 영향 없도록 collision 완전 해제.
		GridFloorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AOJJ_Grid::UpdateHoverPreview(AMachineBase* Machine, FIntPoint Origin, int32 RotationSteps)
{
	ClearHoverPreview();

	if (!Machine)
	{
		return;
	}

	// 단일 진실원: 호버 색 판정과 클릭 시 placement 판정을 같은 함수(CanPlaceMachine)로 결정.
	// 풋프린트 중 한 칸이라도 점유 / out-of-bounds이면 전체 빨강. 시각 피드백이 실제
	// CanPlaceMachine 결과와 항상 일치 → "겹친 칸만 빨강, 나머지 녹색" 같은 거짓말 제거.
	// (이전 셀별 판정 — bIsOccupied/bIsOutOfBounds를 셀마다 OR — 으로 인한 회귀.)
	const bool bCanPlace = CanPlaceMachine(Machine, Origin, RotationSteps);
	UInstancedStaticMeshComponent* TargetISM = bCanPlace ? ValidHoverISM.Get() : InvalidHoverISM.Get();
	if (!TargetISM)
	{
		return;
	}

	const TArray<FIntPoint> FootprintCells = CalculateFootprint(Machine, Origin, RotationSteps);
	for (const FIntPoint& Cell : FootprintCells)
	{
		// 베이스 그리드 평면(Z=1)보다 위로 +2 오프셋 → 가림 방지
		const FVector CellCenter = GridToWorld(Cell);
		const FVector InstanceLocation(CellCenter.X, CellCenter.Y, CellCenter.Z + 2.0f);

		// Plane(100x100) → CellSize 유닛으로 스케일
		const FVector InstanceScale(CellSize / 100.0f, CellSize / 100.0f, 1.0f);
		const FTransform InstanceTransform(FRotator::ZeroRotator, InstanceLocation, InstanceScale);

		// World-space 좌표로 추가 (액터 위치 무관)
		TargetISM->AddInstance(InstanceTransform, /*bWorldSpace=*/true);
	}
}

void AOJJ_Grid::ClearHoverPreview()
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

bool AOJJ_Grid::RemoveMachine(AMachineBase* Machine)
{
	if (!HasAuthority())
	{
		ensureMsgf(false, TEXT("RemoveMachine called on non-authority"));
		return false;
	}

	if (!Machine)
	{
		return false;
	}

	const TArray<FIntPoint>* Cells = MachineToCells.Find(Machine);
	if (!Cells)
	{
		return false;
	}

	for (const FIntPoint& Cell : *Cells)
	{
		OccupiedCells.Remove(Cell);
	}
	MachineToCells.Remove(Machine);
	return true;
}

bool AOJJ_Grid::RemoveMachineAt(FIntPoint Coord)
{
	if (!HasAuthority())
	{
		ensureMsgf(false, TEXT("RemoveMachineAt called on non-authority"));
		return false;
	}

	const TWeakObjectPtr<AMachineBase>* Found = OccupiedCells.Find(Coord);
	if (!Found || !Found->IsValid())
	{
		return false;
	}

	return RemoveMachine(Found->Get());
}
