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

	// 노드 로컬 중심 — XY는 셀 중앙, Z는 PathCellZs(있으면 노드별, 없으면 ZOffset 균일).
	// PathCellZs가 채워지면 양끝 Z 차이가 그대로 경사 실린더로 반영됨(F4-3 오버패스 대비, 렌더 무변경).
	auto NodeLocalCenter = [this, &Centroid](int32 NodeIndex) -> FVector
	{
		const FIntPoint Cell = PathCells[NodeIndex];
		const float NodeZ = PathCellZs.IsValidIndex(NodeIndex) ? PathCellZs[NodeIndex] : ZOffset;
		return FVector(
			(Cell.X * CellSize) + (CellSize * 0.5f) - Centroid.X,
			(Cell.Y * CellSize) + (CellSize * 0.5f) - Centroid.Y,
			NodeZ);
	};

	// 조인트 구 스케일 — 메시 실측 지름으로 환산해 균일 반경(=PipeRadius) 보장.
	const FVector SphereMeshSize = JoinInstances ? OJJ_MeshBoxSize(JoinInstances) : FVector(100.0f);
	const FVector SphereScale(
		Diameter / SphereMeshSize.X,
		Diameter / SphereMeshSize.Y,
		Diameter / SphereMeshSize.Z);

	// 단일 셀(퇴화 경로): 방향이 없으므로 조인트 구 하나로 마감.
	if (PathCells.Num() == 1)
	{
		if (JoinInstances)
		{
			JoinInstances->AddInstance(FTransform(FRotator::ZeroRotator, NodeLocalCenter(0), SphereScale));
		}
		return;
	}

	// ① 직선 세그먼트 — 노드쌍마다 실린더 1개(중점 배치 + 방향 정렬 + 길이/지름 실측 스케일).
	const FVector CylinderMeshSize = OJJ_MeshBoxSize(SegmentInstances);
	for (int32 Index = 0; Index + 1 < PathCells.Num(); ++Index)
	{
		const FVector StartLocation = NodeLocalCenter(Index);
		const FVector EndLocation = NodeLocalCenter(Index + 1);
		const FVector SegmentVector = EndLocation - StartLocation;
		const float SegmentLength = SegmentVector.Length();
		if (SegmentLength <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		// FindBetweenNormals는 3D — 양끝 Z가 다르면 기울어진 실린더(경사 파이프)가 자동 생성됨.
		const FQuat SegmentRotation = FQuat::FindBetweenNormals(FVector::UpVector, SegmentVector / SegmentLength);
		const FVector SegmentMidpoint = (StartLocation + EndLocation) * 0.5f;
		const FVector SegmentScale(
			Diameter / CylinderMeshSize.X,
			Diameter / CylinderMeshSize.Y,
			SegmentLength / CylinderMeshSize.Z);
		SegmentInstances->AddInstance(FTransform(SegmentRotation, SegmentMidpoint, SegmentScale));
	}

	// ② ㄱ자 코너 이음새 — 방향 전환 노드에만 조인트 구를 박아 두 실린더 만나는 모서리 갭을 가림.
	// 직진 통과 노드(Prev==Next)는 동축 실린더라 갭이 없으므로 생략(오버드로 최소화).
	if (JoinInstances)
	{
		for (int32 Index = 1; Index + 1 < PathCells.Num(); ++Index)
		{
			const FIntPoint PrevDir = PathCells[Index] - PathCells[Index - 1];
			const FIntPoint NextDir = PathCells[Index + 1] - PathCells[Index];
			if (PrevDir == NextDir)
			{
				continue;
			}
			JoinInstances->AddInstance(FTransform(FRotator::ZeroRotator, NodeLocalCenter(Index), SphereScale));
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
	return FVector(
		(Cell.X * CellSize) + (CellSize * 0.5f) - Centroid.X,
		(Cell.Y * CellSize) + (CellSize * 0.5f) - Centroid.Y,
		ZOffset);
}
