#include "Pipe.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "MachineBase.h"
#include "Materials/MaterialInterface.h"
#include "Resource/ResourceData.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// 엔진 기본 메시(실린더/구)의 실제 치수를 런타임 측정 — 하드코딩 제거(PowerLine::UpdateLineSegment 패턴).
	// 반환값 = 풀스케일 박스 크기(2×BoxExtent), 각 축 0 나눗셈 방지 클램프 적용.
	FVector OJJ_MeshBoxSize(const UInstancedStaticMeshComponent* ISM)
	{
		const UStaticMesh* Mesh = ISM ? ISM->GetStaticMesh() : nullptr;
		const FVector Size = Mesh ? Mesh->GetBounds().BoxExtent * 2.0f : FVector(100.0f);
		return FVector(
			FMath::Max(Size.X, KINDA_SMALL_NUMBER),
			FMath::Max(Size.Y, KINDA_SMALL_NUMBER),
			FMath::Max(Size.Z, KINDA_SMALL_NUMBER));
	}
}

APipe::APipe()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	SegmentInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SegmentInstances"));
	SegmentInstances->SetupAttachment(Root);
	SegmentInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	JoinInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("JoinInstances"));
	JoinInstances->SetupAttachment(Root);
	JoinInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	LiquidVisualInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LiquidVisualInstances"));
	LiquidVisualInstances->SetupAttachment(Root);
	LiquidVisualInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LiquidVisualInstances->SetCastShadow(false);

	DebugStateText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("DebugStateText"));
	DebugStateText->SetupAttachment(Root);
	DebugStateText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DebugStateText->SetHorizontalAlignment(EHTA_Center);
	DebugStateText->SetVerticalAlignment(EVRTA_TextCenter);
	DebugStateText->SetWorldSize(DebugTextWorldSize);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		SegmentInstances->SetStaticMesh(CylinderMesh.Object);
		LiquidVisualInstances->SetStaticMesh(CylinderMesh.Object);
	}

	// 코너 조인트는 구 — 어떤 방향 전환각이든 반경 PipeRadius로 이음새를 덮음.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		JoinInstances->SetStaticMesh(SphereMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialAsset(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MaterialAsset.Succeeded())
	{
		SegmentInstances->SetMaterial(0, MaterialAsset.Object);
		JoinInstances->SetMaterial(0, MaterialAsset.Object);
		LiquidVisualInstances->SetMaterial(0, MaterialAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UDataTable> ResourceTableFinder(
		TEXT("/Game/DataTable/DT_ResourceData.DT_ResourceData"));
	if (ResourceTableFinder.Succeeded())
	{
		ResourceTable = ResourceTableFinder.Object;
	}
}

void APipe::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	RefreshLiquidVisualInstances();
	if (bShowDebugStateText)
	{
		UpdateDebugTextFacingPlayer();
	}
}

void APipe::BeginPlay()
{
	Super::BeginPlay();
	RestartLiquidMoveTimer();
	RefreshLiquidVisualInstances();
	UpdateDebugStateText();
}

void APipe::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildVisuals();
	RefreshLiquidVisualInstances();
	UpdateDebugStateText();
}

void APipe::SetPath(const TArray<FIntPoint>& NewPathCells, float NewCellSize)
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
	RefreshLiquidVisualInstances();
	UpdateDebugStateText();
}

void APipe::OJJ_SetPathCellLocalZs(const TArray<float>& InCellLifts)
{
	// 1:1(PathCells와 동수)일 때만 적용 — 그 외(빈 배열/불일치)는 평면 fallback. 빈 배열 = 기존 동작 항등.
	PathCellZs = (InCellLifts.Num() == PathCells.Num()) ? InCellLifts : TArray<float>();
	RebuildVisuals();
	RefreshLiquidVisualInstances();
}

void APipe::ConfigureTransport(
	const TArray<FIntPoint>& NewOccupiedGridCells,
	AMachineBase* NewSourceMachine,
	AMachineBase* NewTargetMachine)
{
	OccupiedGridCells.Reset(NewOccupiedGridCells.Num());
	for (const FIntPoint& Cell : NewOccupiedGridCells)
	{
		OccupiedGridCells.AddUnique(Cell);
	}

	SourceMachine = NewSourceMachine;
	TargetMachine = NewTargetMachine;
	ResetLiquidSlots();
	RestartLiquidMoveTimer();
	RefreshLiquidVisualInstances();
	UpdateDebugStateText();
}

void APipe::ClearPath()
{
	PathCells.Reset();
	OccupiedGridCells.Reset();
	LiquidSlots.Reset();
	SourceMachine.Reset();
	TargetMachine.Reset();
	StopLiquidMoveTimer();
	RebuildVisuals();
	RefreshLiquidVisualInstances();
	UpdateDebugStateText();
}

bool APipe::IsOutputBlocked() const
{
	if (LiquidSlots.Num() == 0 || !TargetMachine.IsValid())
	{
		return false;
	}

	const FPipeLiquidSlot& LastSlot = LiquidSlots.Last();
	return !LastSlot.IsEmpty() && !TargetMachine->CanReceiveConveyorItem(LastSlot.LiquidID, LastSlot.Amount);
}

void APipe::UpdateDebugStateText()
{
	if (!DebugStateText)
	{
		return;
	}

	DebugStateText->SetVisibility(bShowDebugStateText);
	DebugStateText->SetWorldSize(DebugTextWorldSize);
	DebugStateText->SetRelativeLocation(DebugTextOffset);
	if (!bShowDebugStateText)
	{
		return;
	}

	int32 TotalAmount = 0;
	TMap<FName, int32> Liquids;
	for (const FPipeLiquidSlot& Slot : LiquidSlots)
	{
		if (Slot.IsEmpty())
		{
			continue;
		}

		TotalAmount += Slot.Amount;
		Liquids.FindOrAdd(Slot.LiquidID) += Slot.Amount;
	}

	FString LiquidSummary = TEXT("None");
	if (Liquids.Num() > 0)
	{
		LiquidSummary.Reset();
		for (const TPair<FName, int32>& Pair : Liquids)
		{
			if (!LiquidSummary.IsEmpty())
			{
				LiquidSummary += TEXT("\n");
			}
			LiquidSummary += FString::Printf(TEXT("%s x%d"), *Pair.Key.ToString(), Pair.Value);
		}
	}

	const TCHAR* StatusText = IsOutputBlocked()
		? TEXT("blocked")
		: (TotalAmount > 0 ? TEXT("moving") : TEXT("idle"));

	const FString DebugText = FString::Printf(
		TEXT("Pipe\nSegments: %d\nStatus: %s\nLiquids\n%s"),
		OccupiedGridCells.Num(),
		StatusText,
		*LiquidSummary);
	DebugStateText->SetText(FText::FromString(DebugText));
}

void APipe::RebuildVisuals()
{
	if (!SegmentInstances)
	{
		return;
	}

	SegmentInstances->ClearInstances();
	if (JoinInstances)
	{
		JoinInstances->ClearInstances();
	}
	if (PathCells.Num() == 0)
	{
		return;
	}

	const float Diameter = FMath::Max(1.0f, PipeRadius * 2.0f);
	const FVector Centroid = GetPathCentroidLocal();

	// PathCells + 셀별 lift(PathCellZs) → 3D 노드 폴리라인. lift가 0↔H로 바뀌는 경계 셀엔 같은 XY에
	// base/top 2노드를 삽입해 수직 라이저(ㄷ자 다리)를 만든다. lift 균일(전부 0/빈 배열)이면 셀당 1노드(평면 항등).
	auto CellLift = [this](int32 NodeIndex) -> float
	{
		return PathCellZs.IsValidIndex(NodeIndex) ? PathCellZs[NodeIndex] : 0.0f;
	};
	const int32 NumCells = PathCells.Num();
	TArray<FVector> Nodes;
	Nodes.Reserve(NumCells + 4);
	for (int32 Index = 0; Index < NumCells; ++Index)
	{
		const FIntPoint Cell = PathCells[Index];
		const float LocalX = (Cell.X * CellSize) + (CellSize * 0.5f) - Centroid.X;
		const float LocalY = (Cell.Y * CellSize) + (CellSize * 0.5f) - Centroid.Y;
		const float Lift = CellLift(Index);
		const float PrevLift = (Index > 0) ? CellLift(Index - 1) : Lift;
		const float NextLift = (Index + 1 < NumCells) ? CellLift(Index + 1) : Lift;

		if (Lift > PrevLift) // 상승 진입 — 이전 높이의 base 먼저(수직 라이저 up)
		{
			Nodes.Add(FVector(LocalX, LocalY, ZOffset + PrevLift));
		}
		Nodes.Add(FVector(LocalX, LocalY, ZOffset + Lift)); // 셀 주 노드(자기 높이)
		if (Lift > NextLift) // 하강 이탈 — 다음 높이의 base 추가(수직 라이저 down)
		{
			Nodes.Add(FVector(LocalX, LocalY, ZOffset + NextLift));
		}
	}

	// 끝 노드를 머신 포트 면까지 반 칸 연장 — 컨베이어가 셀당 풀셀 메시로 끝 셀을 꽉 채우는 것과 동일
	// 커버리지. 파이프는 노드 중심 간 실린더라 끝 반 칸(중심→셀 경계)이 비어 머신에서 떠 보였음.
	// 양 끝(펌프 출력 / 탱크 입력) 모두 끝 세그먼트 진행축으로 0.5칸 밀어 포트 면에 닿게 한다.
	if (Nodes.Num() >= 2)
	{
		const float HalfCell = CellSize * 0.5f;
		const FVector StartDir = (Nodes[1] - Nodes[0]).GetSafeNormal();
		const FVector EndDir = (Nodes.Last() - Nodes[Nodes.Num() - 2]).GetSafeNormal();
		Nodes[0] -= StartDir * HalfCell;          // 시작 노드 → 머신(경로 반대) 쪽으로
		Nodes.Last() += EndDir * HalfCell;         // 끝 노드 → 머신(진행) 쪽으로
	}

	// 조인트 구 스케일 — 메시 실측 지름으로 환산해 균일 반경(=PipeRadius) 보장.
	const FVector SphereMeshSize = JoinInstances ? OJJ_MeshBoxSize(JoinInstances) : FVector(100.0f);
	const FVector SphereScale(
		Diameter / SphereMeshSize.X,
		Diameter / SphereMeshSize.Y,
		Diameter / SphereMeshSize.Z);

	// 단일 노드(퇴화 경로): 방향이 없으므로 조인트 구 하나로 마감.
	if (Nodes.Num() == 1)
	{
		if (JoinInstances)
		{
			JoinInstances->AddInstance(FTransform(FRotator::ZeroRotator, Nodes[0], SphereScale));
		}
		return;
	}

	// ① 세그먼트 — 노드쌍마다 실린더 1개. 수직 라이저(같은 XY, Z만 차이)도 FindBetweenNormals(Up,+Z)로
	// 자연 수직 실린더가 됨. 길이/지름은 메시 실측 스케일.
	const FVector CylinderMeshSize = OJJ_MeshBoxSize(SegmentInstances);
	for (int32 Index = 0; Index + 1 < Nodes.Num(); ++Index)
	{
		const FVector SegmentVector = Nodes[Index + 1] - Nodes[Index];
		const float SegmentLength = SegmentVector.Length();
		if (SegmentLength <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FQuat SegmentRotation = FQuat::FindBetweenNormals(FVector::UpVector, SegmentVector / SegmentLength);
		const FVector SegmentMidpoint = (Nodes[Index] + Nodes[Index + 1]) * 0.5f;
		const FVector SegmentScale(
			Diameter / CylinderMeshSize.X,
			Diameter / CylinderMeshSize.Y,
			SegmentLength / CylinderMeshSize.Z);
		SegmentInstances->AddInstance(FTransform(SegmentRotation, SegmentMidpoint, SegmentScale));
	}

	// ② 코너 이음새 — 3D 방향 전환 노드에 조인트 구(ㄱ자 평면 코너 + ㄷ자 수직↔수평 4모서리 전부).
	// 동축(방향 동일) 노드는 갭이 없으므로 생략(오버드로 최소화).
	if (JoinInstances)
	{
		for (int32 Index = 1; Index + 1 < Nodes.Num(); ++Index)
		{
			const FVector PrevDir = (Nodes[Index] - Nodes[Index - 1]).GetSafeNormal();
			const FVector NextDir = (Nodes[Index + 1] - Nodes[Index]).GetSafeNormal();
			if (PrevDir.Equals(NextDir, KINDA_SMALL_NUMBER))
			{
				continue;
			}
			JoinInstances->AddInstance(FTransform(FRotator::ZeroRotator, Nodes[Index], SphereScale));
		}
	}
}

void APipe::ResetLiquidSlots()
{
	LiquidSlots.SetNum(OccupiedGridCells.Num());
	for (FPipeLiquidSlot& Slot : LiquidSlots)
	{
		Slot.Reset();
	}
}

void APipe::RestartLiquidMoveTimer()
{
	StopLiquidMoveTimer();

	if (!bAutoMoveLiquids || LiquidSlots.Num() == 0 || !SourceMachine.IsValid() || !TargetMachine.IsValid())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			LiquidMoveTimerHandle,
			this,
			&APipe::MoveLiquidsOneSegment,
			FMath::Max(0.01f, SecondsPerSegment),
			true);
	}
}

void APipe::StopLiquidMoveTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LiquidMoveTimerHandle);
	}
}

void APipe::MoveLiquidsOneSegment()
{
	if (LiquidSlots.Num() == 0 || !SourceMachine.IsValid() || !TargetMachine.IsValid())
	{
		UpdateDebugStateText();
		return;
	}

	const int32 LastIndex = LiquidSlots.Num() - 1;
	FPipeLiquidSlot& LastSlot = LiquidSlots[LastIndex];
	if (!LastSlot.IsEmpty() && TargetMachine->CanReceiveConveyorItem(LastSlot.LiquidID, LastSlot.Amount))
	{
		if (TargetMachine->ReceiveConveyorItem(LastSlot.LiquidID, LastSlot.Amount))
		{
			LastSlot.Reset();
		}
	}

	for (int32 Index = LastIndex; Index > 0; --Index)
	{
		if (LiquidSlots[Index].IsEmpty() && !LiquidSlots[Index - 1].IsEmpty())
		{
			LiquidSlots[Index] = LiquidSlots[Index - 1];
			LiquidSlots[Index - 1].Reset();
		}
	}

	if (LiquidSlots[0].IsEmpty())
	{
		FPipeLiquidSlot NewSlot;
		if (TryPullLiquidFromSource(NewSlot))
		{
			LiquidSlots[0] = NewSlot;
		}
	}

	RefreshLiquidVisualInstances();
	UpdateDebugStateText();
}

void APipe::RefreshLiquidVisualInstances()
{
	if (!LiquidVisualInstances)
	{
		return;
	}

	LiquidVisualInstances->ClearInstances();
	if (LiquidSlots.Num() == 0)
	{
		return;
	}

	const float VisualScale = FMath::Max(0.01f, LiquidVisualScaleRatio);
	const FVector InstanceScale(VisualScale, VisualScale, VisualScale);

	for (int32 Index = 0; Index < LiquidSlots.Num(); ++Index)
	{
		if (LiquidSlots[Index].IsEmpty() || !OccupiedGridCells.IsValidIndex(Index))
		{
			continue;
		}

		const FVector LocalLocation = GetCellLocalCenter(OccupiedGridCells[Index]) + FVector(0.0f, 0.0f, LiquidVisualZOffset);
		LiquidVisualInstances->AddInstance(FTransform(FRotator(90.0f, 0.0f, 0.0f), LocalLocation, InstanceScale));
	}
}

void APipe::UpdateDebugTextFacingPlayer()
{
	if (!DebugStateText || !bShowDebugStateText)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!PlayerController || !PlayerController->PlayerCameraManager)
	{
		return;
	}

	FVector ToCamera = PlayerController->PlayerCameraManager->GetCameraLocation() - DebugStateText->GetComponentLocation();
	ToCamera.Z = 0.0f;
	if (ToCamera.IsNearlyZero())
	{
		return;
	}

	DebugStateText->SetWorldRotation(FRotator(0.0f, ToCamera.Rotation().Yaw, 0.0f));
}

bool APipe::TryPullLiquidFromSource(FPipeLiquidSlot& OutSlot)
{
	OutSlot.Reset();

	if (!SourceMachine.IsValid())
	{
		return false;
	}

	FName LiquidID = NAME_None;
	if (!SourceMachine->PeekFirstOutputItem(LiquidID) || !IsLiquidItem(LiquidID))
	{
		return false;
	}

	if (!SourceMachine->TryTakeFirstOutputItem(LiquidID))
	{
		return false;
	}

	OutSlot.LiquidID = LiquidID;
	OutSlot.Amount = 1;

	while (OutSlot.Amount < MaxUnitsPerSegment)
	{
		FName NextLiquidID = NAME_None;
		if (!SourceMachine->PeekFirstOutputItem(NextLiquidID) || NextLiquidID != LiquidID)
		{
			break;
		}

		FName TakenLiquidID = NAME_None;
		if (!SourceMachine->TryTakeFirstOutputItem(TakenLiquidID) || TakenLiquidID != LiquidID)
		{
			break;
		}

		++OutSlot.Amount;
	}

	return true;
}

bool APipe::IsLiquidItem(FName ItemID) const
{
	if (!ResourceTable || ItemID.IsNone())
	{
		return false;
	}

	const FResourceData* Resource = ResourceTable->FindRow<FResourceData>(ItemID, TEXT("Pipe.IsLiquidItem"));
	return Resource && Resource->form == FName(TEXT("liquid"));
}

FVector APipe::GetPathCentroidLocal() const
{
	if (PathCells.Num() == 0)
	{
		return FVector::ZeroVector;
	}

	FVector Center = FVector::ZeroVector;
	for (const FIntPoint& Cell : PathCells)
	{
		Center.X += (Cell.X * CellSize) + (CellSize * 0.5f);
		Center.Y += (Cell.Y * CellSize) + (CellSize * 0.5f);
	}

	Center /= static_cast<float>(PathCells.Num());
	return FVector(Center.X, Center.Y, 0.0f);
}

FVector APipe::GetCellLocalCenter(FIntPoint Cell) const
{
	const FVector Centroid = GetPathCentroidLocal();
	// F4-3: 액체 비주얼도 노드 lift를 따라 오르게 — 셀의 경로 인덱스로 PathCellZs 조회(없으면 0=평면).
	const int32 NodeIndex = PathCells.IndexOfByKey(Cell);
	const float Lift = PathCellZs.IsValidIndex(NodeIndex) ? PathCellZs[NodeIndex] : 0.0f;
	return FVector(
		(Cell.X * CellSize) + (CellSize * 0.5f) - Centroid.X,
		(Cell.Y * CellSize) + (CellSize * 0.5f) - Centroid.Y,
		ZOffset + Lift);
}
