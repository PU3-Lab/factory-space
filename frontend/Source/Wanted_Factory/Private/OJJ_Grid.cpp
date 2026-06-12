// Fill out your copyright notice in the Description page of Project Settings.


#include "OJJ_Grid.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Conveyor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FactoryManagerSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "MachineBase.h"
#include "Materials/MaterialInterface.h"
#include "OJJ_Foundation.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Resource/ResourceBase.h"
#include "UObject/ConstructorHelpers.h"

// === Conveyor 포트 판정 헬퍼 (Step 3-b-1) ===
// Dummy_GridConveyor.cpp의 검증된 anonymous-namespace 헬퍼를 OJJ_로 이식(parity — 로직 동일, 명칭/타입만 치환).
// 포트 모델 = Dummy dot-product 방식 그대로(Step 2 입력포트 API와 별개; Step 2는 Step 5 스냅샷용으로 보존).
// 머신 타입은 머지(PR #55)로 일반화된 AMachineBase*. Grid 좌표변환은 AOJJ_Grid 멤버(GridToWorld/IsValidGridCell).
namespace
{
constexpr float OJJ_PortDotThreshold = 0.01f;

const FIntPoint OJJ_NeighborSteps[] = {
	FIntPoint(1, 0),
	FIntPoint(-1, 0),
	FIntPoint(0, 1),
	FIntPoint(0, -1)
};

int32 OJJ_ManhattanDistance(FIntPoint A, FIntPoint B)
{
	return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y);
}

AMachineBase* OJJ_GetMachineAtCell(
	const TMap<FIntPoint, TWeakObjectPtr<AActor>>& OccupiedCells,
	FIntPoint Cell)
{
	const TWeakObjectPtr<AActor>* Found = OccupiedCells.Find(Cell);
	return Found && Found->IsValid() ? Cast<AMachineBase>(Found->Get()) : nullptr;
}

FIntPoint OJJ_GetMachineBackStep(const AMachineBase* Machine)
{
	const FVector Forward = Machine ? Machine->GetActorForwardVector() : FVector::ForwardVector;
	if (FMath::Abs(Forward.X) >= FMath::Abs(Forward.Y))
	{
		return FIntPoint(Forward.X >= 0.f ? -1 : 1, 0);
	}

	return FIntPoint(0, Forward.Y >= 0.f ? -1 : 1);
}

FIntPoint OJJ_GetMachineFrontStep(const AMachineBase* Machine)
{
	const FIntPoint BackStep = OJJ_GetMachineBackStep(Machine);
	return FIntPoint(-BackStep.X, -BackStep.Y);
}

float OJJ_GetMachineForwardDotToCell(const AOJJ_Grid* Grid, const AMachineBase* Machine, FIntPoint Cell)
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

bool OJJ_IsBehindMachine(const AOJJ_Grid* Grid, const AMachineBase* Machine, FIntPoint Cell)
{
	return OJJ_GetMachineForwardDotToCell(Grid, Machine, Cell) < -OJJ_PortDotThreshold;
}

bool OJJ_IsInFrontOfMachine(const AOJJ_Grid* Grid, const AMachineBase* Machine, FIntPoint Cell)
{
	return OJJ_GetMachineForwardDotToCell(Grid, Machine, Cell) > OJJ_PortDotThreshold;
}

bool OJJ_IsMachineBackOutputPair(
	const AOJJ_Grid* Grid,
	const AMachineBase* Machine,
	FIntPoint MachineCell,
	FIntPoint ConveyorCell,
	const TArray<FIntPoint>& MachineCells)
{
	if (!MachineCells.Contains(MachineCell))
	{
		return false;
	}

	const FIntPoint BackStep = OJJ_GetMachineBackStep(Machine);
	if (MachineCell + BackStep != ConveyorCell)
	{
		return false;
	}

	if (MachineCells.Contains(ConveyorCell) || !Grid->IsValidGridCell(ConveyorCell))
	{
		return false;
	}

	// 포트 셀 일원화: ConveyorCell이 대칭 규칙으로 선택된 출력 포트 셀이어야 도킹 허용.
	// GetMachineOutputCells와 동일한 OJJ_PortCellsFromFootprint(BackStep, 출력 포트수) 경유 →
	// 화살표 표시 셀 = 도킹 허용 셀 완전 일치. 포트수=면길이/0이면 전부라 기존 동작 불변.
	const TArray<FIntPoint> OutputPortCells =
		AOJJ_Grid::OJJ_PortCellsFromFootprint(MachineCells, BackStep, Machine->GetOutputPortCount());
	if (!OutputPortCells.Contains(ConveyorCell))
	{
		return false;
	}

	return OJJ_IsBehindMachine(Grid, Machine, ConveyorCell);
}

bool OJJ_IsMachineFrontInputPair(
	const AOJJ_Grid* Grid,
	const AMachineBase* Machine,
	FIntPoint MachineCell,
	FIntPoint ConveyorCell,
	const TArray<FIntPoint>& MachineCells)
{
	if (!MachineCells.Contains(MachineCell))
	{
		return false;
	}

	const FIntPoint FrontStep = OJJ_GetMachineFrontStep(Machine);
	if (MachineCell + FrontStep != ConveyorCell)
	{
		return false;
	}

	if (MachineCells.Contains(ConveyorCell) || !Grid->IsValidGridCell(ConveyorCell))
	{
		return false;
	}

	// 포트 셀 일원화: ConveyorCell이 대칭 규칙으로 선택된 입력 포트 셀이어야 도킹 허용.
	// OJJ_GetMachineInputCells와 동일한 OJJ_PortCellsFromFootprint(FrontStep, 입력 포트수) 경유 →
	// 화살표 표시 셀 = 도킹 허용 셀 완전 일치. 포트수=면길이/0이면 전부라 기존 동작 불변.
	const TArray<FIntPoint> InputPortCells =
		AOJJ_Grid::OJJ_PortCellsFromFootprint(MachineCells, FrontStep, Machine->GetInputPortCount());
	if (!InputPortCells.Contains(ConveyorCell))
	{
		return false;
	}

	return OJJ_IsInFrontOfMachine(Grid, Machine, ConveyorCell);
}

bool OJJ_FindInputMachineAtPathEnd(
	const AOJJ_Grid* Grid,
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
		// 포트 없는 머신(송전탑/발전소/차폐장 등)은 컨베이어 endpoint 불가 — 입력 포트 0이면 수신 불가.
		if (!EndMachine || EndMachine == StartMachine || !EndMachineCells
			|| EndMachine->GetInputPortCount() <= 0
			|| !OJJ_IsMachineFrontInputPair(Grid, EndMachine, EndCell, PreviousCell, *EndMachineCells))
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
	for (const FIntPoint& Step : OJJ_NeighborSteps)
	{
		const FIntPoint MachineCell = EndCell - Step;
		AMachineBase* AdjacentMachine = OJJ_GetMachineAtCell(OccupiedCells, MachineCell);
		// 포트 없는 머신(송전탑/발전소/차폐장 등)은 컨베이어 endpoint 불가 — 입력 포트 0이면 후보 제외.
		if (!AdjacentMachine || AdjacentMachine == StartMachine || AdjacentMachine->GetInputPortCount() <= 0)
		{
			continue;
		}

		bSawAdjacentMachine = true;
		const TArray<FIntPoint>* MachineCells = ActorToCells.Find(AdjacentMachine);
		if (MachineCells && OJJ_IsMachineFrontInputPair(Grid, AdjacentMachine, MachineCell, EndCell, *MachineCells))
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

// F3.7-1(f3_7 계획 ㊆): 균일 SurfaceZ 실패 경로의 경사 허용 검사 — 단일 높이 규칙의 램프 경로
// 예외 게이트. 조건: ① 전 셀 Foundation 커버(혼합 지형 경사는 F1-c 규칙대로 계속 거부 — 경사는
// 램프/평판 위만) ② 인접 셀 |ΔZ| ≤ OJJ_MaxConveyorStepZ(F3.7' 개정 — 기본 100, 램프
// MaxRampStepPerRow 기본과 동기) ③ 방향 전환(코너) 셀은 양옆 ΔZ=0
// (㊅ — 벨트 코너 세그먼트가 평면 전제라 경사 코너 금지). 성공 시 OutCellZs = 셀별 절대 SurfaceZ
// (PathCells와 1:1) — OJJ_TryPlaceConveyor가 시작 셀 기준 로컬화(㊇) 후 벨트에 주입(㊃).
// ※ 의도(Codex F3.7-1 ②④): 검사는 **형태 기반**(㊆(a) 채택 — 액터가 램프인지는 안 봄).
// F3.10 결정 승격(2026-06-12 PIE 실측 확정): 한계 100(F3.7')에서 평판↔평판 Δ1단(100uu) **벨트
// 직결도 의도된 기능** — 벨트 시각/흐름 정상 실측. 단차 물류 수단: gap 0 = 직결, gap 1 = 1칸
// 램프(45° 쐐기), gap 2+ = 자동 맞춤 램프. (구 서술 "평판 간 ΔZ는 항상 45 초과라 걸러짐"은
// 한계 45 시절 — 100 완화로 폐기.)
// 끝점 머신 셀 포함 순회도 의도 — 기존 균일 검사와 동일 범위("전 경로 셀, 머신 끝점 포함"),
// 지형 직배치 머신과의 혼합 경로는 기존 정책대로 거부.
bool OJJ_ValidateConveyorSlopePath(
	const AOJJ_Grid* Grid,
	const TArray<FIntPoint>& PathCells,
	TArray<float>& OutCellZs,
	FString& OutReason)
{
	// F3.7' 개정(㊆): 45 고정(보행 기준) → 그리드 프로퍼티(기본 100 — 램프 행간 한계 기본과 동기,
	// 급경사 컨베이어 전용 램프 허용). 컨베이어는 비보행이라 보행 경고는 램프 배치 로그 소관.
	const float MaxSlopeStepZ = FMath::Max(1.0f, Grid->OJJ_GetMaxConveyorStepZ());

	OutCellZs.Reset();
	OutCellZs.Reserve(PathCells.Num());
	for (const FIntPoint& Cell : PathCells)
	{
		float CellSurfaceZ = 0.0f;
		if (!Grid->GetFoundationSurfaceZ(Cell, CellSurfaceZ))
		{
			OutReason = TEXT("Conveyor slope path must stay on foundations (mixed terrain/foundation path is not allowed).");
			return false;
		}
		OutCellZs.Add(CellSurfaceZ);
	}

	for (int32 Index = 1; Index < OutCellZs.Num(); ++Index)
	{
		if (FMath::Abs(OutCellZs[Index] - OutCellZs[Index - 1]) > MaxSlopeStepZ + KINDA_SMALL_NUMBER)
		{
			OutReason = FString::Printf(
				TEXT("Conveyor slope between adjacent cells exceeds %.0fuu (use a ramp path)."),
				MaxSlopeStepZ);
			return false;
		}
	}

	for (int32 Index = 1; Index + 1 < PathCells.Num(); ++Index)
	{
		const FIntPoint PrevDir = PathCells[Index] - PathCells[Index - 1];
		const FIntPoint NextDir = PathCells[Index + 1] - PathCells[Index];
		if (PrevDir != NextDir
			&& (!FMath::IsNearlyEqual(OutCellZs[Index], OutCellZs[Index - 1])
				|| !FMath::IsNearlyEqual(OutCellZs[Index + 1], OutCellZs[Index])))
		{
			OutReason = TEXT("Conveyor cannot turn on a slope (corner cells must be on flat surface).");
			return false;
		}
	}

	OutReason.Reset();
	return true;
}

bool OJJ_CollectConveyorReservedCells(
	const AOJJ_Grid* Grid,
	const TMap<FIntPoint, TWeakObjectPtr<AActor>>& OccupiedCells,
	const TMap<TWeakObjectPtr<AActor>, TArray<FIntPoint>>& ActorToCells,
	const TArray<FIntPoint>& PathCells,
	TArray<FIntPoint>& OutReservedCells,
	FString& OutReason,
	AMachineBase** OutSourceMachine = nullptr,
	AMachineBase** OutTargetMachine = nullptr)
{
	OutReservedCells.Reset();
	if (OutSourceMachine)
	{
		*OutSourceMachine = nullptr;
	}
	if (OutTargetMachine)
	{
		*OutTargetMachine = nullptr;
	}

	if (PathCells.Num() < 2)
	{
		OutReason = TEXT("Conveyor path must include the machine output and at least one outside cell.");
		return false;
	}

	AMachineBase* StartMachine = OJJ_GetMachineAtCell(OccupiedCells, PathCells[0]);
	const TArray<FIntPoint>* StartMachineCells = StartMachine ? ActorToCells.Find(StartMachine) : nullptr;
	// 포트 없는 머신(송전탑/발전소/차폐장 등)은 컨베이어 endpoint 불가 — 출력 포트 0이면 송신 불가.
	if (!StartMachine || !StartMachineCells
		|| StartMachine->GetOutputPortCount() <= 0
		|| !OJJ_IsMachineBackOutputPair(Grid, StartMachine, PathCells[0], PathCells[1], *StartMachineCells))
	{
		OutReason = TEXT("Conveyor must start from a machine output port.");
		return false;
	}

	AMachineBase* EndMachine = nullptr;
	bool bEndsOnMachine = false;
	if (!OJJ_FindInputMachineAtPathEnd(
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

		// 건설 게이트(F1-c: buildable OR Foundation) — 컨베이어는 CanPlaceMachine을 안 거치므로 여기서 직접 차단.
		if (!Grid->IsCellConstructible(Cell))
		{
			OutReason = TEXT("Conveyor path crosses unbuildable terrain (no foundation).");
			return false;
		}

		if (Index > 0 && OJJ_ManhattanDistance(PathCells[Index - 1], Cell) != 1)
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

	// 단일 건설면 규칙(F1-c §7-3, 경로판): 전 경로 셀(머신 끝점 포함)이 같은 높이여야 함.
	// F3.7-1(㊆): 균일 **실패 시에만** 경사 검사 — 기존 평면/지형 경로(균일 통과)는 이 분기에
	// 진입조차 안 해 회귀 0(f3_7 계획 §3-1). 경사 통과 = 램프 경로 예외 허용("F3에서 해소" 태그 이행).
	float UnusedUniformZ = 0.0f;
	if (!Grid->OJJ_GetUniformSurfaceZ(PathCells, UnusedUniformZ))
	{
		TArray<float> UnusedCellZs;
		if (!OJJ_ValidateConveyorSlopePath(Grid, PathCells, UnusedCellZs, OutReason))
		{
			return false; // OutReason은 경사 검사가 구체 사유로 세팅(한계 초과/혼합/경사 코너).
		}
	}

	if (OutSourceMachine)
	{
		*OutSourceMachine = StartMachine;
	}
	if (OutTargetMachine)
	{
		*OutTargetMachine = EndMachine;
	}

	OutReason.Reset();
	return true;
}
}  // namespace

AOJJ_Grid::AOJJ_Grid()
{
	PrimaryActorTick.bCanEverTick = true;
	// ★ AMachineBase::MeshFitCellWorld(=100)와 반드시 동기화 ★ — 머신 메시 바운즈 정규화가 이 값을 가정.
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
		// 베이스 캐싱 — 호버/오버레이/물 MID가 이 머티리얼(translucent Unlit M_OJJ_GridFloor 기반)에서 파생.
		HoverValidBaseMaterial = ValidHoverMat.Object;
		ValidHoverISM->SetMaterial(0, ValidHoverMat.Object);  // MID 생성 전 정적 폴백
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> InvalidHoverMat(
		TEXT("/Game/OJJ/Materials/MI_OJJ_GridHoverInvalid.MI_OJJ_GridHoverInvalid"));
	if (InvalidHoverMat.Succeeded())
	{
		HoverInvalidBaseMaterial = InvalidHoverMat.Object;
		InvalidHoverISM->SetMaterial(0, InvalidHoverMat.Object);  // MID 생성 전 정적 폴백
	}

	// 건설 가능(초록) per-cell 그리드 비주얼 — ValidHover와 동일 초록 재사용, void 제외해 바닥 모양 추종.
	BuildableCellISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BuildableCellISM"));
	BuildableCellISM->SetupAttachment(RootComponent);
	BuildableCellISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BuildableCellISM->SetCastShadow(false);
	if (PlaneMesh.Succeeded())
	{
		BuildableCellISM->SetStaticMesh(PlaneMesh.Object);
	}
	if (ValidHoverMat.Succeeded())
	{
		BuildableCellISM->SetMaterial(0, ValidHoverMat.Object);
	}

	// 건설 불가(빨강) per-cell 그리드 비주얼 — InvalidHover와 동일 빨강. blocked만(void 제외).
	BlockedCellISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BlockedCellISM"));
	BlockedCellISM->SetupAttachment(RootComponent);
	BlockedCellISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BlockedCellISM->SetCastShadow(false);
	if (PlaneMesh.Succeeded())
	{
		BlockedCellISM->SetStaticMesh(PlaneMesh.Object);
	}
	if (InvalidHoverMat.Succeeded())
	{
		BlockedCellISM->SetMaterial(0, InvalidHoverMat.Object);
	}

	// Foundation 커버 셀(초록 — constructible 기준, F3.5') per-cell 비주얼. 머티리얼은 EnsureTileMIDs에서
	// BuildableCellMID 공유(의미 동일 초록 — MID 1종 추가 없이).
	CoveredCellISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CoveredCellISM"));
	CoveredCellISM->SetupAttachment(RootComponent);
	CoveredCellISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CoveredCellISM->SetCastShadow(false);
	if (PlaneMesh.Succeeded())
	{
		CoveredCellISM->SetStaticMesh(PlaneMesh.Object);
	}
	if (ValidHoverMat.Succeeded())
	{
		CoveredCellISM->SetMaterial(0, ValidHoverMat.Object);
	}

	// 캐릭터 점유 셀(노랑) per-cell 비주얼(F2-4 후속 ②) — 빌드모드 중 BuildController가 구동. 시각 전용.
	CharacterCellISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CharacterCellISM"));
	CharacterCellISM->SetupAttachment(RootComponent);
	CharacterCellISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CharacterCellISM->SetCastShadow(false);
	if (PlaneMesh.Succeeded())
	{
		CharacterCellISM->SetStaticMesh(PlaneMesh.Object);
	}
	if (ValidHoverMat.Succeeded())
	{
		CharacterCellISM->SetMaterial(0, ValidHoverMat.Object);  // MID 생성 전 정적 폴백 — 색은 MID가 덮어씀
	}

	// 물(파랑) per-cell 비주얼 — ShowWater 디버그/빌드모드 오버레이. 머티리얼(파랑 MID)은 RefreshGridVisual에서 lazy 부여.
	WaterCellISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("WaterCellISM"));
	WaterCellISM->SetupAttachment(RootComponent);
	WaterCellISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WaterCellISM->SetCastShadow(false);
	if (PlaneMesh.Succeeded())
	{
		WaterCellISM->SetStaticMesh(PlaneMesh.Object);
	}

	// === 포트 방향 화살표 ISM (Cone — 엔진 기본, 전용 메시는 후속) ===
	// 배치 머신용 / 호버 프리뷰용을 분리해 수명주기를 독립. 색은 BeginPlay의 MID로 입힘.
	auto MakeArrowISM = [this](const TCHAR* Name) -> UInstancedStaticMeshComponent*
	{
		UInstancedStaticMeshComponent* ISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		ISM->SetupAttachment(RootComponent);
		ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ISM->SetCastShadow(false);
		return ISM;
	};
	PlacedInputArrowISM = MakeArrowISM(TEXT("PlacedInputArrowISM"));
	PlacedOutputArrowISM = MakeArrowISM(TEXT("PlacedOutputArrowISM"));
	HoverInputArrowISM = MakeArrowISM(TEXT("HoverInputArrowISM"));
	HoverOutputArrowISM = MakeArrowISM(TEXT("HoverOutputArrowISM"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeMesh.Succeeded())
	{
		PlacedInputArrowISM->SetStaticMesh(ConeMesh.Object);
		PlacedOutputArrowISM->SetStaticMesh(ConeMesh.Object);
		HoverInputArrowISM->SetStaticMesh(ConeMesh.Object);
		HoverOutputArrowISM->SetStaticMesh(ConeMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ArrowMat(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ArrowMat.Succeeded())
	{
		ArrowBaseMaterial = ArrowMat.Object;
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

		// 센터 기준 그리드: Plane은 본래 액터 중심에 위치하므로 XY 오프셋 0이면 원점 중심에 정렬.
		// (기존 +half 오프셋 제거 — 그리드가 사방으로 자라므로 커서 충돌 플레인도 원점 중심.) Z=1 Z-fighting 방지.
		GridFloorMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f));
	}

	// 포트 화살표 틴트 동적 머티리얼 — 입력=파랑 계열, 출력=주황 계열.
	// ⚠️ BasicShapeMaterial이 "Color" 파라미터를 노출하지 않으면 기본색(회색)으로 표시된다.
	//    그래도 입력/출력은 위치(입력측/출력측)와 방향(들어옴/나감)으로 구분되므로 기능상 식별 가능.
	//    후속: 전용 OJJ MI 에셋(MI_OJJ_PortArrowInput/Output) 작성 후 교체 + 색 미세조정(PIE 확인 후).
	if (ArrowBaseMaterial)
	{
		InputArrowMID = UMaterialInstanceDynamic::Create(ArrowBaseMaterial, this);
		OutputArrowMID = UMaterialInstanceDynamic::Create(ArrowBaseMaterial, this);
		if (InputArrowMID)
		{
			InputArrowMID->SetFlags(RF_Transient); // 레벨 dirty(가짜 diff) 차단 — EnsureTileMIDs와 동일(F2-0)
			InputArrowMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.1f, 0.4f, 1.0f));
			if (PlacedInputArrowISM) PlacedInputArrowISM->SetMaterial(0, InputArrowMID);
			if (HoverInputArrowISM) HoverInputArrowISM->SetMaterial(0, InputArrowMID);
		}
		if (OutputArrowMID)
		{
			OutputArrowMID->SetFlags(RF_Transient);
			OutputArrowMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0f, 0.45f, 0.05f));
			if (PlacedOutputArrowISM) PlacedOutputArrowISM->SetMaterial(0, OutputArrowMID);
			if (HoverOutputArrowISM) HoverOutputArrowISM->SetMaterial(0, OutputArrowMID);
		}
	}

	// 타일 전용 MID(호버/오버레이/물) lazy 생성 + 현재 시각 위계 값 적용 — 색 섞임 방지 위해 공유 MI 분리.
	OJJ_EnsureTileMIDs();

	// 정적 지형 높낮이 건설 제약 — 사전베이크 캐시가 유효하면 로드만(런타임 트레이스 0), 없으면/불일치면 트레이스 폴백.
	// 오버레이는 빌드모드 진입 시 표시되므로 여기선 채우지 않음.
	if (!TryLoadBakeCache())
	{
		BakeBuildableCells(/*bVerbose=*/false, /*bWriteCache=*/false);
	}
}

void AOJJ_Grid::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

FIntPoint AOJJ_Grid::WorldToGrid(FVector WorldPos) const
{
	const FVector Local = WorldPos - GetActorLocation();
	// 센터 기준 역변환: GridToWorld와 동일한 0.5*GridSize 오프셋을 더해 인덱스화. 두 함수가 같은 항을
	// 공유하므로 round-trip 정확(Floor(c+0.5)=c) — 홀수 GridSize도 반 칸 어긋남 없음.
	const int32 X = FMath::FloorToInt(Local.X / CellSize + GridSize.X * 0.5f);
	const int32 Y = FMath::FloorToInt(Local.Y / CellSize + GridSize.Y * 0.5f);
	return FIntPoint(X, Y);
}

FVector AOJJ_Grid::GridToWorld(FIntPoint Coord) const
{
	const FVector Origin = GetActorLocation();
	// 센터 기준: 인덱스 공간 [0,N)을 원점 중심으로 매핑(− 절반 extent). WorldToGrid와 0.5*GridSize 항 공유.
	// 홀수 N → 중앙셀이 원점에 안착 / 짝수 N → 원점이 셀 경계. 둘 다 round-trip 정확.
	const float HalfExtentX = GridSize.X * CellSize * 0.5f;
	const float HalfExtentY = GridSize.Y * CellSize * 0.5f;
	const float WorldX = Origin.X + (Coord.X * CellSize) + (CellSize * 0.5f) - HalfExtentX;
	const float WorldY = Origin.Y + (Coord.Y * CellSize) + (CellSize * 0.5f) - HalfExtentY;
	return FVector(WorldX, WorldY, Origin.Z);
}

FVector AOJJ_Grid::GetGridCenter() const
{
	// 센터 기준에선 placement extent의 정중앙이 곧 액터 원점.
	return GetActorLocation();
}

FIntPoint AOJJ_Grid::EffectiveSize(FVector2D RawSize, int32 RotationSteps)
{
	// CalculateFootprint / GetMachinePlacementLocation과 동일한 정수화 규칙(CeilToInt + Max 1).
	const int32 X = FMath::Max(1, FMath::CeilToInt(RawSize.X));
	const int32 Y = FMath::Max(1, FMath::CeilToInt(RawSize.Y));

	// 90°/270°(홀수 step)에서 치수 swap. 음수 step도 parity로 정상 동작.
	return ((RotationSteps % 2) != 0) ? FIntPoint(Y, X) : FIntPoint(X, Y);
}

FIntPoint AOJJ_Grid::OJJ_OriginFromCursorCellForSize(FIntPoint CursorCell, FIntPoint EffSize)
{
	// (Size-1)/2 정수 나눗셈 → lower-left bias. 1x1 offset 0. BuildController
	// ComputeOriginFromCursorCellForSize에서 본문 이관(F3.6-0) — 호출처 정책 주석은 컨트롤러 측 참조.
	return FIntPoint(CursorCell.X - (EffSize.X - 1) / 2, CursorCell.Y - (EffSize.Y - 1) / 2);
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

	// Z: 피벗 무관 "바닥 안착". 메시 AABB의 최저점이 그리드 평면(LowerLeftCenter.Z)에 닿도록
	// 액터 Z를 보정한다. 메시 로컬 AABB를 "MeshComponent→Actor" 상대 트랜스폼으로 변환해 액터
	// 기준 최저점(ActorSpaceBox.Min.Z)을 구하므로, 컴포넌트의 상대 위치·회전·스케일(음수 포함)을
	// 모두 반영한다(TransformBy가 변환 후 AABB를 재산출). ZOffset = -ActorSpaceBox.Min.Z.
	//   · 바닥 피벗 메시(상대 transform 항등, Min.Z≈0): 보정 0 → 기존 동작과 동일(회귀 없음).
	//   · 중앙 피벗(Min.Z<0)은 위로, 상단 피벗(Min.Z>0)은 아래로 옮겨 AABB 바닥을 평면에 안착.
	// 메시 미지정/널이면 보정 0(현행 유지). GetMachinePlacementLocation은 항상 스폰된 실제
	// 인스턴스로만 호출되므로(호버는 평면 ISM 타일이라 이 함수 미사용) 인스턴스 MeshComponent 기준.
	float ZOffset = 0.0f;
	if (const UStaticMeshComponent* Mesh = Machine->GetMeshComponent())
	{
		if (const UStaticMesh* MeshAsset = Mesh->GetStaticMesh())
		{
			const FTransform CompToActor =
				Mesh->GetComponentTransform().GetRelativeTransform(Machine->GetActorTransform());
			const FBox ActorSpaceBox = MeshAsset->GetBoundingBox().TransformBy(CompToActor);
			ZOffset = -ActorSpaceBox.Min.Z;
		}
	}

	// F1-c: 바닥 기준면 = 풋프린트의 단일 건설면(Foundation 상면 또는 평면). 걸침은 CanPlaceMachine이
	// 사전 거부하므로 여기 도달 시 균일 보장 — 그래도 실패하면 평면 폴백(방어, 기존 동작과 동일).
	float BaseZ = LowerLeftCenter.Z;
	float UniformZ = 0.0f;
	if (OJJ_GetUniformSurfaceZ(CalculateFootprint(Machine, Origin, RotationSteps), UniformZ))
	{
		BaseZ = UniformZ;
	}

	return FVector(LowerLeftCenter.X + OffsetX, LowerLeftCenter.Y + OffsetY, BaseZ + ZOffset);
}

bool AOJJ_Grid::IsValidGridCell(FIntPoint Cell) const
{
	return Cell.X >= 0 && Cell.X < GridSize.X
		&& Cell.Y >= 0 && Cell.Y < GridSize.Y;
}

bool AOJJ_Grid::IsCellBuildable(FIntPoint Cell) const
{
	// blocked(높이초과)·void(바닥없음)·water(물) 중 하나라도면 불가. water 건설금지 불변식(§3) — 별도 태그지만 여기서 게이트.
	// 베이크 전이면 세 집합 모두 비어 전부 가능(기존 흐름 무영향).
	return !UnbuildableCells.Contains(Cell) && !VoidCells.Contains(Cell) && !WaterCells.Contains(Cell);
}

bool AOJJ_Grid::IsCellConstructible(FIntPoint Cell) const
{
	// OR 게이트(F1-c §7-3): 지형이 가능하거나 Foundation이 커버하면 건설 허용. OR은 허용 집합을
	// 넓히기만 하므로 기존 지형 직배치(추출기 포함)는 전부 그대로 통과(§5-2 F1~F2 직배치 유지).
	return IsCellBuildable(Cell) || IsCellOnFoundation(Cell);
}

bool AOJJ_Grid::IsCellVoid(FIntPoint Cell) const
{
	return VoidCells.Contains(Cell);
}

bool AOJJ_Grid::IsCellWater(FIntPoint Cell) const
{
	return WaterCells.Contains(Cell);
}

void AOJJ_Grid::BakeBuildableCells(bool bVerbose, bool bWriteCache)
{
	UnbuildableCells.Reset();
	VoidCells.Reset();
	WaterCells.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const int32 NumCells = GridSize.X * GridSize.Y;
	if (NumCells <= 0)
	{
		return;
	}

	const FVector GridOrigin = GetActorLocation();
	const float PlaneZ = GridOrigin.Z;

	// 분류 임시 버퍼(선형 인덱스 = X*GridSize.Y + Y). flood-fill 필터까지 끝낸 뒤 TSet/패킹으로 한 번에 커밋.
	// WouldBlockTmp: water 필터 환원 시 "원래 blocked였는지"로 blocked/buildable 되돌림 판정.
	TArray<uint8> ClassTmp;      ClassTmp.Init((uint8)EOJJCellClass::Buildable, NumCells);
	TArray<uint8> WouldBlockTmp; WouldBlockTmp.Init(0, NumCells);
	TArray<int16> GroundZTmp;
	if (bBakeGroundHeights) { GroundZTmp.Init(0, NumCells); }

	// verbose 진단: 평탄(바닥) 외 셀만 로그(스팸 방지 캡). 큐브 등 베이크 오판 원인 추적용.
	int32 VerboseLogged = 0;
	const int32 MaxVerboseLines = 400;
	if (bVerbose)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[Grid] Bake VERBOSE: PlaneZ=%.1f, tol=%.1f, waterZ=%.1f, startH=%.1f, depth=%.1f, trace=%d (평탄 외 셀만, 최대 %d줄)"),
			PlaneZ, BuildableHeightTolerance, WaterSurfaceZ, BuildableTraceStartHeight, BuildableTraceDepth,
			(int32)BuildableTraceChannel.GetValue(), MaxVerboseLines);
	}

	// 트레이스 견고화: 그리드 자신(floor/호버 ISM) + 셀 위에 서 있는 게임 액터(머신·컨베이어·자원)를 무시 →
	// 지형 대신 머신/광맥 메시에 걸려 false-block 되는 것 방지. 책임 분리: 자원은 "트레이스 ignore"하고
	// 점유 등록(OJJ_RegisterActorCells)으로 건설 차단(베이크 높이 판정과 이중처리해도 같은 방향이라 안전).
	// ※ 한계: ECC_Visibility 채널 의존 — 지형 메시가 이 채널을 Block해야 함. 전용 채널 필요 시 BuildableTraceChannel 조정.
	FCollisionQueryParams Params(FName(TEXT("GridBuildableBake")), /*bTraceComplex=*/false, this);
	for (TActorIterator<AMachineBase> It(World); It; ++It) { Params.AddIgnoredActor(*It); }
	for (TActorIterator<AConveyor> It(World); It; ++It) { Params.AddIgnoredActor(*It); }
	for (TActorIterator<AResourceBase> It(World); It; ++It) { Params.AddIgnoredActor(*It); }
	// F1-c: Foundation 슬래브가 Visibility를 Block(커서 스냅용)하므로 베이크 ↓트레이스에서 제외 —
	// 지형 분류가 슬래브 상면을 지형으로 오인하지 않게(향후 레벨 사전배치 대비 — WaterArea 이중 안전 패턴).
	for (TActorIterator<AOJJ_Foundation> It(World); It; ++It) { Params.AddIgnoredActor(*It); }

	const double StartTime = FPlatformTime::Seconds();
	for (int32 X = 0; X < GridSize.X; ++X)
	{
		for (int32 Y = 0; Y < GridSize.Y; ++Y)
		{
			const int32 Idx = X * GridSize.Y + Y;
			const FVector Center = GridToWorld(FIntPoint(X, Y));

			// 셀당 5점 샘플링(중심 + 4귀퉁이 ±0.4셀) — 큐브가 셀 중심을 안 밟아도 귀퉁이로 검출.
			// 하나라도 |델타| > tol이면 blocked. void는 5점 전부 미히트일 때만(바닥 전무). water는 5점 중 최저 델타 기준.
			const float S = CellSize * 0.4f;
			const FVector2D SampleOffsets[5] = {
				FVector2D(0.f, 0.f), FVector2D(S, S), FVector2D(S, -S), FVector2D(-S, S), FVector2D(-S, -S) };

			bool bAnyHit = false;
			float WorstAbsDelta = 0.0f;
			float WorstSignedDelta = 0.0f;                          // verbose/분류 — 최악점의 부호 델타
			float WorstHitZ = 0.0f;                                 // verbose
			float HighestSignedDelta = -TNumericLimits<float>::Max(); // groundZ 대표값 — 5점 중 최고점(F2-1 결정 ①)
			float LowestSignedDelta = TNumericLimits<float>::Max(); // water 판정 — 5점 중 최저(가장 깊은) 평면대비 델타
			for (const FVector2D& Off : SampleOffsets)
			{
				const FVector TraceStart(Center.X + Off.X, Center.Y + Off.Y, PlaneZ + BuildableTraceStartHeight);
				const FVector TraceEnd(Center.X + Off.X, Center.Y + Off.Y, PlaneZ - BuildableTraceDepth);
				FHitResult Hit;
				if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, BuildableTraceChannel, Params))
				{
					bAnyHit = true;
					const float Delta = Hit.ImpactPoint.Z - PlaneZ;
					if (FMath::Abs(Delta) > WorstAbsDelta)
					{
						WorstAbsDelta = FMath::Abs(Delta);
						WorstSignedDelta = Delta;
						WorstHitZ = Hit.ImpactPoint.Z;
					}
					HighestSignedDelta = FMath::Max(HighestSignedDelta, Delta);
					LowestSignedDelta = FMath::Min(LowestSignedDelta, Delta);
				}
			}

			// 4단 분류 (우선순위: void > water > blocked > buildable). water는 셀 최저점이 WaterSurfaceZ보다 깊을 때 —
			// blocked보다 먼저 태그(둘 다 건설 불가지만 파랑 표시/수원 후보 우선). WouldBlock은 필터 환원용으로 별도 보존.
			const bool bWouldBlock = bAnyHit && (WorstAbsDelta > BuildableHeightTolerance);
			const bool bWater = bAnyHit && (LowestSignedDelta < WaterSurfaceZ);
			EOJJCellClass CellClass;
			if (!bAnyHit)         { CellClass = EOJJCellClass::Void; }
			else if (bWater)      { CellClass = EOJJCellClass::Water; }
			else if (bWouldBlock) { CellClass = EOJJCellClass::Blocked; }
			else                  { CellClass = EOJJCellClass::Buildable; }

			ClassTmp[Idx] = (uint8)CellClass;
			WouldBlockTmp[Idx] = bWouldBlock ? 1 : 0;
			if (bBakeGroundHeights)
			{
				// 셀 대표높이 = 5점 최고점(F2-1 결정 ① — 직배치가 메시 위에 안착, 묻힘 0). 분류(blocked)는
				// 계속 최악점(|델타| 최대) — 구덩이/절벽 셀이 buildable로 풀리지 않게 분리 유지.
				// 절대높이 = ActorLocation.Z + 값. 평탄셀=0, 미히트(void)=0.
				GroundZTmp[Idx] = (int16)FMath::Clamp(FMath::RoundToInt(bAnyHit ? HighestSignedDelta : 0.0f), -32768, 32767);
			}

			// verbose: 평탄 바닥(델타≈0) 외 셀만 출력 — 5점 중 최악/최저값 기준.
			if (bVerbose && VerboseLogged < MaxVerboseLines && (!bAnyHit || WorstAbsDelta > 1.0f || bWater))
			{
				const TCHAR* Cls = !bAnyHit ? TEXT("void")
					: (bWater ? TEXT("WATER") : (bWouldBlock ? TEXT("BLOCKED") : TEXT("buildable")));
				UE_LOG(LogTemp, Log,
					TEXT("[Grid]   cell(%d,%d) anyHit=%d worstZ=%.1f worstDelta=%+.1f hiDelta=%+.1f lowDelta=%+.1f (5pt) -> %s"),
					X, Y, bAnyHit ? 1 : 0, bAnyHit ? WorstHitZ : 0.0f, WorstSignedDelta,
					bAnyHit ? HighestSignedDelta : 0.0f, bAnyHit ? LowestSignedDelta : 0.0f, Cls);
				++VerboseLogged;
			}
		}
	}

	// 잔웅덩이 필터: 연결된(4-이웃) water 영역의 셀 수 < MinWaterCellCount면 일반 지형으로 환원(blocked/buildable).
	// BFS는 Region을 큐로 재사용(Pop 시그니처 버전차/리얼로케이션 회피). 0/1이면 필터 없음.
	int32 WaterFilteredRegions = 0;
	int32 WaterFilteredCells = 0;
	if (MinWaterCellCount > 1)
	{
		TArray<uint8> Visited; Visited.Init(0, NumCells);
		TArray<int32> Region;
		for (int32 Seed = 0; Seed < NumCells; ++Seed)
		{
			if (Visited[Seed] || ClassTmp[Seed] != (uint8)EOJJCellClass::Water) { continue; }
			Region.Reset();
			Region.Add(Seed);
			Visited[Seed] = 1;
			for (int32 Head = 0; Head < Region.Num(); ++Head)
			{
				const int32 Cur = Region[Head];
				const int32 CX = Cur / GridSize.Y;
				const int32 CY = Cur % GridSize.Y;
				const FIntPoint Neighbors[4] = {
					FIntPoint(CX + 1, CY), FIntPoint(CX - 1, CY), FIntPoint(CX, CY + 1), FIntPoint(CX, CY - 1) };
				for (const FIntPoint& N : Neighbors)
				{
					if (N.X < 0 || N.X >= GridSize.X || N.Y < 0 || N.Y >= GridSize.Y) { continue; }
					const int32 NIdx = N.X * GridSize.Y + N.Y;
					if (!Visited[NIdx] && ClassTmp[NIdx] == (uint8)EOJJCellClass::Water)
					{
						Visited[NIdx] = 1;
						Region.Add(NIdx);
					}
				}
			}
			if (Region.Num() < MinWaterCellCount)
			{
				for (const int32 RIdx : Region)
				{
					ClassTmp[RIdx] = WouldBlockTmp[RIdx] ? (uint8)EOJJCellClass::Blocked : (uint8)EOJJCellClass::Buildable;
				}
				++WaterFilteredRegions;
				WaterFilteredCells += Region.Num();
			}
		}
	}

	// 임시 분류 → 런타임 TSet 커밋. Buildable은 미저장(기존 규약).
	for (int32 X = 0; X < GridSize.X; ++X)
	{
		for (int32 Y = 0; Y < GridSize.Y; ++Y)
		{
			switch ((EOJJCellClass)ClassTmp[X * GridSize.Y + Y])
			{
			case EOJJCellClass::Blocked: UnbuildableCells.Add(FIntPoint(X, Y)); break;
			case EOJJCellClass::Void:    VoidCells.Add(FIntPoint(X, Y)); break;
			case EOJJCellClass::Water:   WaterCells.Add(FIntPoint(X, Y)); break;
			default: break;
			}
		}
	}

	bBuildableBaked = true;

	// 패킹 캐시 기록(에디터 RebakeAndCache 경로). 시그니처는 BeginPlay 로드 시 정합 검증에 사용.
	if (bWriteCache)
	{
		PackedCellClasses.Init(0, (NumCells + 3) / 4);
		for (int32 Idx = 0; Idx < NumCells; ++Idx)
		{
			OJJ_SetPackedClass(Idx, (EOJJCellClass)ClassTmp[Idx]);
		}
		if (bBakeGroundHeights) { CellGroundZQuant = MoveTemp(GroundZTmp); }
		else { CellGroundZQuant.Reset(); }

		CacheGridSize = GridSize;
		CacheCellSize = CellSize;
		CacheGridOrigin = GridOrigin;
		CacheHeightTolerance = BuildableHeightTolerance;
		CacheWaterSurfaceZ = WaterSurfaceZ;
		CacheMinWaterCellCount = MinWaterCellCount;
		CacheBakeVersion = OJJ_CurrentBakeVersion;
		CacheTraceStartHeight = BuildableTraceStartHeight;
		CacheTraceDepth = BuildableTraceDepth;
		CacheTraceChannel = BuildableTraceChannel;
		bCacheBakeGroundHeights = bBakeGroundHeights;
		bHasBakeCache = true;
	}

	const double ElapsedMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;
	const int32 Blocked = UnbuildableCells.Num();
	const int32 Void = VoidCells.Num();
	const int32 Water = WaterCells.Num();
	const int32 Buildable = NumCells - Blocked - Void - Water;
	UE_LOG(LogTemp, Log,
		TEXT("[Grid] Bake: buildable %d / blocked %d / void %d / water %d (total %d, GridSize %dx%d, trace=%d, tol=%.0f, waterZ=%.0f, minWater=%d) in %.1f ms%s"),
		Buildable, Blocked, Void, Water, NumCells, GridSize.X, GridSize.Y, (int32)BuildableTraceChannel.GetValue(),
		BuildableHeightTolerance, WaterSurfaceZ, MinWaterCellCount, ElapsedMs, bWriteCache ? TEXT(" [cached]") : TEXT(""));

	if (WaterFilteredRegions > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[Grid] Bake: water 잔웅덩이 필터 — %d개 영역(%d셀) 환원 (MinWaterCellCount=%d)."),
			WaterFilteredRegions, WaterFilteredCells, MinWaterCellCount);
	}

	// 사고 조기 발견: 건설 가능 셀이 0이면 트레이스 채널/시작높이/기준면 Z/그리드 위치 의심.
	if (NumCells > 0 && Buildable == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Grid] Bake 이상치: 건설 가능 셀 0 (blocked %d / void %d / water %d) — 트레이스 채널/시작높이/기준면 Z/그리드 위치 확인 요망."),
			Blocked, Void, Water);
	}
}

// === 사전베이크 캐시 직렬화 ===

int32 AOJJ_Grid::OJJ_CellLinearIndex(FIntPoint Cell, FIntPoint GridSz)
{
	return Cell.X * GridSz.Y + Cell.Y;
}

EOJJCellClass AOJJ_Grid::OJJ_GetPackedClass(int32 LinearIdx) const
{
	const int32 ByteIdx = LinearIdx >> 2;
	if (!PackedCellClasses.IsValidIndex(ByteIdx))
	{
		return EOJJCellClass::Buildable;  // 범위 밖 = 안전 기본(건설 가능)
	}
	const int32 Shift = (LinearIdx & 3) * 2;
	return (EOJJCellClass)((PackedCellClasses[ByteIdx] >> Shift) & 0x3);
}

void AOJJ_Grid::OJJ_SetPackedClass(int32 LinearIdx, EOJJCellClass CellClass)
{
	const int32 ByteIdx = LinearIdx >> 2;
	if (!PackedCellClasses.IsValidIndex(ByteIdx))
	{
		return;
	}
	const int32 Shift = (LinearIdx & 3) * 2;
	const uint8 Cleared = (uint8)(PackedCellClasses[ByteIdx] & ~(0x3u << Shift));
	PackedCellClasses[ByteIdx] = (uint8)(Cleared | (((uint8)CellClass & 0x3u) << Shift));
}

void AOJJ_Grid::OJJ_GetBakeCacheSignatureMatch(bool& bOutStructMatch, bool& bOutParamMatch) const
{
	// 시그니처 정합 — 구조(GridSize/CellSize/Origin)는 패킹 인덱싱 정확성, 분류 파라미터(tol/waterZ/minWater)는 결과 정확성.
	// TryLoadBakeCache(분류)와 OJJ_HasValidGroundZData(높이)가 공유하는 단일원 — 두 경로의 무효화 기준이 갈라지지 않게.
	bOutStructMatch =
		CacheGridSize == GridSize &&
		FMath::IsNearlyEqual(CacheCellSize, CellSize) &&
		GetActorLocation().Equals(CacheGridOrigin, 1.0f);
	bOutParamMatch =
		FMath::IsNearlyEqual(CacheHeightTolerance, BuildableHeightTolerance) &&
		FMath::IsNearlyEqual(CacheWaterSurfaceZ, WaterSurfaceZ) &&
		CacheMinWaterCellCount == MinWaterCellCount &&
		FMath::IsNearlyEqual(CacheTraceStartHeight, BuildableTraceStartHeight) &&
		FMath::IsNearlyEqual(CacheTraceDepth, BuildableTraceDepth) &&
		CacheTraceChannel.GetValue() == BuildableTraceChannel.GetValue() &&
		bCacheBakeGroundHeights == bBakeGroundHeights &&
		// 산식 버전(F2-1 결정 ②): 파라미터 동일해도 베이크 의미가 바뀐 옛 캐시(0/1) 자동 무효화.
		CacheBakeVersion == OJJ_CurrentBakeVersion;
}

bool AOJJ_Grid::TryLoadBakeCache()
{
	if (!bHasBakeCache)
	{
		return false;
	}

	// 하나라도 불일치면 캐시 무효 → 트레이스 폴백. 에디터에서 Rebake 누르면 캐시 갱신.
	bool bStructMatch = false;
	bool bParamMatch = false;
	OJJ_GetBakeCacheSignatureMatch(bStructMatch, bParamMatch);

	if (!bStructMatch || !bParamMatch)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Grid] Bake 캐시 무효(시그니처 불일치: struct=%d param=%d) — 재트레이스 폴백. 에디터에서 Rebake로 캐시 갱신 요망."),
			bStructMatch ? 1 : 0, bParamMatch ? 1 : 0);
		return false;
	}

	const int32 NumCells = GridSize.X * GridSize.Y;
	const int32 ExpectedBytes = (NumCells + 3) / 4;
	if (NumCells <= 0 || PackedCellClasses.Num() != ExpectedBytes)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Grid] Bake 캐시 크기 불일치(packed %d != expected %d, cells %d) — 재트레이스 폴백."),
			PackedCellClasses.Num(), ExpectedBytes, NumCells);
		return false;
	}

	UnbuildableCells.Reset();
	VoidCells.Reset();
	WaterCells.Reset();
	for (int32 X = 0; X < GridSize.X; ++X)
	{
		for (int32 Y = 0; Y < GridSize.Y; ++Y)
		{
			switch (OJJ_GetPackedClass(X * GridSize.Y + Y))
			{
			case EOJJCellClass::Blocked: UnbuildableCells.Add(FIntPoint(X, Y)); break;
			case EOJJCellClass::Void:    VoidCells.Add(FIntPoint(X, Y)); break;
			case EOJJCellClass::Water:   WaterCells.Add(FIntPoint(X, Y)); break;
			default: break;
			}
		}
	}

	bBuildableBaked = true;
	UE_LOG(LogTemp, Log,
		TEXT("[Grid] Bake 캐시 로드(트레이스 0): buildable %d / blocked %d / void %d / water %d (total %d, GridSize %dx%d)."),
		NumCells - UnbuildableCells.Num() - VoidCells.Num() - WaterCells.Num(),
		UnbuildableCells.Num(), VoidCells.Num(), WaterCells.Num(), NumCells, GridSize.X, GridSize.Y);
	return true;
}

void AOJJ_Grid::RebakeAndCache()
{
	// 에디터 버튼: 트레이스 1회 + 패킹 캐시 저장. 즉시 오버레이(blocked+water)로 분포 확인.
	BakeBuildableCells(/*bVerbose=*/false, /*bWriteCache=*/true);

	// 대형 맵 가드: 오버레이(blocked+water) 인스턴스가 임계 초과면 표시 생략. 에디터에서 수만 개 ISM
	// 인스턴스를 한 프레임에 올리면 게임스레드가 멈추고 가상메모리가 폭주해 OOM(Meadows 300²,
	// 84,679개에서 2회 재현 — 페이징 파일 고갈). 분포 확인은 Bake 요약 로그로 대체.
	// 플래그는 베이크마다 재계산되므로 작은 맵에선 기존처럼 즉시 표시(회귀 없음).
	constexpr int32 MaxEditorOverlayInstances = 20000;
	const int32 OverlayCount = UnbuildableCells.Num() + WaterCells.Num();
	const bool bShowOverlay = OverlayCount <= MaxEditorOverlayInstances;
	bForceShowBlocked = bShowOverlay;
	bForceShowWater = bShowOverlay;
	RefreshGridVisual();  // 생략 시에도 호출 — 이전 오버레이 잔존분 클리어(ClearInstances)
	if (!bShowOverlay)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Grid] RebakeAndCache: 오버레이 %d개 > 임계 %d — 에디터 표시 생략(분포는 위 Bake 로그 참조)."),
			OverlayCount, MaxEditorOverlayInstances);
	}

#if WITH_EDITOR
	Modify();
	MarkPackageDirty();
#endif

	UE_LOG(LogTemp, Log,
		TEXT("[Grid] RebakeAndCache 완료 — 캐시 저장됨(water %d, GroundZ %s). ⚠️ 레벨을 저장해야 .umap에 영속됩니다."),
		WaterCells.Num(), bBakeGroundHeights ? TEXT("on") : TEXT("off"));
}

void AOJJ_Grid::RefreshGridVisual()
{
	// 타일 MID(오버레이 초록/빨강/파랑 + 호버) lazy 생성 + 현재 시각 위계 값 적용.
	// 에디터 Rebake/PIE 공용(BeginPlay에 의존하지 않음). 호버와 분리된 전용 MID라 색 섞임 없음.
	OJJ_EnsureTileMIDs();

	// 클리어 후 재적재 — 빌드모드 진입/퇴장 반복에도 인스턴스 중복·잔존 방지(단일 진실원).
	// 부분 갱신 장부(F3.5')도 함께 리셋 — bVisualizationActive 경로가 재구축, 그 외엔 빈 장부 = 이벤트 no-op.
	if (BuildableCellISM) { BuildableCellISM->ClearInstances(); }
	if (BlockedCellISM) { BlockedCellISM->ClearInstances(); }
	if (WaterCellISM) { WaterCellISM->ClearInstances(); }
	if (CoveredCellISM) { CoveredCellISM->ClearInstances(); }
	BlockedCellToInstance.Reset();
	CoveredCellToInstance.Reset();

	// 셀 중심 → 인스턴스 트랜스폼. 기준면 +3(호버 프리뷰 +2보다 위 — z-fighting 방지).
	// F1-c: 기준면 = 셀 비주얼 Z(지형 GroundZ/Foundation 상면 추종) — 굴곡 지형 묻힘 해결.
	// GroundZ 유효성(시그니처 비교)은 셀 불변 → 90k셀 루프 밖으로 호이스팅(Codex F1-c #5).
	const bool bGroundZValid = OJJ_HasValidGroundZData();
	auto MakeCellXform = [this, bGroundZValid](const FIntPoint& Cell) -> FTransform
	{
		return OJJ_MakeOverlayCellTransform(Cell, bGroundZValid);
	};

	if (bVisualizationActive)
	{
		// 빌드모드: 전 셀을 water(파랑)/blocked(빨강)/가능(초록)으로 채움. void는 모두 제외 → 그리드가 바닥 모양만 따라 보임.
		// 우선순위 water > blocked: water도 건설 불가지만 파랑으로 구분 표시(분류 우선순위와 일치).
		// F3.5': 커버된 blocked는 초록(CoveredCellISM — constructible 기준, 색=의미). blocked/covered는
		// 부분 갱신 대상이라 장부(셀↔인스턴스)를 적재 순서로 병행 구축.
		TArray<FTransform> GreenXforms;
		TArray<FTransform> RedXforms;
		TArray<FTransform> BlueXforms;
		TArray<FTransform> CoveredXforms;
		TArray<FIntPoint> BlockedCellsInOrder;
		TArray<FIntPoint> CoveredCellsInOrder;
		for (int32 X = 0; X < GridSize.X; ++X)
		{
			for (int32 Y = 0; Y < GridSize.Y; ++Y)
			{
				const FIntPoint Cell(X, Y);
				if (VoidCells.Contains(Cell)) { continue; }                  // void → 아무것도 안 그림
				if (WaterCells.Contains(Cell)) { BlueXforms.Add(MakeCellXform(Cell)); }
				else if (UnbuildableCells.Contains(Cell))
				{
					if (IsCellOnFoundation(Cell))
					{
						CoveredXforms.Add(MakeCellXform(Cell));
						CoveredCellsInOrder.Add(Cell);
					}
					else
					{
						RedXforms.Add(MakeCellXform(Cell));
						BlockedCellsInOrder.Add(Cell);
					}
				}
				else { GreenXforms.Add(MakeCellXform(Cell)); }
			}
		}
		if (BuildableCellISM && GreenXforms.Num() > 0) { BuildableCellISM->AddInstances(GreenXforms, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/true); }
		if (BlockedCellISM && RedXforms.Num() > 0) { BlockedCellISM->AddInstances(RedXforms, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/true); }
		if (WaterCellISM && BlueXforms.Num() > 0) { WaterCellISM->AddInstances(BlueXforms, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/true); }
		if (CoveredCellISM && CoveredXforms.Num() > 0) { CoveredCellISM->AddInstances(CoveredXforms, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/true); }
		// 배치 적재는 배열 순서 = 인스턴스 인덱스 — 적재 순서 배열에서 장부 구축(이후 인덱스 불변 — 숨김 방식).
		for (int32 Idx = 0; Idx < BlockedCellsInOrder.Num(); ++Idx) { BlockedCellToInstance.Add(BlockedCellsInOrder[Idx], Idx); }
		for (int32 Idx = 0; Idx < CoveredCellsInOrder.Num(); ++Idx) { CoveredCellToInstance.Add(CoveredCellsInOrder[Idx], Idx); }
	}
	else
	{
		// 디버그(빌드모드 밖): 토글된 분류만 표시. ShowBlocked=빨강, ShowWater=파랑(독립).
		if (bForceShowBlocked)
		{
			TArray<FTransform> RedXforms;
			for (const FIntPoint& Cell : UnbuildableCells) { RedXforms.Add(MakeCellXform(Cell)); }
			if (BlockedCellISM && RedXforms.Num() > 0) { BlockedCellISM->AddInstances(RedXforms, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/true); }
		}
		if (bForceShowWater)
		{
			TArray<FTransform> BlueXforms;
			for (const FIntPoint& Cell : WaterCells) { BlueXforms.Add(MakeCellXform(Cell)); }
			if (WaterCellISM && BlueXforms.Num() > 0) { WaterCellISM->AddInstances(BlueXforms, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/true); }
		}
	}
}

FTransform AOJJ_Grid::OJJ_MakeOverlayCellTransform(FIntPoint Cell, bool bGroundZValid) const
{
	const FVector C = GridToWorld(Cell);
	return FTransform(
		FRotator::ZeroRotator,
		FVector(C.X, C.Y, OJJ_GetCellVisualBaseZInternal(Cell, bGroundZValid) + 3.0f),
		FVector(CellSize / 100.0f, CellSize / 100.0f, 1.0f));
}

void AOJJ_Grid::OJJ_ShowOverlayInstance(UInstancedStaticMeshComponent* ISM, TMap<FIntPoint, int32>& CellToInstance,
	FIntPoint Cell, bool bGroundZValid)
{
	if (!ISM)
	{
		return;
	}
	const FTransform Xform = OJJ_MakeOverlayCellTransform(Cell, bGroundZValid);
	if (const int32* FoundIdx = CellToInstance.Find(Cell))
	{
		// 숨김(zero-scale) 인스턴스 재활용 — 인덱스 불변이라 장부 무수정, 트랜스폼만 복원.
		const bool bUpdated = ISM->UpdateInstanceTransform(*FoundIdx, Xform, /*bWorldSpace=*/true, /*bMarkRenderStateDirty=*/true);
		ensureMsgf(bUpdated, TEXT("[Grid] 오버레이 장부 어긋남(show): idx %d / 인스턴스 %d개"), *FoundIdx, ISM->GetInstanceCount());
		return;
	}
	const int32 InstanceIdx = ISM->AddInstance(Xform, /*bWorldSpace=*/true);
	CellToInstance.Add(Cell, InstanceIdx);
	ensureMsgf(ISM->GetInstanceCount() == CellToInstance.Num(),
		TEXT("[Grid] 오버레이 장부 어긋남(add): 인스턴스 %d != 장부 %d"), ISM->GetInstanceCount(), CellToInstance.Num());
}

void AOJJ_Grid::OJJ_HideOverlayInstance(UInstancedStaticMeshComponent* ISM,
	const TMap<FIntPoint, int32>& CellToInstance, FIntPoint Cell)
{
	const int32* FoundIdx = CellToInstance.Find(Cell);
	if (!ISM || !FoundIdx)
	{
		return; // 장부 없음 = 오버레이 미적재 상태(디버그 모드 등) — no-op.
	}
	// 제거 대신 zero-scale 숨김 — RemoveInstance의 인덱스 시프트 시맨틱(Codex F3.5' ①)에 비의존.
	// 장부 엔트리는 유지(재커버 시 OJJ_ShowOverlayInstance가 재활용), 잔존분은 ClearInstances가 정리.
	FTransform Hidden = FTransform::Identity;
	Hidden.SetScale3D(FVector::ZeroVector);
	const bool bUpdated = ISM->UpdateInstanceTransform(*FoundIdx, Hidden, /*bWorldSpace=*/true, /*bMarkRenderStateDirty=*/true);
	ensureMsgf(bUpdated, TEXT("[Grid] 오버레이 장부 어긋남(hide): idx %d / 인스턴스 %d개"), *FoundIdx, ISM->GetInstanceCount());
}

void AOJJ_Grid::OJJ_OnFoundationCoverageVisualChanged(const TArray<FIntPoint>& Cells, bool bCovered)
{
	// F3.5' 부분 갱신: 빌드모드 오버레이가 채워진 상태에서만(밖이면 장부 비어 사실상 no-op이지만
	// 명시 가드 — ShowBlocked 디버그는 원 분류 표시 유지 의도). 커버 가능 셀은 blocked뿐:
	// water/void는 CanPlaceFoundation이 거부, buildable은 이미 초록(전환 불요).
	if (!bVisualizationActive)
	{
		return;
	}

	const bool bGroundZValid = OJJ_HasValidGroundZData();
	for (const FIntPoint& Cell : Cells)
	{
		if (!UnbuildableCells.Contains(Cell))
		{
			continue;
		}
		if (bCovered)
		{
			// 커버 등록 "후" 호출 전제 — 트랜스폼 Z가 슬래브 상면(OJJ_GetCellVisualBaseZ 단일원)을 읽음.
			OJJ_HideOverlayInstance(BlockedCellISM, BlockedCellToInstance, Cell);
			OJJ_ShowOverlayInstance(CoveredCellISM, CoveredCellToInstance, Cell, bGroundZValid);
		}
		else
		{
			// 커버 해제 "후" 호출 전제 — Z가 지형으로 복귀, 원 분류(빨강) 복원.
			OJJ_HideOverlayInstance(CoveredCellISM, CoveredCellToInstance, Cell);
			OJJ_ShowOverlayInstance(BlockedCellISM, BlockedCellToInstance, Cell, bGroundZValid);
		}
	}
}

void AOJJ_Grid::SetForceShowBlocked(bool bShow)
{
	bForceShowBlocked = bShow;
	// 상태 기반 갱신 — 빌드모드 중이면 전체(초록+빨강+파랑) 유지, 밖이면 토글된 분류만.
	RefreshGridVisual();
}

void AOJJ_Grid::SetForceShowWater(bool bShow)
{
	bForceShowWater = bShow;
	RefreshGridVisual();
}

// === Foundation 커버리지 레이어 (F1-a) ===
// 점유(OccupiedCells=차단)와 의미가 반대인 "허가" 레이어. 기존 read/write 경로와 완전 독립 —
// 이 블록의 함수들만 FoundationCells/OJJ_FoundationToCells를 만진다(소비처 연결은 F1-c).

bool AOJJ_Grid::CanPlaceFoundation(FIntPoint Origin, FIntPoint Size, FString& OutReason) const
{
	OutReason.Reset();

	if (Size.X < 1 || Size.Y < 1)
	{
		OutReason = FString::Printf(TEXT("Invalid foundation size %dx%d."), Size.X, Size.Y);
		return false;
	}

	// 오버플로/거대 입력 방어(Codex F1-a #2): 끝 좌표는 int64로 계산하고, 순회는 그리드와의 교집합
	// 사각형만 — off-grid 수는 산술 집계. (Origin+Size를 int32로 더하면 UB, 전수 순회는 INT_MAX 입력에서 폭주.)
	const int64 EndX = (int64)Origin.X + Size.X;
	const int64 EndY = (int64)Origin.Y + Size.Y;
	const int32 IterMinX = FMath::Max(Origin.X, 0);
	const int32 IterMinY = FMath::Max(Origin.Y, 0);
	const int32 IterEndX = (int32)FMath::Min<int64>(EndX, (int64)GridSize.X);
	const int32 IterEndY = (int32)FMath::Min<int64>(EndY, (int64)GridSize.Y);
	const int64 TotalCells = (int64)Size.X * (int64)Size.Y;
	const int64 InGridCells =
		(int64)FMath::Max(0, IterEndX - IterMinX) * (int64)FMath::Max(0, IterEndY - IterMinY);

	// 교집합 전 셀을 끝까지 순회해 사유별 집계(조기 종료 없음) — water 43% 지형에서 분포 실측이
	// F1-b 디버깅·waterZ 재검토(§5-3) 근거. stale(파괴된 Foundation/점유 액터) 엔트리는 비차단 —
	// IsCellOnFoundation/IsCellOccupied의 weak 유효 의미와 일관(const라 sweep은 write 경로에 위임).
	const int64 OffGrid = TotalCells - InGridCells;
	int32 Overlap = 0, VoidCount = 0, WaterCount = 0, Occupied = 0;
	for (int32 X = IterMinX; X < IterEndX; ++X)
	{
		for (int32 Y = IterMinY; Y < IterEndY; ++Y)
		{
			const FIntPoint Cell(X, Y);  // 교집합 내부 — IsValidGridCell 보장
			if (IsCellOnFoundation(Cell)) { ++Overlap; }
			if (IsCellVoid(Cell)) { ++VoidCount; }
			if (IsCellWater(Cell)) { ++WaterCount; }   // §5-3(물 위 Foundation) 허용 결정 시 이 게이트만 제거
			if (IsCellOccupied(Cell)) { ++Occupied; }  // 머신/컨베이어/자원 점유 — 기존 건물과의 Z 충돌 방지
		}
	}

	if (OffGrid + Overlap + VoidCount + WaterCount + Occupied == 0)
	{
		return true;
	}

	// 사유별 셀 수 — 한 셀이 복수 사유(예: water+occupied)면 중복 집계될 수 있음(사유 합 ≥ 불가 셀 수).
	TArray<FString> Parts;
	if (OffGrid > 0)    { Parts.Add(FString::Printf(TEXT("off-grid %lld"), OffGrid)); }
	if (Overlap > 0)    { Parts.Add(FString::Printf(TEXT("foundation-overlap %d"), Overlap)); }
	if (VoidCount > 0)  { Parts.Add(FString::Printf(TEXT("void %d"), VoidCount)); }
	if (WaterCount > 0) { Parts.Add(FString::Printf(TEXT("water %d"), WaterCount)); }
	if (Occupied > 0)   { Parts.Add(FString::Printf(TEXT("occupied %d"), Occupied)); }
	OutReason = FString::Printf(TEXT("Foundation blocked (%dx%d=%lld cells): %s."),
		Size.X, Size.Y, TotalCells, *FString::Join(Parts, TEXT(" / ")));
	return false;
}

bool AOJJ_Grid::TryPlaceFoundation(AActor* Foundation, FIntPoint Origin, FIntPoint Size, float SurfaceZ, FString& OutReason)
{
	// 단일값 = 전 셀 동일 — 검증/커밋은 내부 단일원에 위임(F3-1). 배열 미경유라 할당 0(기존과 동일).
	return OJJ_TryPlaceFoundationInternal(
		Foundation, Origin, Size,
		[SurfaceZ](FIntPoint) { return SurfaceZ; },
		OutReason);
}

bool AOJJ_Grid::OJJ_TryPlaceFoundationPerCell(AActor* Foundation, FIntPoint Origin, FIntPoint Size,
	const TArray<float>& CellSurfaceZs, FString& OutReason)
{
	// 배열 불변식(결정 ㉲ — 액터 신뢰 금지). 크기 비교는 int64(거대 Size 곱 오버플로 방어 — CanPlace 미러).
	const int64 ExpectedNum = (int64)Size.X * (int64)Size.Y;
	if (Size.X < 1 || Size.Y < 1 || (int64)CellSurfaceZs.Num() != ExpectedNum)
	{
		OutReason = FString::Printf(TEXT("Per-cell SurfaceZ count mismatch (%d != %lld for %dx%d)."),
			CellSurfaceZs.Num(), ExpectedNum, Size.X, Size.Y);
		return false;
	}

	float MinZ = TNumericLimits<float>::Max();
	float MaxZ = -TNumericLimits<float>::Max();
	for (const float Z : CellSurfaceZs)
	{
		if (!FMath::IsFinite(Z))
		{
			OutReason = TEXT("Per-cell SurfaceZ contains a non-finite value.");
			return false;
		}
		MinZ = FMath::Min(MinZ, Z);
		MaxZ = FMath::Max(MaxZ, Z);
	}

	// (max−min)이 단 간격의 정수배 — 절대 단 격자 정합은 그리드가 Thickness를 모르는 계약(F1-b)상
	// 상대 검증. 양 끝 정합 산식(f3 계획 §보강 — r/(R−1) 보간)이 절대 정합을 클래스 측에서 보장.
	const float Span = MaxZ - MinZ;
	const float StepRemainder = FMath::Fmod(Span, OJJ_FoundationSnapStep);
	if (!FMath::IsNearlyZero(StepRemainder, 0.1f)
		&& !FMath::IsNearlyEqual(StepRemainder, OJJ_FoundationSnapStep, 0.1f))
	{
		OutReason = FString::Printf(
			TEXT("Per-cell SurfaceZ span %.2f is not a multiple of snap step %.0f."), Span, OJJ_FoundationSnapStep);
		return false;
	}

	return OJJ_TryPlaceFoundationInternal(
		Foundation, Origin, Size,
		[&CellSurfaceZs, Origin, Size](FIntPoint Cell)
		{
			return CellSurfaceZs[(Cell.X - Origin.X) * Size.Y + (Cell.Y - Origin.Y)];
		},
		OutReason);
}

bool AOJJ_Grid::OJJ_TryPlaceFoundationInternal(AActor* Foundation, FIntPoint Origin, FIntPoint Size,
	TFunctionRef<float(FIntPoint Cell)> SurfaceZForCell, FString& OutReason)
{
	if (!HasAuthority())
	{
		ensureMsgf(false, TEXT("TryPlaceFoundation called on non-authority"));
		OutReason = TEXT("Not authority");
		return false;
	}

	SweepStaleFoundationEntries();

	if (!IsValid(Foundation))
	{
		OutReason = TEXT("Invalid foundation actor");
		return false;
	}

	if (OJJ_FoundationToCells.Contains(Foundation))
	{
		OutReason = TEXT("Foundation already placed.");
		return false;
	}

	if (!CanPlaceFoundation(Origin, Size, OutReason))
	{
		return false;
	}

	// 검증 통과 후 일괄 커밋 — 부분 등록 없음. 액터 위치/비주얼은 호출자(F1-b BuildController) 책임.
	// CanPlace 성공 = off-grid 0 = footprint 전체가 그리드 내부 → 아래 int32 덧셈은 오버플로 불가.
	TArray<FIntPoint> Cells;
	Cells.Reserve(Size.X * Size.Y);
	for (int32 X = Origin.X; X < Origin.X + Size.X; ++X)
	{
		for (int32 Y = Origin.Y; Y < Origin.Y + Size.Y; ++Y)
		{
			const FIntPoint Cell(X, Y);
			FOJJFoundationCellInfo Info;
			Info.Foundation = Foundation;
			Info.SurfaceZ = SurfaceZForCell(Cell);
			FoundationCells.Add(Cell, Info);
			Cells.Add(Cell);
		}
	}
	// 오버레이 부분 갱신(F3.5' — 커버 등록 완료 후라 Z 단일원이 슬래브 상면을 읽음). MoveTemp 전에 호출.
	OJJ_OnFoundationCoverageVisualChanged(Cells, /*bCovered=*/true);

	OJJ_FoundationToCells.Add(Foundation, MoveTemp(Cells));

	OutReason.Reset();
	return true;
}

bool AOJJ_Grid::RemoveFoundation(AActor* Foundation, FString& OutReason)
{
	if (!HasAuthority())
	{
		ensureMsgf(false, TEXT("RemoveFoundation called on non-authority"));
		OutReason = TEXT("Not authority");
		return false;
	}

	SweepStaleFoundationEntries();

	const TArray<FIntPoint>* Cells = Foundation ? OJJ_FoundationToCells.Find(Foundation) : nullptr;
	if (!Cells)
	{
		OutReason = TEXT("Foundation not registered.");
		return false;
	}

	// 위 건물 게이트: 커버 셀에 유효 점유(머신/컨베이어)가 있으면 거부 — F1은 연쇄 철거 대신 거부가 안전.
	// 판정식은 OJJ_CountOccupiedFoundationCells와 공유(F2-0) — 철거 호버와 단일 진실원.
	const int32 OccupiedOnTop = OJJ_CountOccupiedFoundationCells(Foundation);
	if (OccupiedOnTop > 0)
	{
		OutReason = FString::Printf(TEXT("Foundation has %d occupied cell(s) on top — remove buildings first."), OccupiedOnTop);
		return false;
	}

	// 양방향 대칭 해제. forward 엔트리는 본인 소유일 때만 제거(방어 — 겹침 금지라 정상 흐름에선 항상 본인).
	// 셀 목록은 맵 제거 전에 복사 — 오버레이 복원(아래)이 커버 해제 "후" Z(지형)를 읽어야 해서 순서 고정.
	const TArray<FIntPoint> RemovedCells = *Cells;
	for (const FIntPoint& Cell : RemovedCells)
	{
		const FOJJFoundationCellInfo* Found = FoundationCells.Find(Cell);
		if (Found && Found->Foundation == Foundation)
		{
			FoundationCells.Remove(Cell);
		}
	}
	OJJ_FoundationToCells.Remove(Foundation);

	// 오버레이 부분 갱신(F3.5') — 원 분류(빨강) 복원, Z는 지형 기준으로 복귀.
	OJJ_OnFoundationCoverageVisualChanged(RemovedCells, /*bCovered=*/false);

	OutReason.Reset();
	return true;
}

// 지배 SurfaceZ 선출(결정 ㉷): 접촉 셀 수 최다, 동률이면 낮은 단(아래에서 위로 짓는 흐름).
static bool OJJ_PickDominantSurfaceZ(const TArray<TPair<float, int32>>& Groups, float& OutSurfaceZ, int32& OutContactCells)
{
	OutSurfaceZ = 0.0f;
	OutContactCells = 0;
	for (const TPair<float, int32>& Group : Groups)
	{
		if (Group.Value > OutContactCells
			|| (Group.Value == OutContactCells && OutContactCells > 0 && Group.Key < OutSurfaceZ))
		{
			OutSurfaceZ = Group.Key;
			OutContactCells = Group.Value;
		}
	}
	return OutContactCells > 0;
}

void AOJJ_Grid::OJJ_AccumulateFoundationSurfaceZ(FIntPoint RectOrigin, FIntPoint RectSize,
	float SnapGridOriginZ, TArray<TPair<float, int32>>& InOutGroups) const
{
	// 그리드 교집합만 순회(int64 끝좌표 — CanPlaceFoundation 거대 입력 방어 미러). stale 셀은
	// GetFoundationSurfaceZ가 weak IsValid로 false — 파괴된 Foundation의 유령 단 상속 차단(F3.5 계약).
	const int32 IterMinX = FMath::Max(RectOrigin.X, 0);
	const int32 IterMinY = FMath::Max(RectOrigin.Y, 0);
	const int32 IterEndX = (int32)FMath::Min<int64>((int64)RectOrigin.X + RectSize.X, (int64)GridSize.X);
	const int32 IterEndY = (int32)FMath::Min<int64>((int64)RectOrigin.Y + RectSize.Y, (int64)GridSize.Y);
	for (int32 X = IterMinX; X < IterEndX; ++X)
	{
		for (int32 Y = IterMinY; Y < IterEndY; ++Y)
		{
			float SurfaceZ = 0.0f;
			if (!GetFoundationSurfaceZ(FIntPoint(X, Y), SurfaceZ))
			{
				continue;
			}
			// 비격자 단 제외(Codex F3.5 C): 램프 중간 행(행당 100/(R−1)uu) 등 단 격자 밖 SurfaceZ를
			// 상속하면 평판이 비정수 단에 떠서 단 격자 전제(걸침 균일·㉲ 불변식)가 무너진다.
			// 램프 양 끝 행은 격자 위라 정상 후보로 남음(엣지 확장 허용 — 의도).
			// [F3.10 관찰 b①] 1칸 램프는 셀 전체가 격자 위(Z_low 평판 등록)라 이 필터를 통과 —
			// 자동 맞춤 스캔이 45° 쐐기 면을 평판 이웃으로 쓸 수 있음(낮은쪽 단 기준 산출). PIE 관찰
			// 후 문제 실측 시 face 훅(OJJ_GetVisualSurfaceZAtWorld 보유 = 비평탄) 제외로 보강.
			const float StepRemainder = FMath::Fmod(SurfaceZ - SnapGridOriginZ, OJJ_FoundationSnapStep);
			if (!FMath::IsNearlyZero(StepRemainder, 0.1f)
				&& !FMath::IsNearlyEqual(FMath::Abs(StepRemainder), OJJ_FoundationSnapStep, 0.1f))
			{
				continue;
			}
			bool bGrouped = false;
			for (TPair<float, int32>& Group : InOutGroups)
			{
				// 같은 단은 상속/씨앗 모두 단 격자 산출이라 사실상 동일 float — IsNearlyEqual은 여유 방어.
				if (FMath::IsNearlyEqual(Group.Key, SurfaceZ))
				{
					++Group.Value;
					bGrouped = true;
					break;
				}
			}
			if (!bGrouped)
			{
				InOutGroups.Emplace(SurfaceZ, 1);
			}
		}
	}
}

bool AOJJ_Grid::OJJ_GetDominantFoundationSurfaceZInRect(FIntPoint RectOrigin, FIntPoint RectSize,
	float SnapGridOriginZ, float& OutSurfaceZ, int32& OutContactCells) const
{
	TArray<TPair<float, int32>> Groups;
	OJJ_AccumulateFoundationSurfaceZ(RectOrigin, RectSize, SnapGridOriginZ, Groups);
	return OJJ_PickDominantSurfaceZ(Groups, OutSurfaceZ, OutContactCells);
}

bool AOJJ_Grid::OJJ_GetNeighborFoundationSurfaceZ(FIntPoint Origin, FIntPoint Size,
	float SnapGridOriginZ, float& OutSurfaceZ, int32& OutContactCells) const
{
	// 면접촉 둘레 4변(결정 ㉶ — 대각 모서리 셀 미포함): 서/동 세로 라인 + 남/북 가로 라인 합산 집계.
	TArray<TPair<float, int32>> Groups;
	OJJ_AccumulateFoundationSurfaceZ(FIntPoint(Origin.X - 1, Origin.Y), FIntPoint(1, Size.Y), SnapGridOriginZ, Groups);
	OJJ_AccumulateFoundationSurfaceZ(FIntPoint(Origin.X + Size.X, Origin.Y), FIntPoint(1, Size.Y), SnapGridOriginZ, Groups);
	OJJ_AccumulateFoundationSurfaceZ(FIntPoint(Origin.X, Origin.Y - 1), FIntPoint(Size.X, 1), SnapGridOriginZ, Groups);
	OJJ_AccumulateFoundationSurfaceZ(FIntPoint(Origin.X, Origin.Y + Size.Y), FIntPoint(Size.X, 1), SnapGridOriginZ, Groups);
	return OJJ_PickDominantSurfaceZ(Groups, OutSurfaceZ, OutContactCells);
}

int32 AOJJ_Grid::OJJ_CountOccupiedFoundationCells(AActor* Foundation) const
{
	// stale 점유는 IsCellOccupied가 weak IsValid로 걸러 비차단(점유 레이어와 동일 의미).
	// const라 sweep은 write 경로(RemoveFoundation)에 위임 — CanPlaceFoundation과 동일 방어.
	const TArray<FIntPoint>* Cells = Foundation ? OJJ_FoundationToCells.Find(Foundation) : nullptr;
	if (!Cells)
	{
		return INDEX_NONE;
	}
	int32 Occupied = 0;
	for (const FIntPoint& Cell : *Cells)
	{
		if (IsCellOccupied(Cell)) { ++Occupied; }
	}
	return Occupied;
}

bool AOJJ_Grid::IsCellOnFoundation(FIntPoint Cell) const
{
	const FOJJFoundationCellInfo* Found = FoundationCells.Find(Cell);
	return Found && Found->Foundation.IsValid();
}

AActor* AOJJ_Grid::GetFoundationAtCell(FIntPoint Cell) const
{
	const FOJJFoundationCellInfo* Found = FoundationCells.Find(Cell);
	// stale(파괴된) Foundation 셀은 nullptr — IsCellOnFoundation과 동일 의미.
	return Found ? Found->Foundation.Get() : nullptr;
}

const TArray<FIntPoint>* AOJJ_Grid::GetFoundationCells(AActor* Foundation) const
{
	return Foundation ? OJJ_FoundationToCells.Find(Foundation) : nullptr;
}

bool AOJJ_Grid::GetFoundationSurfaceZ(FIntPoint Cell, float& OutSurfaceZ) const
{
	const FOJJFoundationCellInfo* Found = FoundationCells.Find(Cell);
	if (Found && Found->Foundation.IsValid())
	{
		OutSurfaceZ = Found->SurfaceZ;
		return true;
	}
	OutSurfaceZ = 0.0f;
	return false;
}

bool AOJJ_Grid::OJJ_GetUniformSurfaceZ(const TArray<FIntPoint>& Cells, float& OutZ) const
{
	// 단일 건설면 규칙(F1-c §7-3): 전 셀이 같은 높이의 면이어야 Z 안착이 유일하게 정해진다.
	// 지형(비-Foundation)은 균일 취급 유지 — §5-2 직배치는 F3까지 비파괴(셀 간 GroundZ 차로 거부하지 않음).
	// [F3.10 관찰 b②] 1셀 풋프린트는 자명히 균일 통과 — 램프 셀(중간 행/1칸 램프) 위 1×1 배치물이
	// 장부 Z(계단/Z_low)에 앉아 쐐기에 묻힐 수 있음(램프 중간 행 거부는 다셀 이높이 비교에만 의존).
	// PIE 관찰 후 실측 시 face 훅 보유 셀 게이트로 보강.
	// F2-1(결정 (a)/③)부터 지형 OutZ는 평면 고정이 아니라 풋프린트 GroundZ 최고점(함수 말미) —
	// 베이크 대표값(셀 최고점)과 한 쌍으로 직배치 묻힘 해소. 뜸은 ≤ tol+셀간차로 유계(충돌 없음).
	// stale Foundation 셀은 GetFoundationSurfaceZ가 false라 비-Foundation 취급(점유 stale 의미와 일관).
	OutZ = GetActorLocation().Z;
	if (Cells.Num() == 0)
	{
		return true;
	}

	bool bFirst = true;
	bool bOnFoundation = false;
	float SurfaceZ = 0.0f;
	for (const FIntPoint& Cell : Cells)
	{
		float CellSurfaceZ = 0.0f;
		const bool bCellOnFoundation = GetFoundationSurfaceZ(Cell, CellSurfaceZ);
		if (bFirst)
		{
			bFirst = false;
			bOnFoundation = bCellOnFoundation;
			SurfaceZ = CellSurfaceZ;
			continue;
		}
		// 혼합(지형+Foundation) 또는 이높이 Foundation = 경계 걸침 → 거부.
		if (bCellOnFoundation != bOnFoundation
			|| (bCellOnFoundation && !FMath::IsNearlyEqual(CellSurfaceZ, SurfaceZ)))
		{
			return false;
		}
	}

	if (bOnFoundation)
	{
		OutZ = SurfaceZ;
		return true;
	}

	// 지형 경로(F2-1): GroundZ 유효 시 풋프린트 최고 셀에 안착. 무효(미베이크/시그니처 불일치)면 평면
	// 폴백 = 기존(F1) 동작 그대로(회귀 0). 유효성은 셀 불변이라 1회만 검사(비주얼 경로 호이스팅과 동일).
	if (OJJ_HasValidGroundZData())
	{
		float MaxDelta = -TNumericLimits<float>::Max();
		bool bAllCellsInGrid = true; // off-grid 셀 포함 풋프린트는 평면 폴백(방어 — 호출자 사전 거부가 정상).
		for (const FIntPoint& Cell : Cells)
		{
			if (!IsValidGridCell(Cell))
			{
				bAllCellsInGrid = false;
				break;
			}
			MaxDelta = FMath::Max(MaxDelta, (float)CellGroundZQuant[OJJ_CellLinearIndex(Cell, GridSize)]);
		}
		if (bAllCellsInGrid)
		{
			OutZ = GetActorLocation().Z + MaxDelta;
		}
	}
	return true;
}

float AOJJ_Grid::OJJ_GetCellVisualBaseZ(FIntPoint Cell) const
{
	return OJJ_GetCellVisualBaseZInternal(Cell, OJJ_HasValidGroundZData());
}

float AOJJ_Grid::OJJ_GetCellVisualBaseZInternal(FIntPoint Cell, bool bGroundZValid) const
{
	// 우선순위: Foundation 상면 > 지형(GroundZ 추종) > 평면. GroundZ 무효(미베이크/시그니처 불일치) 맵은
	// 평면 폴백 = 기존 동작(회귀 0). GroundZ는 셀 최고점 기준(F2-1) — 타일이 셀 안 지형면 위에 위치.
	// 직배치 액터 Z도 풋프린트 최고점(OJJ_GetUniformSurfaceZ)이라 비주얼-액터 편차는 풋프린트 내
	// 셀간 GroundZ 차이로 유계(F1의 평면 안착 ±tol 편차는 F2-1로 해소 — Codex F1-c #1·#3 기각 기록 대체).
	float SurfaceZ = 0.0f;
	if (GetFoundationSurfaceZ(Cell, SurfaceZ))
	{
		return SurfaceZ;  // 슬래브 상면은 평탄 — 교차 없음, 리프트 불요
	}
	if (bGroundZValid && IsValidGridCell(Cell))
	{
		// VisualZLift: 가장자리 미샘플(5점=±0.4셀) 잔존 교차 대비 들어올림(시각 전용). 최고점 기준(F2-1)
		// 전환으로 셀 내 교차는 구조 해소 — PIE 실측 후 0 축소 재검토(F2 계획 §1).
		return GetActorLocation().Z + (float)CellGroundZQuant[OJJ_CellLinearIndex(Cell, GridSize)] + VisualZLift;
	}
	return GridToWorld(Cell).Z;  // 평면 폴백 — 기존(F1-c 이전) 동작 그대로, 리프트 미적용
}

FVector AOJJ_Grid::GetFoundationPlacementLocation(FIntPoint Origin, FIntPoint Size) const
{
	// 머신 GetMachinePlacementLocation과 동일한 "lower-left 셀 중심 + (Size-1)/2" 수식 — 좌표 규약 공유.
	// 메시 AABB Z 보정 없음(Foundation이 상면=평면+Thickness 오프셋을 자체 처리). Z = 그리드 평면.
	const FVector LowerLeftCenter = GridToWorld(Origin);
	const float OffsetX = (Size.X - 1) * CellSize * 0.5f;
	const float OffsetY = (Size.Y - 1) * CellSize * 0.5f;
	return FVector(LowerLeftCenter.X + OffsetX, LowerLeftCenter.Y + OffsetY, LowerLeftCenter.Z);
}

float AOJJ_Grid::OJJ_ComputeFoundationSnapLift(FIntPoint Origin, FIntPoint Size, float Thickness) const
{
	// GetCellGroundZ 소비처 — 셀 불변 유효성은 1회 호이스팅(비주얼/GetUniformSurfaceZ 경로와 동일 패턴).
	if (!OJJ_HasValidGroundZData() || Size.X < 1 || Size.Y < 1)
	{
		return 0.0f;
	}

	// 끝 좌표 int64 + 그리드 교집합 계산(CanPlaceFoundation의 거대 입력 방어 미러). 풋프린트가 하나라도
	// 그리드 밖이면 0(계약 — Codex F2-4 ①): 그런 배치는 CanPlaceFoundation이 어차피 거부하므로 리프트는
	// 평면 폴백이 일관(부분 교집합 max로 non-zero를 돌려주면 계약 위반 + 호출자 오해 소지).
	const int32 IterMinX = FMath::Max(Origin.X, 0);
	const int32 IterMinY = FMath::Max(Origin.Y, 0);
	const int32 IterEndX = (int32)FMath::Min<int64>((int64)Origin.X + Size.X, (int64)GridSize.X);
	const int32 IterEndY = (int32)FMath::Min<int64>((int64)Origin.Y + Size.Y, (int64)GridSize.Y);
	const int64 TotalCells = (int64)Size.X * (int64)Size.Y;
	const int64 InGridCells =
		(int64)FMath::Max(0, IterEndX - IterMinX) * (int64)FMath::Max(0, IterEndY - IterMinY);
	if (InGridCells != TotalCells)
	{
		return 0.0f;
	}

	float MaxGroundZ = -TNumericLimits<float>::Max();
	for (int32 X = IterMinX; X < IterEndX; ++X)
	{
		for (int32 Y = IterMinY; Y < IterEndY; ++Y)
		{
			float Delta = 0.0f;
			if (GetCellGroundZ(FIntPoint(X, Y), Delta))
			{
				MaxGroundZ = FMath::Max(MaxGroundZ, Delta);
			}
		}
	}
	if (MaxGroundZ == -TNumericLimits<float>::Max())
	{
		return 0.0f; // 도달 불가(유효 데이터 + 전 셀 in-grid면 항상 채워짐) — CeilToInt 극값 방어선만.
	}

	// N = ceil((max GroundZ − Thickness) / 100) clamp ≥0. 경계: max == Thickness+k×100이면 상면이 지형
	// 최고점에 정확히 접함(N=k) — "이상" 조건 충족이라 다음 단으로 올리지 않음.
	const int32 SnapSteps = FMath::Max(0, FMath::CeilToInt((MaxGroundZ - Thickness) / OJJ_FoundationSnapStep));
	return SnapSteps * OJJ_FoundationSnapStep;
}

void AOJJ_Grid::OJJ_UpdateFoundationHoverPreview(FIntPoint Origin, FIntPoint Size, bool bForceInvalid)
{
	ClearHoverPreview();

	if (Size.X < 1 || Size.Y < 1)
	{
		return;
	}

	// 프리뷰 인스턴스 폭주 방어 — FoundationSize는 디자이너 프로퍼티라 거대값 실수 가능. 호버는 매 셀
	// 이동마다 리빌드되므로 셀 수 상한(에디터 오버레이 OOM과 동일 교훈). 8×8 정상 케이스(64)에 충분한 여유.
	constexpr int64 MaxPreviewCells = 4096;
	const int64 EndX = (int64)Origin.X + Size.X;
	const int64 EndY = (int64)Origin.Y + Size.Y;
	if ((int64)Size.X * (int64)Size.Y > MaxPreviewCells)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Grid] FoundationHoverPreview: 풋프린트 %dx%d > 상한 %lld셀 — 프리뷰 생략(FoundationSize 확인)."),
			Size.X, Size.Y, MaxPreviewCells);
		return;
	}

	// 단일 진실원: 호버 색 = 클릭 시 판정(CanPlaceFoundation). 사유는 색에선 버림 — 클릭 실패 시
	// BuildController가 같은 함수의 OutReason(사유별 셀 수)을 로그.
	// bForceInvalid(F3.6-1)는 클릭 측도 같은 게이트(풋프린트 훅 bValid)로 거부하므로 단일원 유지.
	FString UnusedReason;
	const bool bCanPlace = !bForceInvalid && CanPlaceFoundation(Origin, Size, UnusedReason);
	UInstancedStaticMeshComponent* TargetISM = bCanPlace ? ValidHoverISM.Get() : InvalidHoverISM.Get();
	if (!TargetISM)
	{
		return;
	}

	// 셀→인스턴스 규칙은 머신 호버(UpdateHoverPreview)와 동일: Z+2 가림 방지, Plane 100→CellSize 스케일,
	// world-space. off-grid 셀도 그려 경계 밖 빨강 피드백 유지(머신 CalculateFootprint도 off-grid 포함).
	for (int64 X = Origin.X; X < EndX; ++X)
	{
		for (int64 Y = Origin.Y; Y < EndY; ++Y)
		{
			const FIntPoint PreviewCell((int32)X, (int32)Y);
			const FVector CellCenter = GridToWorld(PreviewCell);
			const FVector InstanceLocation(CellCenter.X, CellCenter.Y, OJJ_GetCellVisualBaseZ(PreviewCell) + 2.0f + HoverExtraZLift);
			const FVector InstanceScale(CellSize / 100.0f, CellSize / 100.0f, 1.0f);
			TargetISM->AddInstance(FTransform(FRotator::ZeroRotator, InstanceLocation, InstanceScale), /*bWorldSpace=*/true);
		}
	}
}

void AOJJ_Grid::OJJ_UpdateCharacterCellOverlay(const TArray<FIntPoint>& Cells)
{
	OJJ_EnsureTileMIDs();

	UInstancedStaticMeshComponent* TargetISM = CharacterCellISM.Get();
	if (!TargetISM)
	{
		return;
	}

	// 캡슐 풋프린트는 1~4셀 — ClearInstances+재적재가 가장 단순(셀 변경 시에만 호출되는 계약은 호출자 책임).
	TargetISM->ClearInstances();
	for (const FIntPoint& Cell : Cells)
	{
		const FVector CellCenter = GridToWorld(Cell);
		// 호버 타일(+2+HoverExtraZLift)보다 1uu 위 — 캐릭터 표시는 경고 성격이라 호버 풋프린트와 겹쳐도 식별.
		const FVector InstanceLocation(CellCenter.X, CellCenter.Y, OJJ_GetCellVisualBaseZ(Cell) + 3.0f + HoverExtraZLift);
		const FVector InstanceScale(CellSize / 100.0f, CellSize / 100.0f, 1.0f);
		TargetISM->AddInstance(FTransform(FRotator::ZeroRotator, InstanceLocation, InstanceScale), /*bWorldSpace=*/true);
	}
}

void AOJJ_Grid::SweepStaleFoundationEntries()
{
	// SweepStaleEntries 미러(커버리지판). forward 셀은 weak 무효일 때만 제거 — 점유 sweep과 동일 방어.
	// 제거 셀은 모아서 오버레이 복원(Codex F3.5' ③) — 외부 파괴된 Foundation의 초록 잔존 차단.
	TArray<FIntPoint> StaleCells;
	for (auto It = OJJ_FoundationToCells.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			for (const FIntPoint& Cell : It.Value())
			{
				const FOJJFoundationCellInfo* Found = FoundationCells.Find(Cell);
				if (Found && !Found->Foundation.IsValid())
				{
					FoundationCells.Remove(Cell);
					StaleCells.Add(Cell);
				}
			}
			It.RemoveCurrent();
		}
	}
	if (StaleCells.Num() > 0)
	{
		// 커버 해제 후 호출(정상 철거와 동일 순서) — 원 분류(빨강) 복원, Z는 지형 복귀.
		OJJ_OnFoundationCoverageVisualChanged(StaleCells, /*bCovered=*/false);
	}
}

// === 파이프 레이어 (F4-0, f4_pipe_plan.md 결정 ㉠ — FoundationCells 독립 레이어 패턴 미러) ===

void AOJJ_Grid::SweepStalePipeEntries()
{
	// SweepStaleFoundationEntries 미러(파이프판). forward 셀은 weak 무효일 때만 제거 — 동일 방어.
	// 오버레이 복원 호출 없음(F4-0 파이프 레이어는 시각 표현 0).
	for (auto It = OJJ_PipeToCells.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			for (const FIntPoint& Cell : It.Value())
			{
				const FOJJPipeCellInfo* Found = OJJ_PipeCells.Find(Cell);
				if (Found && !Found->Pipe.IsValid())
				{
					OJJ_PipeCells.Remove(Cell);
				}
			}
			It.RemoveCurrent();
		}
	}
}

bool AOJJ_Grid::OJJ_TryRegisterPipeCells(AActor* Pipe, const TArray<FIntPoint>& Cells,
	const TArray<float>& CellZs, const TArray<bool>& ElevatedFlags, FString& OutReason)
{
	if (!HasAuthority())
	{
		ensureMsgf(false, TEXT("OJJ_TryRegisterPipeCells called on non-authority"));
		OutReason = TEXT("Not authority");
		return false;
	}
	// IsValid: pending kill 거부(Foundation 등록 원본 미러 — Codex F4-0 ②). null 통과 시 weak가 즉시
	// invalid라 "등록 성공인데 질의는 미점유" 모순 상태가 생김.
	if (!IsValid(Pipe) || Cells.Num() == 0)
	{
		OutReason = TEXT("Invalid pipe or empty cells.");
		return false;
	}
	// 배열 1:1 불변식(액터 신뢰 금지 — PerCell 등록의 크기 검증과 동일 정신).
	if (CellZs.Num() != Cells.Num() || ElevatedFlags.Num() != Cells.Num())
	{
		OutReason = FString::Printf(TEXT("Cell array size mismatch (cells %d / z %d / elevated %d)."),
			Cells.Num(), CellZs.Num(), ElevatedFlags.Num());
		return false;
	}

	SweepStalePipeEntries();

	if (OJJ_PipeToCells.Contains(Pipe))
	{
		OutReason = TEXT("Pipe already registered.");
		return false;
	}

	// 검증 패스: 그리드 내 + 비유한 Z 거부 + 파이프 간 겹침 거부(결정 ㉥ — 레이어 내 단일 점유).
	// 중복 입력 셀은 AddUnique로 1회 등록(컨베이어 등록 UniqueCells 정신).
	TArray<FIntPoint> UniqueCells;
	UniqueCells.Reserve(Cells.Num());
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FIntPoint Cell = Cells[Index];
		if (!IsValidGridCell(Cell))
		{
			OutReason = FString::Printf(TEXT("Pipe cell (%d,%d) is outside the grid."), Cell.X, Cell.Y);
			return false;
		}
		if (!FMath::IsFinite(CellZs[Index]))
		{
			OutReason = FString::Printf(TEXT("Pipe cell (%d,%d) has non-finite Z."), Cell.X, Cell.Y);
			return false;
		}
		const FOJJPipeCellInfo* Existing = OJJ_PipeCells.Find(Cell);
		if (Existing && Existing->Pipe.IsValid())
		{
			OutReason = FString::Printf(TEXT("Pipe cell (%d,%d) is occupied by another pipe."), Cell.X, Cell.Y);
			return false;
		}
		UniqueCells.AddUnique(Cell);
	}

	// 커밋 패스 — 셀별 값은 "마지막 입력 우선"이 아닌 첫 입력 기준(중복 셀은 경로 왕복 같은 비정상
	// 입력 — F4-1 수집기가 애초에 안 만들지만 방어적으로 결정론 고정).
	for (const FIntPoint& Cell : UniqueCells)
	{
		const int32 FirstIndex = Cells.IndexOfByKey(Cell);
		FOJJPipeCellInfo Info;
		Info.Pipe = Pipe;
		Info.CellZ = CellZs[FirstIndex];
		Info.bElevated = ElevatedFlags[FirstIndex];
		OJJ_PipeCells.Add(Cell, Info);
	}
	OJJ_PipeToCells.Add(Pipe, MoveTemp(UniqueCells));

	OutReason.Reset();
	return true;
}

bool AOJJ_Grid::OJJ_UnregisterPipeCells(AActor* Pipe, FString& OutReason)
{
	if (!HasAuthority())
	{
		ensureMsgf(false, TEXT("OJJ_UnregisterPipeCells called on non-authority"));
		OutReason = TEXT("Not authority");
		return false;
	}

	SweepStalePipeEntries();

	const TArray<FIntPoint>* Cells = Pipe ? OJJ_PipeToCells.Find(Pipe) : nullptr;
	if (!Cells)
	{
		OutReason = TEXT("Pipe not registered.");
		return false;
	}

	// 양방향 대칭 해제 — forward 엔트리는 본인 소유일 때만 제거(RemoveFoundation 방어 미러).
	// 위 건물 게이트 없음: 파이프 레이어 위엔 아무것도 안 올라감(결정 ㉠ — 항상 해제 성공).
	for (const FIntPoint& Cell : *Cells)
	{
		const FOJJPipeCellInfo* Found = OJJ_PipeCells.Find(Cell);
		if (Found && Found->Pipe == Pipe)
		{
			OJJ_PipeCells.Remove(Cell);
		}
	}
	OJJ_PipeToCells.Remove(Pipe);

	OutReason.Reset();
	return true;
}

AActor* AOJJ_Grid::OJJ_GetPipeAtCell(FIntPoint Cell) const
{
	// stale weak는 nullptr(미점유 취급) — GetFoundationAtCell/GetFoundationSurfaceZ의 stale 정책 미러.
	const FOJJPipeCellInfo* Found = OJJ_PipeCells.Find(Cell);
	return (Found && Found->Pipe.IsValid()) ? Found->Pipe.Get() : nullptr;
}

bool AOJJ_Grid::OJJ_IsCellBlockedByGroundPipe(FIntPoint Cell) const
{
	// 결정 ㉡: 지상 파이프 셀만 차단 — 공중(bElevated, 오버패스) 셀 아래는 컨베이어 통과 허용.
	// 소비처(컨베이어 경로 수집 거부 + OutReason "지상 파이프 통과 불가 — 파이프 상향/우회")는 F4-3.
	const FOJJPipeCellInfo* Found = OJJ_PipeCells.Find(Cell);
	return Found && Found->Pipe.IsValid() && !Found->bElevated;
}

const TArray<FIntPoint>* AOJJ_Grid::OJJ_GetPipeCells(AActor* Pipe) const
{
	const TWeakObjectPtr<AActor> Key(Pipe);
	return Key.IsValid() ? OJJ_PipeToCells.Find(Key) : nullptr;
}

// === 지형 높이 캐시 접근 (F0 갭 해소) ===

bool AOJJ_Grid::OJJ_HasValidGroundZData() const
{
	// 크기 정합만으론 부족(Codex F1-a #4-1): 시그니처 불일치 시 BeginPlay 폴백 베이크(bWriteCache=false)는
	// 분류만 다시 굽고 직렬화된 CellGroundZQuant를 안 건드려, 같은 크기의 stale 배열이 남는다.
	// 캐시 로드와 동일한 시그니처 검증을 공유해 차단. 보수적: 높이와 무관한 파라미터(waterZ 등)만 바뀌어도
	// 무효 — "무효 기준 단일원" 유지 비용으로 수용(Rebake 한 번이면 해소).
	const int32 NumCells = GridSize.X * GridSize.Y;
	if (NumCells <= 0 || CellGroundZQuant.Num() != NumCells || !bHasBakeCache || !bCacheBakeGroundHeights)
	{
		return false;
	}
	bool bStructMatch = false;
	bool bParamMatch = false;
	OJJ_GetBakeCacheSignatureMatch(bStructMatch, bParamMatch);
	return bStructMatch && bParamMatch;
}

bool AOJJ_Grid::GetCellGroundZ(FIntPoint Cell, float& OutGroundZDelta) const
{
	OutGroundZDelta = 0.0f;
	if (!OJJ_HasValidGroundZData() || !IsValidGridCell(Cell))
	{
		return false;
	}
	OutGroundZDelta = (float)CellGroundZQuant[OJJ_CellLinearIndex(Cell, GridSize)];
	return true;
}

void AOJJ_Grid::DumpGroundZReport(FIntPoint Center, int32 Radius) const
{
	const int32 NumCells = GridSize.X * GridSize.Y;
	if (!OJJ_HasValidGroundZData())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Grid] GroundZReport: 유효한 높이 캐시 없음(quant %d / cells %d / hasCache %d / cacheGroundZ %d) — bBakeGroundHeights=true로 RebakeAndCache + 레벨 저장 필요(시그니처 불일치 포함)."),
			CellGroundZQuant.Num(), NumCells, bHasBakeCache ? 1 : 0, bCacheBakeGroundHeights ? 1 : 0);
		return;
	}

	int32 MinV = TNumericLimits<int32>::Max();
	int32 MaxV = TNumericLimits<int32>::Min();
	int64 Sum = 0;
	int32 NonZero = 0;
	for (const int16 V : CellGroundZQuant)
	{
		MinV = FMath::Min(MinV, (int32)V);
		MaxV = FMath::Max(MaxV, (int32)V);
		Sum += V;
		if (V != 0) { ++NonZero; }
	}
	UE_LOG(LogTemp, Log,
		TEXT("[Grid] GroundZReport: %d셀, 델타(평면 상대 uu, 최고점) min %d / max %d / 평균 %.1f / 비제로 %d (%.1f%%)"),
		NumCells, MinV, MaxV, (double)Sum / NumCells, NonZero, 100.0 * NonZero / NumCells);

	// 10버킷 히스토그램 — 분포 모양 확인(waterZ/톨러런스 튜닝 근거). 범위 < 10uu면 정수 절단으로
	// 라벨이 왜곡(빈 구간에 카운트)되므로 생략 — 그 정도 평탄함은 요약(min/max/평균)으로 충분(Codex F1-a #4-2).
	if (MaxV - MinV >= 10)
	{
		int32 Buckets[10] = { 0 };
		const float Range = float(MaxV - MinV);
		for (const int16 V : CellGroundZQuant)
		{
			const int32 B = FMath::Clamp(int32((V - MinV) / Range * 10.0f), 0, 9);
			++Buckets[B];
		}
		for (int32 B = 0; B < 10; ++B)
		{
			const int32 Lo = MinV + FMath::RoundToInt(Range * B / 10.0f);
			const int32 Hi = (B == 9) ? MaxV : MinV + FMath::RoundToInt(Range * (B + 1) / 10.0f);
			UE_LOG(LogTemp, Log, TEXT("[Grid]   [%+6d..%+6d%s %d셀"), Lo, Hi, (B == 9) ? TEXT("]") : TEXT(")"), Buckets[B]);
		}
	}

	// 영역 덤프: Center 유효 + Radius>0일 때만. 캡 400줄(베이크 verbose와 동일).
	if (Radius > 0 && IsValidGridCell(Center))
	{
		const int32 MaxLines = 400;
		int32 Logged = 0;
		for (int32 X = FMath::Max(0, Center.X - Radius); X <= FMath::Min(GridSize.X - 1, Center.X + Radius) && Logged < MaxLines; ++X)
		{
			for (int32 Y = FMath::Max(0, Center.Y - Radius); Y <= FMath::Min(GridSize.Y - 1, Center.Y + Radius) && Logged < MaxLines; ++Y)
			{
				const int16 V = CellGroundZQuant[OJJ_CellLinearIndex(FIntPoint(X, Y), GridSize)];
				UE_LOG(LogTemp, Log, TEXT("[Grid]   cell(%d,%d) groundZ %+d"), X, Y, (int32)V);
				++Logged;
			}
		}
		if (Logged >= MaxLines)
		{
			UE_LOG(LogTemp, Log, TEXT("[Grid]   ... 덤프 캡(%d줄) 도달 — Radius를 줄여 재시도."), MaxLines);
		}
	}
}

// === 디버그 콘솔 명령 (PIE에서 베이크 검증) ===
namespace
{
	AOJJ_Grid* OJJ_FindGridInWorld(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		for (TActorIterator<AOJJ_Grid> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}
}

static FAutoConsoleCommandWithWorld GOJJGridBuildableReport(
	TEXT("OJJ.Grid.BuildableReport"),
	TEXT("AOJJ_Grid 지형 베이크 재실행 + 불가/전체 셀 요약 로그 + 오버레이 표시."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		AOJJ_Grid* Grid = OJJ_FindGridInWorld(World);
		if (!Grid)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Grid] BuildableReport: 월드에 AOJJ_Grid 없음."));
			return;
		}
		Grid->BakeBuildableCells(/*bVerbose=*/true); // 요약 + 평탄 외 셀별 (좌표/hitZ/부호델타/분류) 로그
		Grid->SetForceShowBlocked(true);             // 즉시 시각 확인
	}));

static FAutoConsoleCommandWithWorldAndArgs GOJJGridShowBlocked(
	TEXT("OJJ.Grid.ShowBlocked"),
	TEXT("불가 셀 오버레이 강제 토글: OJJ.Grid.ShowBlocked 0|1 (빌드모드 무관)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		AOJJ_Grid* Grid = OJJ_FindGridInWorld(World);
		if (!Grid)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Grid] ShowBlocked: 월드에 AOJJ_Grid 없음."));
			return;
		}
		const bool bShow = (Args.Num() == 0) || (Args[0] != TEXT("0"));
		Grid->SetForceShowBlocked(bShow);
	}));

static FAutoConsoleCommandWithWorldAndArgs GOJJGridShowWater(
	TEXT("OJJ.Grid.ShowWater"),
	TEXT("물 셀 오버레이(파랑) 강제 토글: OJJ.Grid.ShowWater 0|1 (빌드모드 무관). WaterSurfaceZ 튜닝 분포 확인용."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		AOJJ_Grid* Grid = OJJ_FindGridInWorld(World);
		if (!Grid)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Grid] ShowWater: 월드에 AOJJ_Grid 없음."));
			return;
		}
		const bool bShow = (Args.Num() == 0) || (Args[0] != TEXT("0"));
		Grid->SetForceShowWater(bShow);
	}));

static FAutoConsoleCommandWithWorldAndArgs GOJJGridGroundZReport(
	TEXT("OJJ.Grid.GroundZReport"),
	TEXT("지형 높이(GroundZ) 캐시 리포트: OJJ.Grid.GroundZReport [X Y [Radius=5]] — 요약+히스토그램, 좌표 지정 시 영역 셀별 덤프."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		AOJJ_Grid* Grid = OJJ_FindGridInWorld(World);
		if (!Grid)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Grid] GroundZReport: 월드에 AOJJ_Grid 없음."));
			return;
		}
		// 인자 검증(Codex F1-a #4-3): 0개(요약만) 또는 2~3개(X Y [Radius])만 허용, 비정수는 거부.
		FIntPoint Center(-1, -1);
		int32 Radius = 0;
		if (Args.Num() == 1 || Args.Num() > 3)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Grid] GroundZReport 사용법: OJJ.Grid.GroundZReport [X Y [Radius=5]]"));
			return;
		}
		if (Args.Num() >= 2)
		{
			int32 X = 0, Y = 0, R = 5;
			if (!LexTryParseString(X, *Args[0]) || !LexTryParseString(Y, *Args[1])
				|| (Args.Num() == 3 && !LexTryParseString(R, *Args[2])))
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[Grid] GroundZReport: 잘못된 인자(정수 필요) — 사용법: OJJ.Grid.GroundZReport [X Y [Radius=5]]"));
				return;
			}
			Center = FIntPoint(X, Y);
			Radius = R;
		}
		Grid->DumpGroundZReport(Center, Radius);
	}));

// === Grid Query (GridManager/컨베이어용 읽기 전용 조회) — 순수 추가, write 경로 미변경 ===

AMachineBase* AOJJ_Grid::GetMachineAtCell(FIntPoint Cell) const
{
	if (const TWeakObjectPtr<AActor>* Found = OccupiedCells.Find(Cell))
	{
		// Get()은 유효하면 actor ptr, GC됐으면 nullptr. Cast로 머신만 좁힘 →
		// 비머신(컨베이어 등) 점유 셀은 nullptr 반환 (의도된 동작; 의미 확정은 1-c).
		return Cast<AMachineBase>(Found->Get());
	}
	return nullptr;
}

AActor* AOJJ_Grid::GetActorAtCell(FIntPoint Cell) const
{
	if (const TWeakObjectPtr<AActor>* Found = OccupiedCells.Find(Cell))
	{
		// Get()은 유효하면 actor ptr, GC됐으면 nullptr. Cast 없이 점유 액터를 그대로 반환.
		return Found->Get();
	}
	return nullptr;
}

bool AOJJ_Grid::IsCellOccupied(FIntPoint Cell) const
{
	// AActor 점유 기준 — 유효한 점유 액터가 있으면 true (컨베이어 셀도 true).
	// GetMachineAtCell(Cast로 머신만 좁힘) 위임을 끊어 의미 분리: "점유 여부" ≠ "머신 존재".
	//   - IsCellOccupied=true / GetMachineAtCell=null  → 컨베이어 등 비머신 점유 (Step 3)
	// 파괴된 액터 셀은 weak IsValid()로 비점유 처리 → 기존 stale 일관성 유지.
	// (현재는 컨베이어 미등록이라 결과는 1-a 이전과 동일 — 머신만 점유.)
	const TWeakObjectPtr<AActor>* Found = OccupiedCells.Find(Cell);
	return Found && Found->IsValid();
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
	// OJJ_ActorToCells는 weak-key(AActor) 맵 — 머신 raw ptr로 조회 가능(암시적 TWeakObjectPtr<AActor> 변환)
	return OJJ_ActorToCells.Find(Machine);
}

FIntPoint AOJJ_Grid::GetMachineOrigin(AMachineBase* Machine) const
{
	// min-recompute 폐기 → 등록 시점에 명시 저장한 OJJ_ActorToOrigin 조회.
	// IsValid 가드로 nullptr/stale 머신 차단 (GetMachineCells와 동일 일관성).
	if (!IsValid(Machine))
	{
		return FIntPoint(INT_MIN, INT_MIN);
	}
	if (const FIntPoint* Origin = OJJ_ActorToOrigin.Find(Machine))
	{
		return *Origin;
	}
	// 미등록 머신 센티넬 (BuildController의 INT_MIN 컨벤션과 일치)
	return FIntPoint(INT_MIN, INT_MIN);
}

// === Grid Conveyor (출력포트 자급 판별 — ssr 포트 시스템 미변경) ===

FIntPoint AOJJ_Grid::CardinalFromVector(FVector V)
{
	// 비유한/거의 0인 XY 입력 방어 → 방향 없음(ZeroValue). public/BlueprintPure라 직접 오용 대비 (Codex 지적).
	// (GetMachineOutputDir 경로는 yaw-only 단위 forward라 정상이지만 외부 직접 호출 보호.)
	const double Mag2 = static_cast<double>(V.X) * V.X + static_cast<double>(V.Y) * V.Y;
	if (!FMath::IsFinite(Mag2) || Mag2 < UE_KINDA_SMALL_NUMBER)
	{
		return FIntPoint::ZeroValue;
	}

	// 우세 축 스냅: |X| >= |Y| 면 X축, 아니면 Y축. 대각선 방지 (Codex 검증 반영).
	// tie(|X|==|Y|, 예: 정확히 45°)는 결정적으로 X 선택. 평면 그리드라 Z 무시.
	if (FMath::Abs(V.X) >= FMath::Abs(V.Y))
	{
		return FIntPoint(V.X >= 0.f ? 1 : -1, 0);
	}
	return FIntPoint(0, V.Y >= 0.f ? 1 : -1);
}

FIntPoint AOJJ_Grid::GetMachineOutputDir(AMachineBase* Machine) const
{
	if (!IsValid(Machine))
	{
		return FIntPoint::ZeroValue;
	}
	// 출력 = 머신 뒤(-Front). 액터 yaw가 source of truth → R키 회전/사전배치 모두 반영.
	const FVector Back = -Machine->GetActorForwardVector();
	return CardinalFromVector(Back);
}

TArray<FIntPoint> AOJJ_Grid::OJJ_PortCellsFromFootprint(const TArray<FIntPoint>& Cells, FIntPoint Dir, int32 PortCount)
{
	TArray<FIntPoint> AllPortCells;

	if (Cells.Num() == 0 || Dir == FIntPoint::ZeroValue)
	{
		return AllPortCells;
	}

	// 1) Dir쪽 모서리 포트 셀 전부 수집: footprint 셀 C 중 (C + Dir)이 footprint 밖이면 그 이웃(C+Dir)이 포트 셀.
	const TSet<FIntPoint> Footprint(Cells);
	for (const FIntPoint& Cell : Cells)
	{
		const FIntPoint Target = Cell + Dir;
		if (!Footprint.Contains(Target))
		{
			AllPortCells.AddUnique(Target);
		}
	}

	const int32 L = AllPortCells.Num();

	// 2) 포트 카운트 미설정(0)/면길이 이상 → 전부 (현행 동일, 리그레션 0).
	if (PortCount <= 0 || PortCount >= L)
	{
		return AllPortCells;
	}

	// 3) 면 축(Dir에 수직)으로 정렬 — 대칭 선택을 위한 결정적 순서. Dir이 X축이면 면은 Y로 변함(키=Y), 아니면 키=X.
	const bool bDirAlongX = (Dir.X != 0);
	AllPortCells.Sort([bDirAlongX](const FIntPoint& A, const FIntPoint& B)
	{
		return bDirAlongX ? (A.Y < B.Y) : (A.X < B.X);
	});

	// 4) 중심축 대칭 균등 분산으로 K개 인덱스 선택.
	TArray<int32> Indices;
	if (PortCount == 1)
	{
		// 단일 포트는 홀수 면에서만 정중앙 가능. 짝수 면이면 대칭 불가 → 아래 검증에서 폴백.
		if ((L % 2) == 1)
		{
			Indices.Add((L - 1) / 2);
		}
	}
	else
	{
		// 양끝(0, L-1) 포함 균등 분산. idx_j = round(j*(L-1)/(K-1)).
		for (int32 j = 0; j < PortCount; ++j)
		{
			const int32 Idx = FMath::RoundToInt(static_cast<float>(j) * (L - 1) / (PortCount - 1));
			Indices.AddUnique(Idx);
		}
	}

	// 5) 검증: 정확히 K개 + 중심축(L-1) 대칭이어야 채택. 아니면 (면길이,포트수)당 1회 경고 + 전부 반환 폴백.
	bool bValid = (Indices.Num() == PortCount);
	if (bValid)
	{
		const TSet<int32> IndexSet(Indices);
		for (int32 Idx : Indices)
		{
			if (!IndexSet.Contains((L - 1) - Idx))
			{
				bValid = false;
				break;
			}
		}
	}

	if (!bValid)
	{
		static TSet<int32> WarnedConfigs;  // 매 프레임/매 도킹 호출 스팸 방지 — 조합당 1회.
		const int32 ConfigKey = L * 1000 + PortCount;
		if (!WarnedConfigs.Contains(ConfigKey))
		{
			WarnedConfigs.Add(ConfigKey);
			UE_LOG(LogTemp, Warning,
				TEXT("[OJJ_Grid] 포트 대칭 배치 불가 (면길이=%d, 포트수=%d) — 전부 반환 폴백."),
				L, PortCount);
		}
		return AllPortCells;
	}

	// 6) 선택 인덱스 → 포트 셀.
	TArray<FIntPoint> Selected;
	Selected.Reserve(Indices.Num());
	for (int32 Idx : Indices)
	{
		Selected.Add(AllPortCells[Idx]);
	}
	return Selected;
}

TArray<FIntPoint> AOJJ_Grid::OJJ_GetMachinePortCells(AMachineBase* Machine, FIntPoint Dir, int32 PortCount) const
{
	const TArray<FIntPoint>* Cells = GetMachineCells(Machine);  // 내부 IsValid 가드
	if (!Cells)
	{
		return TArray<FIntPoint>();
	}

	// 등록 머신의 footprint를 공유 모서리 워크 + 대칭 규칙에 위임(호버 프리뷰·도킹과 동일 규칙).
	return OJJ_PortCellsFromFootprint(*Cells, Dir, PortCount);
}

TArray<FIntPoint> AOJJ_Grid::GetMachineOutputCells(AMachineBase* Machine) const
{
	// 출력 = OutputDir(-Front) 방향 포트 셀. 출력 포트수로 대칭 배치 적용.
	return Machine
		? OJJ_GetMachinePortCells(Machine, GetMachineOutputDir(Machine), Machine->GetOutputPortCount())
		: TArray<FIntPoint>();
}

FIntPoint AOJJ_Grid::OJJ_GetMachineInputDir(AMachineBase* Machine) const
{
	// 입력 = 앞면(+Front) = 출력(-Front)의 부호 반전. 무효 머신은 출력이 (0,0)이라 반전해도 (0,0).
	const FIntPoint Out = GetMachineOutputDir(Machine);
	return FIntPoint(-Out.X, -Out.Y);
}

TArray<FIntPoint> AOJJ_Grid::OJJ_GetMachineInputCells(AMachineBase* Machine) const
{
	// 입력 = InputDir(+Front) 방향 포트 셀. 출력 셀과 같은 헬퍼 공유, 방향만 반전 + 입력 포트수로 대칭 배치.
	return Machine
		? OJJ_GetMachinePortCells(Machine, OJJ_GetMachineInputDir(Machine), Machine->GetInputPortCount())
		: TArray<FIntPoint>();
}

TArray<AMachineBase*> AOJJ_Grid::GetMachineOutputTargets(AMachineBase* Machine) const
{
	TArray<AMachineBase*> Targets;
	for (const FIntPoint& Cell : GetMachineOutputCells(Machine))
	{
		if (AMachineBase* Target = GetMachineAtCell(Cell))  // 유효(weak Get)만 반환
		{
			if (Target != Machine)  // self 제외 (이론상 불가하나 방어)
			{
				Targets.AddUnique(Target);
			}
		}
	}
	return Targets;
}

// === Conveyor 인지 (Step 3-a — 셀 등록/조회만, 경로·포트 유효성은 3-c) ===

AConveyor* AOJJ_Grid::OJJ_GetConveyorAtCell(FIntPoint Cell) const
{
	if (const TWeakObjectPtr<AActor>* Found = OccupiedCells.Find(Cell))
	{
		// weak Get()은 stale이면 nullptr. Cast로 컨베이어만 좁힘 → 머신/비컨베이어 셀은 nullptr.
		return Cast<AConveyor>(Found->Get());
	}
	return nullptr;
}

bool AOJJ_Grid::OJJ_RegisterActorCells(AActor* Actor, const TArray<FIntPoint>& Cells)
{
	if (!HasAuthority())
	{
		ensureMsgf(false, TEXT("OJJ_RegisterActorCells called on non-authority"));
		return false;
	}

	SweepStaleEntries();

	if (!IsValid(Actor) || Cells.Num() == 0)
	{
		return false;
	}

	// 머신은 이 경로 금지 — 머신은 RegisterMachineInternal(footprint/bounds/origin 불변식) 경로로만 등록한다.
	// 이 API는 컨베이어 등 비머신 actor 전용. 머신을 넣으면 머신 불변식을 우회하므로 거부(Codex #3).
	if (Cast<AMachineBase>(Actor))
	{
		return false;
	}

	if (OJJ_ActorToCells.Contains(Actor))
	{
		// 이미 등록된 actor — 중복 등록 금지(이동/갱신은 별도 경로).
		return false;
	}

	// 중복 셀 제거(set 의미 보장) — 충돌 검사·등록 모두 dedup된 목록으로 수행(Codex #2).
	TArray<FIntPoint> UniqueCells;
	UniqueCells.Reserve(Cells.Num());
	for (const FIntPoint& Cell : Cells)
	{
		UniqueCells.AddUnique(Cell);
	}

	// 데이터 무결성 가드: off-grid 셀 등록 차단 + 다른 유효 actor가 이미 점유한 셀이면 거부(양방향 맵 corruption 방지).
	// ※ 경로 연속성·포트 정합 등 placement 유효성은 컨베이어 경로 검증(OJJ_CollectConveyorReservedCells) 담당.
	//    여기선 bounds + 점유 충돌만 — 직접 호출(경로 밖) 시에도 off-grid/겹침 등록을 막는 방어(Codex #4).
	for (const FIntPoint& Cell : UniqueCells)
	{
		if (!IsValidGridCell(Cell))
		{
			return false;
		}

		const TWeakObjectPtr<AActor>* Found = OccupiedCells.Find(Cell);
		if (Found && Found->IsValid() && Found->Get() != Actor)
		{
			return false;
		}
	}

	// origin = 등록 셀의 lower-left(min corner). 머신과 동일 컨벤션으로 OJJ_ActorToOrigin 동기 유지.
	FIntPoint Origin = UniqueCells[0];
	for (const FIntPoint& Cell : UniqueCells)
	{
		Origin.X = FMath::Min(Origin.X, Cell.X);
		Origin.Y = FMath::Min(Origin.Y, Cell.Y);
		OccupiedCells.Add(Cell, Actor);
	}
	OJJ_ActorToOrigin.Add(Actor, Origin);
	OJJ_ActorToCells.Add(Actor, MoveTemp(UniqueCells));

	// 컨베이어 등 actor가 포트 셀을 점유하면 해당 포트 화살표가 숨겨져야 하므로 빌드모드 중 재계산.
	if (bPlacedArrowsVisible)
	{
		RefreshPlacedMachineArrows();
	}

	return true;
}

bool AOJJ_Grid::OJJ_RemoveActorAt(FIntPoint Cell)
{
	if (!HasAuthority())
	{
		ensureMsgf(false, TEXT("OJJ_RemoveActorAt called on non-authority"));
		return false;
	}

	const TWeakObjectPtr<AActor>* Found = OccupiedCells.Find(Cell);
	if (!Found || !Found->IsValid())
	{
		return false;
	}

	AActor* Actor = Found->Get();
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFactoryManagerSubsystem* FactoryManager = GameInstance->GetSubsystem<UFactoryManagerSubsystem>())
		{
			if (AConveyor* Conveyor = Cast<AConveyor>(Actor))
			{
				FactoryManager->UnregisterConveyor(Conveyor);
			}
		}
	}

	const TArray<FIntPoint>* ActorCells = OJJ_ActorToCells.Find(Actor);
	if (!ActorCells)
	{
		// 불변식 위반: OccupiedCells엔 있는데 역맵(OJJ_ActorToCells)엔 없음.
		// 어중간한 부분 제거 대신 — 그 actor가 점유한 모든 OccupiedCells를 스캔 제거 + origin 제거(완전 정리).
		// 불변식 깨짐을 로그로 가시화(Codex #1/#5).
		UE_LOG(LogTemp, Warning,
			TEXT("[OJJ_Grid] OJJ_RemoveActorAt: OccupiedCells/OJJ_ActorToCells 불일치 — actor '%s'를 전체 스캔으로 정리."),
			*Actor->GetName());
		for (auto It = OccupiedCells.CreateIterator(); It; ++It)
		{
			if (It.Value().Get() == Actor)
			{
				It.RemoveCurrent();
			}
		}
		OJJ_ActorToOrigin.Remove(Actor);

		// 비정상 복구 경로에서도 포트 셀 점유가 풀렸을 수 있으므로 화살표 시각 일관성 유지(Codex 리뷰 Low).
		if (bPlacedArrowsVisible)
		{
			RefreshPlacedMachineArrows();
		}
		return false;
	}

	for (const FIntPoint& C : *ActorCells)
	{
		OccupiedCells.Remove(C);
	}
	OJJ_ActorToCells.Remove(Actor);
	OJJ_ActorToOrigin.Remove(Actor);

	// 컨베이어 제거로 포트 셀 점유가 풀리면 숨겼던 포트 화살표가 복귀해야 하므로 빌드모드 중 재계산.
	if (bPlacedArrowsVisible)
	{
		RefreshPlacedMachineArrows();
	}

	return true;
}

bool AOJJ_Grid::OJJ_BuildConveyorPlacementPath(
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

	if (AMachineBase* StartMachine = OJJ_GetMachineAtCell(OccupiedCells, StartCell))
	{
		const TArray<FIntPoint>* MachineCells = OJJ_ActorToCells.Find(StartMachine);
		const FIntPoint OutsideCell = StartCell + OJJ_GetMachineBackStep(StartMachine);
		if (!MachineCells || !OJJ_IsMachineBackOutputPair(this, StartMachine, StartCell, OutsideCell, *MachineCells))
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
		for (const FIntPoint& Step : OJJ_NeighborSteps)
		{
			const FIntPoint MachineCell = StartCell - Step;
			AMachineBase* AdjacentMachine = OJJ_GetMachineAtCell(OccupiedCells, MachineCell);
			if (!AdjacentMachine)
			{
				continue;
			}

			bSawAdjacentMachine = true;
			const TArray<FIntPoint>* MachineCells = OJJ_ActorToCells.Find(AdjacentMachine);
			if (MachineCells
				&& OJJ_IsMachineBackOutputPair(this, AdjacentMachine, MachineCell, StartCell, *MachineCells))
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
	return OJJ_CollectConveyorReservedCells(this, OccupiedCells, OJJ_ActorToCells, OutPathCells, ReservedCells, OutReason);
}

bool AOJJ_Grid::OJJ_CanPlaceConveyorPath(const TArray<FIntPoint>& PathCells) const
{
	TArray<FIntPoint> ReservedCells;
	FString OutReason;
	return OJJ_CollectConveyorReservedCells(this, OccupiedCells, OJJ_ActorToCells, PathCells, ReservedCells, OutReason);
}

bool AOJJ_Grid::OJJ_TryPlaceConveyor(AConveyor* Conveyor, const TArray<FIntPoint>& PathCells, FString& OutReason)
{
	TArray<FIntPoint> PlacementCells;
	if (!OJJ_BuildConveyorPlacementPath(PathCells, PlacementCells, OutReason))
	{
		return false;
	}

	TArray<FIntPoint> ReservedCells;
	AMachineBase* SourceMachine = nullptr;
	AMachineBase* TargetMachine = nullptr;
	if (!OJJ_CollectConveyorReservedCells(
		this,
		OccupiedCells,
		OJJ_ActorToCells,
		PlacementCells,
		ReservedCells,
		OutReason,
		&SourceMachine,
		&TargetMachine))
	{
		return false;
	}

	if (ReservedCells.Num() == 0)
	{
		OutReason = TEXT("Conveyor must occupy at least one grid cell.");
		return false;
	}

	if (!SourceMachine || !TargetMachine)
	{
		OutReason = TEXT("Conveyor item transfer requires valid machine endpoints.");
		return false;
	}

	// 등록 실패 시 OJJ_RegisterActorCells가 부작용 없이 false 반환(가드에서 조기 종료) → 별도 롤백 불필요.
	// 등록 성공 후 Conveyor 호출(SetActorLocation/SetPath/ConfigureTransport)은 void·비실패라 롤백 지점 없음(Dummy parity).
	if (!OJJ_RegisterActorCells(Conveyor, ReservedCells))
	{
		OutReason = TEXT("Failed to register conveyor cells on the grid.");
		return false;
	}

	// SetPath로 PathCells를 먼저 채운 뒤 centroid를 계산해야 하므로 순서 주의(SetPath → SetActorLocation).
	// 피벗을 belt centroid로 옮겨도 belt 월드위치는 불변(로컬에서 centroid를 차감하므로 상쇄).
	// centroid는 액터 로컬 오프셋이므로 액터 회전을 적용해 월드 방향으로 변환(무회전에선 항등, 미래 회전 대비).
	Conveyor->SetPath(PlacementCells, CellSize);
	// [OJJ F3.9] 포트 꺾임 방향 주입: 끝 셀이 머신 위면(끝 스텝 = 포트 법선 — 꺾임이 경로 안이라
	// 기존 코너 생성) Zero, 인접 끝이면 타깃 −FrontStep = BackStep(포트 셀 → 머신 안 —
	// OJJ_IsMachineFrontInputPair의 EndCell = MachineCell + FrontStep 계약에서 도출)을 전달 →
	// 옆 접근 시 벨트 끝 세그먼트가 코너로 표현됨. **시작 측은 도달 불가 방어 잔여**(Codex F3.9 ④):
	// OJJ_BuildConveyorPlacementPath가 인접 시작에 머신 셀을 선두 삽입(:2556)해 정규화하므로 유효
	// 경로는 항상 머신 셀로 시작 — 옆 출구 꺾임은 경로 안이라 기존 코너가 처리, 주입은 항상 Zero.
	const bool bStartsOnMachine = OJJ_GetMachineAtCell(OccupiedCells, PlacementCells[0]) != nullptr;
	const bool bEndsOnMachineCell = OJJ_GetMachineAtCell(OccupiedCells, PlacementCells.Last()) != nullptr;
	Conveyor->OJJ_SetPortFlowDirections(
		bStartsOnMachine ? FIntPoint::ZeroValue : OJJ_GetMachineBackStep(SourceMachine),
		bEndsOnMachineCell ? FIntPoint::ZeroValue : OJJ_GetMachineBackStep(TargetMachine));
	// 센터화(GridToWorld −half) 정합 보정: 컨베이어 벨트(Conveyor.cpp, 타 소유)는 cell→local을 GridToWorld
	// 미경유 수동식(Cell*CellSize + 0.5*CellSize)으로 계산해 옛 좌표(원점 +X/+Y)에 놓인다. 벨트는 손대지 않고
	// 그리드측에서 액터 앵커를 −half extent 만큼 옮겨 다른 시스템(머신/호버/포트)과 동일 좌표로 정렬.
	// (belt 로컬 오프셋은 선형·centroid 상대라 앵커 평행이동만으로 전체 belt가 정확히 −half 시프트됨.)
	// ※ 근본 해결은 belt가 Grid->GridToWorld를 쓰는 것 — belt 소유자(SSR/Chan) 리뷰 대상으로 태그.
	const FVector HalfExtent(GridSize.X * CellSize * 0.5f, GridSize.Y * CellSize * 0.5f, 0.0f);
	FVector ConveyorLocation =
		GetActorLocation() + Conveyor->GetActorRotation().RotateVector(Conveyor->GetPathCentroidLocal()) - HalfExtent;
	// F1-c: 경로의 단일 건설면 Z 적용(§7-3 2지점 중 ②) — Foundation 위 경로면 상면 높이로 들어올림.
	// 균일/경사는 OJJ_CollectConveyorReservedCells가 이미 검증(실패 시 여기 도달 안 함).
	float PathSurfaceZ = GetActorLocation().Z;
	if (OJJ_GetUniformSurfaceZ(PlacementCells, PathSurfaceZ))
	{
		// 평면 경로(기존 전부) — 기존 코드 그대로(F3.7 회귀 0 §3-1). 지형 경로면 델타 0.
		ConveyorLocation.Z += PathSurfaceZ - GetActorLocation().Z;
		Conveyor->SetActorLocation(ConveyorLocation);
	}
	else
	{
		// F3.7-1: 경사 경로 — 기준 = 시작 셀 SurfaceZ(㊇, 소스 머신 포트 정합), 셀별 델타는 벨트
		// 로컬 Z로 주입(㊃). 배열은 SetPath **이후의 최종 PathCells** 기준 1:1(SetPath가 연속 중복을
		// 제거하므로 — Codex F3.7-0 ByCell -1 무음 폴백 불변식). 재검증 실패는 도달 불가지만
		// 방어적으로 평면 폴백(주입 생략 — 벨트는 빈 배열 = 평면).
		const TArray<FIntPoint> FinalPathCells = Conveyor->GetPathCells();
		TArray<float> CellZs;
		FString UnusedSlopeReason;
		if (OJJ_ValidateConveyorSlopePath(this, FinalPathCells, CellZs, UnusedSlopeReason)
			&& CellZs.Num() > 0)
		{
			// F3.8'' 벨트 Z 경계: 장부(FoundationCells.SurfaceZ — 셀 계단)는 배치 검증/걸침/걷기의
			// 진실원으로 **무변경**. 벨트 비주얼 Z만 면(램프 빗변 — OJJ_GetVisualSurfaceZAtWorld
			// 가상 훅, 쐐기 꼭짓점과 수식 단일원)을 소비(평면 경로는 이 분기 비진입 — 회귀 0).
			// F3.8''' 노드화: 주입 단위 = 셀 경계 노드(N+1) — 면의 꺾임점은 항상 셀 경계(램프
			// 풋프린트 변 = 셀 정렬)라 경계 샘플이면 세그먼트 체인이 면과 전 구간 정확 일치
			// (전환부 현-코너 부유 해소 — 세그먼트 분할 불필요, 개수 불변). 셀 중심 Z는 벨트가
			// 노드 중점으로 도출(셀 내 면 선형 — 아이템 보간 자동 수혜). 양 끝 노드 = 끝 셀
			// 장부값(머신 포트 앵커 — Codex F3.8'' ③ 유지), 내부 노드 = 인접 셀의 면 훅
			// (양쪽 다 평판/브리지면 장부 중점 = 평탄).
			auto QueryFaceZ = [this](FIntPoint Cell, const FVector& WorldPos, float& OutFaceZ) -> bool
			{
				const FOJJFoundationCellInfo* CellInfo = FoundationCells.Find(Cell);
				const AOJJ_Foundation* CellFoundation = CellInfo
					? Cast<AOJJ_Foundation>(CellInfo->Foundation.Get())
					: nullptr;
				return CellFoundation && CellFoundation->OJJ_GetVisualSurfaceZAtWorld(WorldPos, OutFaceZ);
			};

			const int32 CellCount = FinalPathCells.Num();
			TArray<float> NodeZs;
			NodeZs.SetNumUninitialized(CellCount + 1);
			NodeZs[0] = CellZs[0];
			NodeZs[CellCount] = CellZs[CellCount - 1];
			for (int32 Node = 1; Node < CellCount; ++Node)
			{
				const FVector BoundaryPos =
					(GridToWorld(FinalPathCells[Node - 1]) + GridToWorld(FinalPathCells[Node])) * 0.5;
				const float ChordZ = 0.5f * (CellZs[Node - 1] + CellZs[Node]); // 구(셀 중심 체인)의 경계값
				// 첫 성공 우선(이전 셀 → 다음 셀). 두 램프 면이 같은 경계에서 어긋나는 구성(다른
				// 단의 끝 행이 맞닿음 = 턱이 실존)은 어느 쪽을 채택해도 한쪽 면과는 불일치 —
				// 세그먼트가 그 턱을 현으로 가로지르는 기존 거동 유지(수용, Codex F3.8''' ⑤).
				float FaceZ = 0.0f;
				if (!QueryFaceZ(FinalPathCells[Node - 1], BoundaryPos, FaceZ)
					&& !QueryFaceZ(FinalPathCells[Node], BoundaryPos, FaceZ))
				{
					FaceZ = ChordZ; // 양쪽 다 평판/브리지 — 면=장부.
				}
				NodeZs[Node] = FaceZ;
			}

			const float BaseZ = NodeZs[0];
			for (float& NodeZ : NodeZs)
			{
				NodeZ -= BaseZ;
			}
			ConveyorLocation.Z += BaseZ - GetActorLocation().Z;
			Conveyor->SetActorLocation(ConveyorLocation);
			Conveyor->OJJ_SetPathNodeLocalZs(NodeZs);
		}
		else
		{
			Conveyor->SetActorLocation(ConveyorLocation);
		}
	}
	Conveyor->ConfigureTransport(ReservedCells, SourceMachine, TargetMachine);
	return true;
}

void AOJJ_Grid::OJJ_UpdateConveyorPathHoverPreview(const TArray<FIntPoint>& PathCells)
{
	ClearHoverPreview();

	if (PathCells.Num() == 0)
	{
		return;
	}

	TArray<FIntPoint> PreviewCells;
	FString OutReason;
	const bool bCanPlace = OJJ_BuildConveyorPlacementPath(PathCells, PreviewCells, OutReason);
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
		const FVector InstanceLocation(CellCenter.X, CellCenter.Y, OJJ_GetCellVisualBaseZ(Cell) + 2.0f + HoverExtraZLift);
		const FVector InstanceScale(CellSize / 100.0f, CellSize / 100.0f, 1.0f);
		const FTransform InstanceTransform(FRotator::ZeroRotator, InstanceLocation, InstanceScale);
		TargetISM->AddInstance(InstanceTransform, /*bWorldSpace=*/true);
	}
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
	const TArray<FIntPoint> Footprint = CalculateFootprint(Machine, Origin, RotationSteps);
	for (const FIntPoint& Cell : Footprint)
	{
		if (!IsValidGridCell(Cell))
		{
			return false;
		}

		// 건설 게이트(F1-c: buildable OR Foundation 커버) — 호버도 같은 함수라 자동 빨강.
		if (!IsCellConstructible(Cell))
		{
			return false;
		}

		// AActor 점유 기준 (현재 머신만 담기므로 동작 무변경; 컨베이어 차단 의미 확정은 1-c).
		const TWeakObjectPtr<AActor>* Found = OccupiedCells.Find(Cell);
		if (Found && Found->IsValid())
		{
			return false;
		}
	}

	// 단일 건설면 규칙(F1-c §7-3): 풋프린트가 Foundation 경계에 걸치거나 이높이 면에 걸치면 Z 안착이
	// 모호 — 거부(호버 빨강 자동). 전부 지형이면 평면 Z로 항상 통과(기존 직배치 비파괴).
	float UnusedUniformZ = 0.0f;
	if (!OJJ_GetUniformSurfaceZ(Footprint, UnusedUniformZ))
	{
		return false;
	}

	// 머신별 추가 제약(인접 광맥/수원 등). 그리드는 머신 종류를 모른 채 위임만 — 머신이 오버라이드.
	// 호버(UpdateHoverPreview)·배치(RegisterMachineInternal) 모두 이 함수를 거치므로 색 판정과 실제 배치가 일치.
	if (!Machine->CanPlaceAdditional(this, Origin, RotationSteps))
	{
		return false;
	}

	return true;
}

void AOJJ_Grid::SweepStaleEntries()
{
	for (auto It = OJJ_ActorToCells.CreateIterator(); It; ++It)
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
			// origin 맵도 동일 키로 정리 (양방향 일관성 — 1-a 신설 맵 누수 방지)
			OJJ_ActorToOrigin.Remove(It.Key());
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

	if (OJJ_ActorToCells.Contains(Machine))
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
	OJJ_ActorToCells.Add(Machine, MoveTemp(Footprint));
	// origin 명시 저장 (min-recompute 대체) — GetMachineOrigin이 이 값을 조회.
	OJJ_ActorToOrigin.Add(Machine, Origin);

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

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFactoryManagerSubsystem* FactoryManager = GameInstance->GetSubsystem<UFactoryManagerSubsystem>())
		{
			FactoryManager->NotifyMachineChanged(Machine);
		}
	}

	// 배치 확정 훅 — 자원 선점 등(채굴기/펌프). SetActorLocation 성공 이후라 머신 위치도 최종 상태.
	Machine->OnPlacedOnGrid(this, Origin, RotationSteps);

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

	if (!RegisterMachineInternal(Machine, Origin, OutReason))
	{
		return false;
	}

	// 사전 배치 머신도 배치 확정 훅을 받아야 자원 선점 등이 일관되게 동작(TryPlaceMachine과 대칭).
	// 회전 step은 사전 배치 경로에 없으므로 0.
	Machine->OnPlacedOnGrid(this, Origin, 0);
	return true;
}

void AOJJ_Grid::SetVisualizationVisible(bool bVisible)
{
	if (!GridFloorMesh)
	{
		return;
	}

	// 그리드 비주얼은 per-cell ISM(가능/blocked)가 담당 → 플레인 메시는 시각적으로 항상 숨김.
	// 단 커서 라인트레이스 대상(컴포넌트 식별)이므로 충돌은 빌드모드에서 유지(아래).
	GridFloorMesh->SetVisibility(false);

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

	// 빌드모드 시각화 상태 기록 — RefreshGridVisual/콘솔 토글이 참조.
	bVisualizationActive = bVisible;

	// per-cell 그리드 비주얼 갱신(가능=초록/blocked=빨강/void=없음). 진입 시 채우고 퇴장 시 비움(강제표시 중이면 blocked 유지).
	RefreshGridVisual();
}

void AOJJ_Grid::UpdateHoverPreview(AMachineBase* Machine, FIntPoint Origin, int32 RotationSteps)
{
	ClearHoverPreview();

	if (!Machine)
	{
		return;
	}

	// 포트 화살표 갱신 — 배치 머신 전체(상시) + 현재 호버 프리뷰. UpdateHoverPreview는 셀 변경/회전/
	// 배치 등 "리빌드" 시에만 호출되므로(UpdateMouseHover가 동일 셀이면 스킵) 매 프레임 비용이 아니다.
	// ClearHoverPreview가 위에서 이전 호버 화살표를 비운 상태 → 여기서 현재 step 기준으로 재적재.
	RefreshPlacedMachineArrows();
	DrawHoverMachineArrows(Machine, Origin, RotationSteps);

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
		// 기준면(F1-c: GroundZ/Foundation 추종) +2 + 호버 추가 리프트 → 분류 오버레이 위에서 식별
		const FVector CellCenter = GridToWorld(Cell);
		const FVector InstanceLocation(CellCenter.X, CellCenter.Y, OJJ_GetCellVisualBaseZ(Cell) + 2.0f + HoverExtraZLift);

		// Plane(100x100) → CellSize 유닛으로 스케일
		const FVector InstanceScale(CellSize / 100.0f, CellSize / 100.0f, 1.0f);
		const FTransform InstanceTransform(FRotator::ZeroRotator, InstanceLocation, InstanceScale);

		// World-space 좌표로 추가 (액터 위치 무관)
		TargetISM->AddInstance(InstanceTransform, /*bWorldSpace=*/true);
	}
}

void AOJJ_Grid::ClearHoverPreview()
{
	// 호버 진입점 공통 chokepoint — 전용 호버 MID 보장(없으면 생성, 있으면 현재 값 재적용 → PIE 실시간 튜닝).
	OJJ_EnsureTileMIDs();

	if (ValidHoverISM)
	{
		ValidHoverISM->ClearInstances();
	}
	if (InvalidHoverISM)
	{
		InvalidHoverISM->ClearInstances();
	}

	// 호버 셀 ISM과 동반 생멸 — 커서가 유효 셀을 떠나면(트레이스 실패/off-grid/퇴장) 호버 화살표도 사라짐.
	ClearHoverMachineArrows();
}

void AOJJ_Grid::OJJ_SetTileParams(UMaterialInstanceDynamic* MID, const FLinearColor& FillColor, float FillOpacity) const
{
	if (!MID)
	{
		return;
	}
	// M_OJJ_GridFloor(Unlit) 노출 파라미터: 채움=BaseColor/Opacity, 선=LineColor/LineOpacity/LineWidth.
	// 머티리얼이 WorldPosition 기반으로 셀 경계선(line)과 내부 채움(fill)을 분리해 합성하므로 둘을 독립 구동:
	//  - 채움: 분류 의미(빨강/초록/파랑) — 연하게(FillOpacity).
	//  - 선  : 스냅 기준선 — 공유 GridLineColor/GridLineOpacity로 항상 선명(채움 투명도와 무관).
	// LineWidth는 베이스 MI 기본값 유지(미설정). 없는 파라미터 set은 안전 무시.
	MID->SetVectorParameterValue(TEXT("BaseColor"), FillColor);
	MID->SetScalarParameterValue(TEXT("Opacity"), FillOpacity);
	MID->SetVectorParameterValue(TEXT("LineColor"), GridLineColor);
	MID->SetScalarParameterValue(TEXT("LineOpacity"), GridLineOpacity);
}

void AOJJ_Grid::OJJ_ApplyTileMIDParams()
{
	// 호버(주인공): 높은 불투명 + 선명/에미시브.
	OJJ_SetTileParams(ValidHoverMID, HoverValidColor, HoverOpacity);
	OJJ_SetTileParams(InvalidHoverMID, HoverInvalidColor, HoverOpacity);
	// 오버레이(정보): 낮은 불투명 + 차분.
	OJJ_SetTileParams(BuildableCellMID, OverlayBuildableColor, OverlayOpacity);
	OJJ_SetTileParams(BlockedCellMID, OverlayBlockedColor, OverlayOpacity);
	OJJ_SetTileParams(WaterCellMID, OverlayWaterColor, OverlayOpacity);
	// 캐릭터 셀(정보 계층 — 오버레이 불투명 공유, 색으로 구분).
	OJJ_SetTileParams(CharacterCellMID, CharacterCellColor, OverlayOpacity);
}

void AOJJ_Grid::OJJ_EnsureTileMIDs()
{
	// 각 용도별 전용 MID를 lazy 생성하고 ISM에 할당 — 오버레이/호버 머티리얼 공유(색 섞임 원인) 차단.
	// 모두 같은 translucent Unlit 베이스(M_OJJ_GridFloor 파생)라 색/불투명만 다르게 구동.
	auto EnsureMID = [this](UInstancedStaticMeshComponent* ISM, TObjectPtr<UMaterialInstanceDynamic>& MID, UMaterialInterface* Base)
	{
		if (ISM && !MID && Base)
		{
			MID = UMaterialInstanceDynamic::Create(Base, this);
			if (MID)
			{
				// 런타임 전용 — outer가 레벨 액터라 저장 대상이 되면 레벨 dirty(가짜 diff) 유발(F2-0).
				MID->SetFlags(RF_Transient);
				ISM->SetMaterial(0, MID);
			}
		}
	};

	EnsureMID(ValidHoverISM, ValidHoverMID, HoverValidBaseMaterial);
	EnsureMID(InvalidHoverISM, InvalidHoverMID, HoverInvalidBaseMaterial);
	EnsureMID(BuildableCellISM, BuildableCellMID, HoverValidBaseMaterial);
	EnsureMID(BlockedCellISM, BlockedCellMID, HoverInvalidBaseMaterial);
	// 물은 별도 색(파랑)을 OverlayWaterColor로 덮어쓰므로 어느 베이스든 무방 — Valid 베이스 재사용.
	EnsureMID(WaterCellISM, WaterCellMID, HoverValidBaseMaterial);
	EnsureMID(CharacterCellISM, CharacterCellMID, HoverValidBaseMaterial);
	// 커버 셀(F3.5')은 buildable과 의미 동일(초록) — MID 공유(멱등 SetMaterial).
	if (CoveredCellISM && BuildableCellMID)
	{
		CoveredCellISM->SetMaterial(0, BuildableCellMID);
	}

	// 생성 직후 + 매 호출 현재 값 재적용(멱등) → PIE에서 프로퍼티 바꾸면 다음 호버/갱신에 반영.
	OJJ_ApplyTileMIDParams();
}

#if WITH_EDITOR
void AOJJ_Grid::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	// 디테일 패널/PIE에서 Visual Hierarchy 색·불투명을 만지면 즉시 MID에 재적용 + 오버레이 리빌드(실시간 튜닝).
	OJJ_EnsureTileMIDs();
	RefreshGridVisual();
}
#endif

const TArray<FIntPoint>* AOJJ_Grid::GetActorCells(AActor* Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}
	// 머신/컨베이어 등 OccupiedCells에 등록된 모든 액터의 footprint를 범용 조회(GetMachineCells의 비머신 포함판).
	return OJJ_ActorToCells.Find(Actor);
}

void AOJJ_Grid::OJJ_HighlightCellsInvalid(const TArray<FIntPoint>& Cells)
{
	// 기존 호버 프리뷰(배치 ISM/화살표)를 먼저 비우고 철거 대상만 빨강으로 표시 — 상태 혼선 방지.
	ClearHoverPreview();

	UInstancedStaticMeshComponent* TargetISM = InvalidHoverISM.Get();
	if (!TargetISM)
	{
		return;
	}

	// 배치 호버(UpdateHoverPreview)와 동일한 셀→인스턴스 규칙(기준면+2+호버 리프트, Plane 100→CellSize 스케일, world-space).
	for (const FIntPoint& Cell : Cells)
	{
		const FVector CellCenter = GridToWorld(Cell);
		const FVector InstanceLocation(CellCenter.X, CellCenter.Y, OJJ_GetCellVisualBaseZ(Cell) + 2.0f + HoverExtraZLift);
		const FVector InstanceScale(CellSize / 100.0f, CellSize / 100.0f, 1.0f);
		const FTransform InstanceTransform(FRotator::ZeroRotator, InstanceLocation, InstanceScale);
		TargetISM->AddInstance(InstanceTransform, /*bWorldSpace=*/true);
	}
}

bool AOJJ_Grid::OJJ_IsExtractionMachine(const AMachineBase* Machine)
{
	if (!Machine)
	{
		return false;
	}

	// TODO(SSR 협의): 문자열 비교 대신 AMachineBase 가상 predicate(예: UsesConveyorInput())로 대체.
	// 추출 머신은 입력을 인접 자원 노드(광맥/수원/공기)에서 받으므로 컨베이어 입력 포트가 없다 → 입력 화살표 생략.
	const FName Type = Machine->GetMachineType();
	return Type == TEXT("MinerMachine") || Type == TEXT("Pump") || Type == TEXT("AirCompressor");
}

void AOJJ_Grid::OJJ_EmitPortArrows(
	UInstancedStaticMeshComponent* InputISM, bool bDrawInput, const TArray<FIntPoint>& InputCells, FIntPoint InputDir,
	UInstancedStaticMeshComponent* OutputISM, bool bDrawOutput, const TArray<FIntPoint>& OutputCells, FIntPoint OutputDir) const
{
	auto EmitOne = [this](UInstancedStaticMeshComponent* ISM, FIntPoint Cell, FIntPoint FacingDir)
	{
		if (!ISM || FacingDir == FIntPoint::ZeroValue)
		{
			return;
		}

		// 연결된 포트 숨김: 포트 셀에 컨베이어가 점유 중이면 그 포트 화살표를 그리지 않는다.
		// bIsConnected(머신↔머신 직접 포트 연결, SSR 소유)는 컨베이어와 무관하므로 기하 판정 사용 —
		// 그리드 점유 진실원(OJJ_GetConveyorAtCell)을 직접 조회. 컨베이어 제거 시 점유가 풀려 화살표 복귀.
		if (OJJ_GetConveyorAtCell(Cell))
		{
			return;
		}

		const FVector Dir3D = FVector(FacingDir.X, FacingDir.Y, 0.0f).GetSafeNormal();
		if (Dir3D.IsNearlyZero())
		{
			return;
		}

		const FVector CellCenter = GridToWorld(Cell);
		// F1-c: 기준면 추종 — Foundation 위 머신의 화살표가 평면 높이에 묻히지 않게(머신 Z 안착과 동일 데이터).
		const FVector Location(CellCenter.X, CellCenter.Y, OJJ_GetCellVisualBaseZ(Cell) + PortArrowHeightOffset);

		// 콘 메시 apex(+Z)를 수평 FacingDir로 정렬.
		const FRotator Rotation = FRotationMatrix::MakeFromZ(Dir3D).Rotator();
		const FTransform InstanceTransform(Rotation, Location, FVector(PortArrowScale));

		ISM->AddInstance(InstanceTransform, /*bWorldSpace=*/true);
	};

	if (bDrawInput)
	{
		// 입력 화살표: 머신을 향해(−InputDir) — "입력 셀 → 머신".
		const FIntPoint InputFacing(-InputDir.X, -InputDir.Y);
		for (const FIntPoint& Cell : InputCells)
		{
			EmitOne(InputISM, Cell, InputFacing);
		}
	}

	if (bDrawOutput)
	{
		// 출력 화살표: 머신에서 나가는(+OutputDir) — "머신 → 출력 셀".
		for (const FIntPoint& Cell : OutputCells)
		{
			EmitOne(OutputISM, Cell, OutputDir);
		}
	}
}

void AOJJ_Grid::RefreshPlacedMachineArrows()
{
	ClearPlacedMachineArrows();

	for (const TPair<TWeakObjectPtr<AActor>, TArray<FIntPoint>>& Pair : OJJ_ActorToCells)
	{
		AMachineBase* Machine = Cast<AMachineBase>(Pair.Key.Get());  // 컨베이어/stale 제외
		if (!Machine)
		{
			continue;
		}

		const bool bDrawInput = Machine->GetInputPortCount() > 0 && !OJJ_IsExtractionMachine(Machine);
		const bool bDrawOutput = Machine->GetOutputPortCount() > 0;

		// 등록 머신: 기존 포트 함수(액터 yaw 기반)를 그대로 재사용 — 컨베이어 연결 판정과 자동 일치.
		OJJ_EmitPortArrows(
			PlacedInputArrowISM, bDrawInput, OJJ_GetMachineInputCells(Machine), OJJ_GetMachineInputDir(Machine),
			PlacedOutputArrowISM, bDrawOutput, GetMachineOutputCells(Machine), GetMachineOutputDir(Machine));
	}

	// 빌드모드 활성(=화살표 표시 중) 표식 — RemoveMachine의 stale 정리 가드용.
	bPlacedArrowsVisible = true;
}

void AOJJ_Grid::DrawHoverMachineArrows(AMachineBase* Machine, FIntPoint Origin, int32 RotationSteps)
{
	ClearHoverMachineArrows();

	if (!Machine)
	{
		return;
	}

	// 호버 프리뷰는 머신 액터를 spawn하지 않으므로(OJJ_BuildController 주석 참조) forward 벡터가 없다.
	// 배치 컨벤션(OJJ_BuildController가 SetActorRotation(0, 90*step, 0) 적용)과 동일하게 yaw로 재구성 →
	// 미리보기 화살표 방향이 실제 배치 결과와 정확히 일치.
	const FVector Forward = FRotator(0.0f, 90.0f * RotationSteps, 0.0f).RotateVector(FVector::ForwardVector);
	const FIntPoint InputDir = CardinalFromVector(Forward);    // 입력 = +Front
	const FIntPoint OutputDir = CardinalFromVector(-Forward);  // 출력 = -Front

	const TArray<FIntPoint> Footprint = CalculateFootprint(Machine, Origin, RotationSteps);

	const bool bDrawInput = Machine->GetInputPortCount() > 0 && !OJJ_IsExtractionMachine(Machine);
	const bool bDrawOutput = Machine->GetOutputPortCount() > 0;

	OJJ_EmitPortArrows(
		HoverInputArrowISM, bDrawInput,
		OJJ_PortCellsFromFootprint(Footprint, InputDir, Machine->GetInputPortCount()), InputDir,
		HoverOutputArrowISM, bDrawOutput,
		OJJ_PortCellsFromFootprint(Footprint, OutputDir, Machine->GetOutputPortCount()), OutputDir);
}

void AOJJ_Grid::ClearPlacedMachineArrows()
{
	if (PlacedInputArrowISM)
	{
		PlacedInputArrowISM->ClearInstances();
	}
	if (PlacedOutputArrowISM)
	{
		PlacedOutputArrowISM->ClearInstances();
	}

	bPlacedArrowsVisible = false;
}

void AOJJ_Grid::ClearHoverMachineArrows()
{
	if (HoverInputArrowISM)
	{
		HoverInputArrowISM->ClearInstances();
	}
	if (HoverOutputArrowISM)
	{
		HoverOutputArrowISM->ClearInstances();
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

	const TArray<FIntPoint>* Cells = OJJ_ActorToCells.Find(Machine);
	if (!Cells)
	{
		return false;
	}

	// 제거 직전 훅 — 자원 선점 해제 등. (자원 상태만 건드리고 그리드 맵은 안 건드리므로 Cells 포인터 유효 유지.)
	Machine->OnRemovedFromGrid();

	for (const FIntPoint& Cell : *Cells)
	{
		OccupiedCells.Remove(Cell);
	}
	OJJ_ActorToCells.Remove(Machine);
	OJJ_ActorToOrigin.Remove(Machine);
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFactoryManagerSubsystem* FactoryManager = GameInstance->GetSubsystem<UFactoryManagerSubsystem>())
		{
			FactoryManager->UnregisterMachine(Machine);
		}
	}

	// 빌드모드 중 제거였다면 placed 화살표를 즉시 재적재 — 제거된 머신의 stale 화살표가 다음 호버
	// 리빌드(커서/회전/모드 전환)까지 남는 것을 방지(Codex 리뷰 Medium). 위에서 OJJ_ActorToCells가
	// 이미 갱신됐으므로 재적재 시 제거 머신은 빠진다. 빌드모드 밖이면 플래그 false → 그림 안 그림.
	if (bPlacedArrowsVisible)
	{
		RefreshPlacedMachineArrows();
	}

	return true;
}

bool AOJJ_Grid::RemoveMachineAt(FIntPoint Coord)
{
	if (!HasAuthority())
	{
		ensureMsgf(false, TEXT("RemoveMachineAt called on non-authority"));
		return false;
	}

	const TWeakObjectPtr<AActor>* Found = OccupiedCells.Find(Coord);
	if (!Found || !Found->IsValid())
	{
		return false;
	}

	// 좌표 점유 액터를 머신으로 좁혀 제거. 비머신(컨베이어)이면 Cast 실패 → RemoveMachine(nullptr)이
	// false 반환 (컨베이어 제거는 Step 3에서 OJJ_RemoveActorAt로 별도 처리).
	return RemoveMachine(Cast<AMachineBase>(Found->Get()));
}
