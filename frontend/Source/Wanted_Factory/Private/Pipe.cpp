#include "Pipe.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "MachineBase.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "PlayerWarehouseSubsystem.h"
#include "Resource/ResourceData.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
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

	auto SetupTubeCollision = [](UInstancedStaticMeshComponent* ISM)
	{
		ISM->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		ISM->SetCollisionObjectType(ECC_WorldStatic);
		ISM->SetCollisionResponseToAllChannels(ECR_Ignore);
		ISM->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		ISM->SetGenerateOverlapEvents(false);
	};

	SegmentInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SegmentInstances"));
	SegmentInstances->SetupAttachment(Root);
	SetupTubeCollision(SegmentInstances);
	SegmentInstances->SetVisibleInRayTracing(false);

	JoinInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("JoinInstances"));
	JoinInstances->SetupAttachment(Root);
	SetupTubeCollision(JoinInstances);
	JoinInstances->SetVisibleInRayTracing(false);

	LiquidVisualInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LiquidVisualInstances"));
	LiquidVisualInstances->SetupAttachment(Root);
	LiquidVisualInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LiquidVisualInstances->SetCastShadow(false);
	LiquidVisualInstances->SetVisibleInRayTracing(false);

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

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		JoinInstances->SetStaticMesh(SphereMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialAsset(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MaterialAsset.Succeeded())
	{
		PipeMaterialBase = MaterialAsset.Object;
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
	UpdateMaterialState();
	RefreshLiquidVisualInstances();
	UpdateDebugStateText();
}

void APipe::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildVisuals();
	UpdateMaterialState();
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
	PathCellEdgeRiser.Reset(); // 경로 바뀌면 stale 플래그 폐기 — 그리드가 OJJ_SetPathCellEdgeRisers로 재주입.

	RebuildVisuals();
	UpdateMaterialState();
	RefreshLiquidVisualInstances();
	UpdateDebugStateText();
}

void APipe::OJJ_SetPathCellLocalZs(const TArray<float>& InCellLifts)
{
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
	UpdateMaterialState();
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
	UpdateMaterialState();
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
		// 솔리드 데크 셀이면 수직 라이저를 셀 중심 대신 셀 엣지 밖(이웃 방향 HalfCell + 마진)에 둬 관통/모서리 스침 방지.
		// 마진은 파이프 반지름만큼 안쪽 몸체가 데크 모서리를 스치는 것까지 막는 여유(OJJ_PipeEdgeDropMargin).
		const bool bEdgeRiser = PathCellEdgeRiser.IsValidIndex(Index) && PathCellEdgeRiser[Index];
		const float EdgeOffset = (CellSize * 0.5f) + OJJ_PipeEdgeDropMargin;

		// 진입 라이저(낮은 prev → 높은 현재). 솔리드면 진입 엣지 밖(prev 방향)서 수직 상승 후 데크 위로 수평.
		if (Lift > PrevLift)
		{
			if (bEdgeRiser && Index > 0)
			{
				const FIntPoint StepToPrev = PathCells[Index - 1] - Cell;
				const float EdgeX = LocalX + StepToPrev.X * EdgeOffset;
				const float EdgeY = LocalY + StepToPrev.Y * EdgeOffset;
				Nodes.Add(FVector(EdgeX, EdgeY, ZOffset + PrevLift)); // 엣지 밖, 낮은 Z(라이저 base)
				Nodes.Add(FVector(EdgeX, EdgeY, ZOffset + Lift));     // 엣지 밖서 수직 상승
			}
			else
			{
				Nodes.Add(FVector(LocalX, LocalY, ZOffset + PrevLift)); // 기존: 셀 중심(오버패스/일반)
			}
		}
		Nodes.Add(FVector(LocalX, LocalY, ZOffset + Lift));
		// 이탈 라이저(높은 현재 → 낮은 next). 솔리드면 이탈 엣지 밖(next 방향)서 수직 하강 후 지면으로 수평.
		if (Lift > NextLift)
		{
			if (bEdgeRiser && Index + 1 < NumCells)
			{
				const FIntPoint StepToNext = PathCells[Index + 1] - Cell;
				const float EdgeX = LocalX + StepToNext.X * EdgeOffset;
				const float EdgeY = LocalY + StepToNext.Y * EdgeOffset;
				Nodes.Add(FVector(EdgeX, EdgeY, ZOffset + Lift));     // 엣지 밖까지 데크높이 수평
				Nodes.Add(FVector(EdgeX, EdgeY, ZOffset + NextLift)); // 엣지 밖서 수직 하강
			}
			else
			{
				Nodes.Add(FVector(LocalX, LocalY, ZOffset + NextLift)); // 기존: 셀 중심(오버패스/일반)
			}
		}
	}

	if (Nodes.Num() >= 2)
	{
		const float HalfCell = CellSize * 0.5f;
		const FVector StartDir = (Nodes[1] - Nodes[0]).GetSafeNormal();
		const FVector EndDir = (Nodes.Last() - Nodes[Nodes.Num() - 2]).GetSafeNormal();
		// 펌프 시작 스텁 — 기존 직선 돌출 그대로(무변경).
		Nodes[0] -= StartDir * HalfCell;
		// #257 탱크 진입: 포트 방향이 주입됐으면 마지막 셀 중심을 bend 조인트로 남기고(이동 X) 포트 방향으로 꺾인
		// 스텁 노드를 추가 — 마지막 실린더가 포트 쪽으로 꺾이고 JoinInstances 구가 이음매를 덮어 "물려 들어가는"
		// 연결이 된다. 스텁은 수평(Z=마지막 셀)이라 경사 탱크에서도 구 조인트가 받쳐 글리치 없음(#252 무관).
		// 포트 방향 없으면(Zero) 기존 직선 돌출 유지(항등).
		const FVector EndPortDirLocal =
			FVector(static_cast<float>(OJJ_EndPortFlowDir.X), static_cast<float>(OJJ_EndPortFlowDir.Y), 0.0f).GetSafeNormal();
		if (!EndPortDirLocal.IsNearlyZero())
		{
			Nodes.Add(Nodes.Last() + EndPortDirLocal * HalfCell);
		}
		else
		{
			Nodes.Last() += EndDir * HalfCell;
		}
	}

	const FVector SphereMeshSize = JoinInstances ? OJJ_MeshBoxSize(JoinInstances) : FVector(100.0f);
	const FVector SphereScale(
		Diameter / SphereMeshSize.X,
		Diameter / SphereMeshSize.Y,
		Diameter / SphereMeshSize.Z);

	if (Nodes.Num() == 1)
	{
		if (JoinInstances)
		{
			JoinInstances->AddInstance(FTransform(FRotator::ZeroRotator, Nodes[0], SphereScale));
		}
		return;
	}

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
	UpdateMaterialState();
}

void APipe::ApplyLiquidSlotsForSave(const TArray<FPipeLiquidSlot>& SavedLiquidSlots)
{
	LiquidSlots = SavedLiquidSlots;
	UpdateMaterialState();
	RefreshLiquidVisualInstances();
	UpdateDebugStateText();
}

bool APipe::RefundLiquidsToWarehouse()
{
	const UGameInstance* GameInstance = GetGameInstance();
	UPlayerWarehouseSubsystem* Warehouse = GameInstance
		? GameInstance->GetSubsystem<UPlayerWarehouseSubsystem>()
		: nullptr;
	if (!Warehouse)
	{
		return false;
	}

	TMap<FName, int32> RefundedLiquids;
	for (const FPipeLiquidSlot& Slot : LiquidSlots)
	{
		if (!Slot.IsEmpty())
		{
			RefundedLiquids.FindOrAdd(Slot.LiquidID) += Slot.Amount;
		}
	}

	if (RefundedLiquids.Num() == 0)
	{
		return true;
	}

	for (const TPair<FName, int32>& Liquid : RefundedLiquids)
	{
		Warehouse->AddItem(Liquid.Key, Liquid.Value);
	}

	for (FPipeLiquidSlot& Slot : LiquidSlots)
	{
		Slot.Reset();
	}

	UpdateMaterialState();
	RefreshLiquidVisualInstances();
	UpdateDebugStateText();
	return true;
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

	UpdateMaterialState();
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

void APipe::UpdateMaterialState()
{
	const bool bHasLiquid = HasAnyLiquid();
	UMaterialInterface* PipeMaterial = GetPipeMaterial(bHasLiquid);
	if (SegmentInstances)
	{
		SegmentInstances->SetMaterial(0, PipeMaterial);
	}
	if (JoinInstances)
	{
		JoinInstances->SetVisibility(true);
		JoinInstances->SetMaterial(0, PipeMaterial);
	}
	if (LiquidVisualInstances)
	{
		LiquidVisualInstances->SetMaterial(0, GetPipeMaterial(true));
	}
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

bool APipe::HasAnyLiquid() const
{
	for (const FPipeLiquidSlot& Slot : LiquidSlots)
	{
		if (!Slot.IsEmpty())
		{
			return true;
		}
	}

	return false;
}

FName APipe::GetPrimaryLiquidID() const
{
	for (const FPipeLiquidSlot& Slot : LiquidSlots)
	{
		if (!Slot.IsEmpty())
		{
			return Slot.LiquidID;
		}
	}

	return NAME_None;
}

FLinearColor APipe::GetFilledPipeColor() const
{
	const FName LiquidID = GetPrimaryLiquidID();
	if (LiquidID.IsNone() || !ResourceTable)
	{
		return FilledPipeColor;
	}

	const FResourceData* Resource = ResourceTable->FindRow<FResourceData>(LiquidID, TEXT("Pipe.GetFilledPipeColor"));
	if (!Resource)
	{
		return FilledPipeColor;
	}

	return Resource->VisualColor;
}

UMaterialInterface* APipe::GetPipeMaterial(bool bHasLiquid)
{
	TObjectPtr<UMaterialInstanceDynamic>& MaterialInstance = bHasLiquid
		? FilledPipeMaterialInstance
		: EmptyPipeMaterialInstance;
	UMaterialInterface* BaseMaterial = nullptr;

	if (bHasLiquid && FilledPipeMaterial)
	{
		BaseMaterial = FilledPipeMaterial;
	}
	else if (!bHasLiquid && EmptyPipeMaterial)
	{
		BaseMaterial = EmptyPipeMaterial;
	}
	else
	{
		BaseMaterial = PipeMaterialBase
			? PipeMaterialBase.Get()
			: UMaterial::GetDefaultMaterial(MD_Surface);
	}

	if (!MaterialInstance || MaterialInstance->Parent != BaseMaterial)
	{
		MaterialInstance = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	}

	ConfigureMaterialInstance(MaterialInstance, bHasLiquid);
	return MaterialInstance.Get();
}

void APipe::ConfigureMaterialInstance(UMaterialInstanceDynamic* MaterialInstance, bool bHasLiquid) const
{
	if (!MaterialInstance)
	{
		return;
	}

	const FLinearColor PipeColor = bHasLiquid ? GetFilledPipeColor() : EmptyPipeColor;
	const float EmissiveStrength = bHasLiquid ? FilledPipeEmissiveStrength : 0.0f;
	const FLinearColor EmissiveColor = PipeColor * EmissiveStrength;

	MaterialInstance->SetVectorParameterValue(TEXT("Color"), PipeColor);
	MaterialInstance->SetVectorParameterValue(TEXT("BaseColor"), PipeColor);
	MaterialInstance->SetVectorParameterValue(TEXT("Tint"), PipeColor);
	MaterialInstance->SetVectorParameterValue(TEXT("EmissiveColor"), EmissiveColor);
	MaterialInstance->SetScalarParameterValue(TEXT("Opacity"), PipeColor.A);
	MaterialInstance->SetScalarParameterValue(TEXT("Alpha"), PipeColor.A);
	MaterialInstance->SetScalarParameterValue(TEXT("EmissiveStrength"), EmissiveStrength);
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
	const int32 NodeIndex = PathCells.IndexOfByKey(Cell);
	const float Lift = PathCellZs.IsValidIndex(NodeIndex) ? PathCellZs[NodeIndex] : 0.0f;
	return FVector(
		(Cell.X * CellSize) + (CellSize * 0.5f) - Centroid.X,
		(Cell.Y * CellSize) + (CellSize * 0.5f) - Centroid.Y,
		ZOffset + Lift);
}
