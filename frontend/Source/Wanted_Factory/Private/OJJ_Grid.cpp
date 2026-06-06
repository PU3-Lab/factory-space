// Fill out your copyright notice in the Description page of Project Settings.


#include "OJJ_Grid.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Conveyor.h"
#include "Engine/StaticMesh.h"
#include "FactoryManagerSubsystem.h"
#include "MachineBase.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
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
		ValidHoverISM->SetMaterial(0, ValidHoverMat.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> InvalidHoverMat(
		TEXT("/Game/OJJ/Materials/MI_OJJ_GridHoverInvalid.MI_OJJ_GridHoverInvalid"));
	if (InvalidHoverMat.Succeeded())
	{
		InvalidHoverISM->SetMaterial(0, InvalidHoverMat.Object);
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

		// Plane은 액터 중심에 위치 → 그리드 lower-left 원점에 맞추려면 절반만큼 +XY 오프셋
		// Z=1로 Z-fighting 방지
		const float OffsetXY = (VisualizationRange * CellSize) / 2.0f;
		GridFloorMesh->SetRelativeLocation(FVector(OffsetXY, OffsetXY, 1.0f));
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
			InputArrowMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.1f, 0.4f, 1.0f));
			if (PlacedInputArrowISM) PlacedInputArrowISM->SetMaterial(0, InputArrowMID);
			if (HoverInputArrowISM) HoverInputArrowISM->SetMaterial(0, InputArrowMID);
		}
		if (OutputArrowMID)
		{
			OutputArrowMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0f, 0.45f, 0.05f));
			if (PlacedOutputArrowISM) PlacedOutputArrowISM->SetMaterial(0, OutputArrowMID);
			if (HoverOutputArrowISM) HoverOutputArrowISM->SetMaterial(0, OutputArrowMID);
		}
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

	return FVector(LowerLeftCenter.X + OffsetX, LowerLeftCenter.Y + OffsetY, LowerLeftCenter.Z + ZOffset);
}

bool AOJJ_Grid::IsValidGridCell(FIntPoint Cell) const
{
	return Cell.X >= 0 && Cell.X < GridSize.X
		&& Cell.Y >= 0 && Cell.Y < GridSize.Y;
}

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
	Conveyor->SetActorLocation(GetActorLocation() + Conveyor->GetActorRotation().RotateVector(Conveyor->GetPathCentroidLocal()));
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
		const FVector InstanceLocation(CellCenter.X, CellCenter.Y, CellCenter.Z + 2.0f);
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
	for (const FIntPoint& Cell : CalculateFootprint(Machine, Origin, RotationSteps))
	{
		if (!IsValidGridCell(Cell))
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

	// 호버 셀 ISM과 동반 생멸 — 커서가 유효 셀을 떠나면(트레이스 실패/off-grid/퇴장) 호버 화살표도 사라짐.
	ClearHoverMachineArrows();
}

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

	// 배치 호버(UpdateHoverPreview)와 동일한 셀→인스턴스 규칙(Z+2 가림 방지, Plane 100→CellSize 스케일, world-space).
	for (const FIntPoint& Cell : Cells)
	{
		const FVector CellCenter = GridToWorld(Cell);
		const FVector InstanceLocation(CellCenter.X, CellCenter.Y, CellCenter.Z + 2.0f);
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
		const FVector Location(CellCenter.X, CellCenter.Y, CellCenter.Z + PortArrowHeightOffset);

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
