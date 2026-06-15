// Fill out your copyright notice in the Description page of Project Settings.

#include "Conveyor.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "FactoryManagerSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "MachineBase.h"
#include "Materials/MaterialInterface.h"
#include "Resource/ResourceData.h"
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

float CornerToYaw90(FIntPoint PreviousDirection, FIntPoint NextDirection)
{
	// 진입(-Prev)+진출(Next) 합 = Next-Prev → 4사분면으로 코너 4종을 구별, 90° 단위 격자 정렬.
	// (CornerBaseYaw 오프셋은 멤버라 호출부에서 더함.)
	const int32 SX = NextDirection.X - PreviousDirection.X;
	const int32 SY = NextDirection.Y - PreviousDirection.Y;

	if (SX > 0 && SY > 0)
	{
		return 0.0f;
	}
	if (SX < 0 && SY > 0)
	{
		return 90.0f;
	}
	if (SX < 0 && SY < 0)
	{
		return 180.0f;
	}
	return 270.0f;   // SX > 0 && SY < 0
}
}

AConveyor::AConveyor()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	StraightSegmentInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("StraightSegmentInstances"));
	StraightSegmentInstances->SetupAttachment(Root);
	StraightSegmentInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	CornerSegmentInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CornerSegmentInstances"));
	CornerSegmentInstances->SetupAttachment(Root);
	CornerSegmentInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ItemVisualInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ItemVisualInstances"));
	ItemVisualInstances->SetupAttachment(Root);
	ItemVisualInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ItemVisualInstances->SetCastShadow(false);

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
		ItemVisualInstances->SetStaticMesh(CubeMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialAsset(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MaterialAsset.Succeeded())
	{
		StraightSegmentInstances->SetMaterial(0, MaterialAsset.Object);
		CornerSegmentInstances->SetMaterial(0, MaterialAsset.Object);
		ItemVisualInstances->SetMaterial(0, MaterialAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UDataTable> ResourceTableFinder(
		TEXT("/Game/DataTable/DT_ResourceData.DT_ResourceData"));
	if (ResourceTableFinder.Succeeded())
	{
		ResourceTable = ResourceTableFinder.Object;
	}
}

void AConveyor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	RefreshItemVisualInstances();
	if (bShowDebugStateText)
	{
		UpdateDebugTextFacingPlayer();
	}
}

void AConveyor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	RebuildVisuals();
	RefreshItemVisualInstances();
	UpdateDebugStateText();
	UpdateDebugTextFacingPlayer();
}

void AConveyor::BeginPlay()
{
	Super::BeginPlay();

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFactoryManagerSubsystem* FactoryManager = GameInstance->GetSubsystem<UFactoryManagerSubsystem>())
		{
			FactoryManager->RegisterConveyor(this);
		}
	}

	RestartItemMoveTimer();
	RefreshItemVisualInstances();
	UpdateDebugTextFacingPlayer();
}

void AConveyor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFactoryManagerSubsystem* FactoryManager = GameInstance->GetSubsystem<UFactoryManagerSubsystem>())
		{
			FactoryManager->UnregisterConveyor(this);
		}
	}

	Super::EndPlay(EndPlayReason);
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

	// [OJJ F3.7-0] 새 경로 = 평면으로 시작(이전 경로의 노드 Z가 stale로 남는 것 방어).
	// 경사 Z는 그리드가 SetPath 직후 OJJ_SetPathNodeLocalZs로 별도 주입(호출 계약).
	PathNodeLocalZs.Reset();
	// [OJJ F3.9] 포트 꺾임 방향도 동일 계약 — 새 경로는 미주입(기존 동작)으로 시작.
	OJJ_StartPortFlowDir = FIntPoint::ZeroValue;
	OJJ_EndPortFlowDir = FIntPoint::ZeroValue;

	RebuildVisuals();
	RefreshItemVisualInstances();
	UpdateDebugStateText();
}

void AConveyor::OJJ_SetPathNodeLocalZs(const TArray<float>& NewNodeLocalZs)
{
	// [OJJ F3.7-0, F3.8'''] 크기 불일치는 주입 거부(평면 유지) — 액터 신뢰 금지(그리드 ㉲ 검증과
	// 같은 취지). 노드 수 = 셀 수 + 1(셀 경계, 양 끝 포함).
	if (NewNodeLocalZs.Num() != 0 && NewNodeLocalZs.Num() != PathCells.Num() + 1)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Conveyor] OJJ_SetPathNodeLocalZs: 크기 불일치(%d != PathCells+1 %d) — 무시(평면 유지)"),
			NewNodeLocalZs.Num(), PathCells.Num() + 1);
		return;
	}

	PathNodeLocalZs = NewNodeLocalZs;
	RebuildVisuals();
	RefreshItemVisualInstances();
}

void AConveyor::OJJ_SetPortFlowDirections(FIntPoint StartFlowDir, FIntPoint EndFlowDir)
{
	// [OJJ F3.9] 포트 꺾임 흐름 방향 — RebuildVisuals의 코너 판정 보충용(Zero = 보충 없음).
	OJJ_StartPortFlowDir = StartFlowDir;
	OJJ_EndPortFlowDir = EndFlowDir;
	RebuildVisuals();
}

float AConveyor::OJJ_GetPathNodeLocalZ(int32 NodeIndex) const
{
	// 빈 배열(평면 — 기존 전 경로)·무효 인덱스 = 0: 소비 수식이 +0/pitch 0/×1 항등(㊉ 유지).
	return PathNodeLocalZs.IsValidIndex(NodeIndex) ? PathNodeLocalZs[NodeIndex] : 0.0f;
}

float AConveyor::OJJ_GetPathCellLocalZByIndex(int32 PathIndex) const
{
	// 셀 중심 = 양 경계 노드의 중점 — 셀 내 면이 선형(꺾임점은 항상 셀 경계)이라 면의 중심값과
	// 정확히 동일. 빈 배열이면 0+0 → 0(평면 항등).
	return 0.5f * (OJJ_GetPathNodeLocalZ(PathIndex) + OJJ_GetPathNodeLocalZ(PathIndex + 1));
}

float AConveyor::OJJ_GetPathCellLocalZByCell(FIntPoint Cell) const
{
	if (PathNodeLocalZs.Num() == 0)
	{
		return 0.0f; // 평면 — 탐색 비용 0(기존 경로 영향 없음).
	}

	// 경사 경로만 도달 — 경로 길이 n의 선형 탐색(프로토 수용, 아이템 슬롯 셀은 PathCells의 부분집합).
	return OJJ_GetPathCellLocalZByIndex(PathCells.IndexOfByKey(Cell));
}

void AConveyor::ConfigureTransport(
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
	ResetItemSlots();
	RestartItemMoveTimer();
	RefreshItemVisualInstances();
	UpdateDebugStateText();
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFactoryManagerSubsystem* FactoryManager = GameInstance->GetSubsystem<UFactoryManagerSubsystem>())
		{
			FactoryManager->NotifyConveyorChanged(this);
		}
	}

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
	PathNodeLocalZs.Reset(); // [OJJ F3.7-0] 경로와 한 쌍 — 함께 정리.
	OJJ_StartPortFlowDir = FIntPoint::ZeroValue; // [OJJ F3.9] 포트 꺾임 방향도 함께 정리.
	OJJ_EndPortFlowDir = FIntPoint::ZeroValue;
	OccupiedGridCells.Reset();
	SourceMachine.Reset();
	TargetMachine.Reset();
	ItemSlots.Reset();
	PreviousItemSlots.Reset();
	ItemVisualIds.Reset();
	PreviousItemVisualIds.Reset();
	NextItemVisualId = 1;
	StopItemMoveTimer();
	RebuildVisuals();
	RefreshItemVisualInstances();
	UpdateDebugStateText();
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFactoryManagerSubsystem* FactoryManager = GameInstance->GetSubsystem<UFactoryManagerSubsystem>())
		{
			FactoryManager->NotifyConveyorChanged(this);
		}
	}
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

void AConveyor::UpdateDebugTextFacingPlayer()
{
	if (!bShowDebugStateText || !DebugStateText)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!PlayerController)
	{
		return;
	}

	APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager;
	if (!CameraManager)
	{
		return;
	}

	FVector ToCamera = CameraManager->GetCameraLocation() - DebugStateText->GetComponentLocation();
	ToCamera.Z = 0.0f;
	if (ToCamera.IsNearlyZero())
	{
		return;
	}

	const float FacingYaw = ToCamera.Rotation().Yaw;
	DebugStateText->SetWorldRotation(FRotator(0.0f, FacingYaw, 0.0f));
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

	// 피벗을 belt centroid로 이동 → 셀 로컬좌표에서 centroid를 차감해 Root 기준으로 재배치.
	const FVector Centroid = GetPathCentroidLocal();

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
		// [OJJ F3.9] 포트 꺾임 보충: 끝 셀의 경로 밖(머신 안) 방향을 포트 흐름 방향으로 채워 코너
		// 판정에 참여 — 옆 접근 시 끝 세그먼트가 직선 대신 코너(좌/우는 기존 CornerToYaw90 단일
		// 규칙). 정면 접근(포트 방향 = 진행 방향)은 직각이 아니라 직선 그대로, 미주입(Zero — 기존
		// 전 경로 포함)은 보충 자체가 없어 완전 기존 동작. 경사 끝 셀(노드 델타 ≠ 0)은 코너 메시가
		// 평면 전제(㊅)라 보충 생략 — 직선 유지(known limitation).
		const bool bCellFlat =
			FMath::Abs(OJJ_GetPathNodeLocalZ(Index + 1) - OJJ_GetPathNodeLocalZ(Index)) <= KINDA_SMALL_NUMBER;
		const FIntPoint EffPrevDirection =
			(!bHasPrevious && bCellFlat) ? OJJ_StartPortFlowDir : PreviousDirection;
		const FIntPoint EffNextDirection =
			(!bHasNext && bCellFlat) ? OJJ_EndPortFlowDir : NextDirection;
		// 직각만 코너로 인정: prev==next는 직선, prev+next==0은 U턴(반대방향) → 코너 아님(직선 흐름 처리).
		const bool bIsRightAngle = EffPrevDirection != EffNextDirection
			&& (EffPrevDirection + EffNextDirection) != FIntPoint::ZeroValue;
		const bool bIsCorner = EffPrevDirection != FIntPoint::ZeroValue
			&& EffNextDirection != FIntPoint::ZeroValue
			&& bIsRightAngle;
		const FIntPoint VisualDirection = bHasNext ? NextDirection : PreviousDirection;

		// [OJJ F3.7-0] 셀별 로컬 Z 가산 — 평면(빈 배열)은 +0.0f로 기존과 수치 동일(㊉).
		const FVector LocalLocation(
			(CurrentCell.X * CellSize) + (CellSize * 0.5f) - Centroid.X,
			(CurrentCell.Y * CellSize) + (CellSize * 0.5f) - Centroid.Y,
			ZOffset + OJJ_GetPathCellLocalZByIndex(Index));

		if (bIsCorner)
		{
			// ㄱ자 코너 메시: XZ평면 수직 벽 → Roll 90(X축)로 XY바닥에 눕히고 진행면을 위로.
			// 합성 YawQuat * RollQuat → Roll(눕히기) 먼저, Yaw(격자 코너 방향) 나중.
			// [OJJ F3.9] Eff 방향 사용 — 경로 안 코너는 원본과 동일값(보충은 끝 셀에서만 발생).
			const FQuat YawQuat(FRotator(0.0f, CornerToYaw90(EffPrevDirection, EffNextDirection) + CornerBaseYaw, 0.0f));
			const FQuat RollQuat(FRotator(0.0f, 0.0f, 90.0f));   // XZ벽 → XY바닥
			const FQuat CornerQuat = YawQuat * RollQuat;
			// 메시가 셀 한 칸에 맞게 제작됨 → 원본 크기 그대로(보정 불필요). 균일 스케일이라 회전 왜곡 0.
			// ClampMin은 에디터 전용 → 런타임에서도 0/음수 방지(BP/직렬화 대비).
			const float CornerUniform = FMath::Max(0.01f, CornerScaleMultiplier);
			const FVector CornerScale(CornerUniform, CornerUniform, CornerUniform);
			CornerSegmentInstances->AddInstance(FTransform(CornerQuat, LocalLocation, CornerScale));
			continue;
		}

		// 직선 메시: 코너와 동일 구조(균일 스케일 + 자세 보정).
		// 세워진 메시를 Roll로 XY바닥에 눕히고(StraightRoll), 흐름 방향 Yaw(+오프셋)로 정렬.
		// 합성 YawQuat * RollQuat → Roll(눕히기) 먼저, Yaw(진행 방향) 나중. 코너와 같은 순서.
		// [OJJ F3.7-0] 경사 직선(㊄): 셀 진입/진출 경계 노드 간 ΔZ로 기울기 + 빗변 길이 스케일.
		// [OJJ F3.8'''] ΔZ = 자기 셀의 경계 노드 델타 — 면의 꺾임점이 항상 셀 경계라 세그먼트가
		// 면과 전 구간 정확 일치(이전 셀 중심 간 델타 휴리스틱은 전환부 ±행간/4 부유 — 현 문제).
		// [OJJ F3.7-2 fix] 기울기는 "흐름 벡터를 +Z로 들어 올리는 월드 수평축(dy, −dx)" 회전을
		// **좌측 합성** — 이전의 Yaw*Pitch*Roll 우측 합성은 YawQuat에 메시 자세 보정
		// StraightBaseYaw(+90°)가 섞여 있어 pitch 축이 90° 돌아간 roll(좌우 기울기)로 나타났음
		// (PIE 버그). 월드축 좌측 합성은 4방향(동서남북 진행) 전부에서 진행 방향 위아래 기울기.
		// 평면(빈 배열)은 ΔZ=0 → 각도 0의 정확한 항등 쿼터니언(sin 0) 좌측 곱이라 기존
		// Yaw*Roll 결과와 비트 동일(㊉ 유지). 단일 셀 경로는 축이 영벡터지만 각도도 0 — 항등 안전.
		const float DeltaZ = OJJ_GetPathNodeLocalZ(Index + 1) - OJJ_GetPathNodeLocalZ(Index);
		const FQuat YawQuat(FRotator(0.0f, DirectionToYaw(VisualDirection) + StraightBaseYaw, 0.0f));
		const FVector PitchAxis((float)VisualDirection.Y, -(float)VisualDirection.X, 0.0f);
		const FQuat PitchQuat(PitchAxis.GetSafeNormal(), FMath::Atan2(DeltaZ, CellSize));
		const FQuat RollQuat(FRotator(0.0f, 0.0f, StraightRoll));
		const FQuat StraightQuat = PitchQuat * YawQuat * RollQuat;
		// 메시가 셀 한 칸에 맞게 제작됨 → 균일 스케일이라 회전 왜곡 0. ClampMin은 에디터 전용이라 런타임 방어.
		const float StraightUniform = FMath::Max(0.01f, StraightScaleMultiplier);
		// [OJJ F3.7-0] 빗변 보정은 흐름 축에만 — 흐름 축이 메시 로컬의 어느 축인지는 베이스 자세
		// (StraightBaseYaw/Roll 튜너블)에 의존하므로 pitch 제외 합성의 역회전으로 산출(90° 격자 = 주성분 축).
		const float SlopeLength = FMath::Sqrt(1.0f + FMath::Square(DeltaZ / CellSize));
		const FVector MeshFlowAxis = (YawQuat * RollQuat).UnrotateVector(
			FVector((float)VisualDirection.X, (float)VisualDirection.Y, 0.0f)).GetAbs();
		const FVector StraightScale =
			FVector(StraightUniform) * (FVector::OneVector + (SlopeLength - 1.0f) * MeshFlowAxis);
		StraightSegmentInstances->AddInstance(FTransform(StraightQuat, LocalLocation, StraightScale));
	}
}

void AConveyor::ResetItemSlots()
{
	ItemSlots.SetNum(OccupiedGridCells.Num());
	for (FName& ItemSlot : ItemSlots)
	{
		ItemSlot = NAME_None;
	}

	ItemVisualIds.Init(INDEX_NONE, OccupiedGridCells.Num());
	PreviousItemSlots = ItemSlots;
	PreviousItemVisualIds = ItemVisualIds;
	LastItemMoveWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	NextItemVisualId = 1;
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

	LastItemMoveWorldTime = World->GetTimeSeconds();

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
		PreviousItemSlots = ItemSlots;
		PreviousItemVisualIds = ItemVisualIds;
		LastItemMoveWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastItemMoveWorldTime;
		RefreshItemVisualInstances();
		UpdateDebugStateText();
		return;
	}

	PreviousItemSlots = ItemSlots;
	PreviousItemVisualIds = ItemVisualIds;

	const int32 LastIndex = ItemSlots.Num() - 1;
	const FName LastItem = ItemSlots[LastIndex];
	if (!LastItem.IsNone())
	{
		if (IsSolidItem(LastItem) && TargetMachine->CanReceiveConveyorItem(LastItem, 1))
		{
			if (TargetMachine->ReceiveConveyorItem(LastItem, 1))
			{
				ItemSlots[LastIndex] = NAME_None;
				ItemVisualIds[LastIndex] = INDEX_NONE;
			}
		}
	}

	for (int32 Index = LastIndex; Index > 0; --Index)
	{
		if (ItemSlots[Index].IsNone() && !ItemSlots[Index - 1].IsNone())
		{
			ItemSlots[Index] = ItemSlots[Index - 1];
			ItemSlots[Index - 1] = NAME_None;
			ItemVisualIds[Index] = ItemVisualIds[Index - 1];
			ItemVisualIds[Index - 1] = INDEX_NONE;
		}
	}

	if (ItemSlots[0].IsNone())
	{
		FName NewItem = NAME_None;
		if (SourceMachine->PeekFirstOutputItem(NewItem)
			&& IsSolidItem(NewItem)
			&& SourceMachine->TryTakeFirstOutputItem(NewItem))
		{
			ItemSlots[0] = NewItem;
			ItemVisualIds[0] = NextItemVisualId++;
		}
	}

	LastItemMoveWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastItemMoveWorldTime;
	RefreshItemVisualInstances();
	UpdateDebugStateText();
}

bool AConveyor::IsSolidItem(FName ItemID) const
{
	if (!ResourceTable || ItemID.IsNone())
	{
		return false;
	}

	const FResourceData* Resource = ResourceTable->FindRow<FResourceData>(ItemID, TEXT("Conveyor.IsSolidItem"));
	return Resource && Resource->form == FName(TEXT("solid"));
}

void AConveyor::RefreshItemVisualInstances()
{
	if (!ItemVisualInstances)
	{
		return;
	}

	ItemVisualInstances->ClearInstances();
	if (!HasVisibleItems())
	{
		return;
	}

	const float MoveAlpha = GetCurrentMoveAlpha();
	const float ItemScale = FMath::Max(0.01f, CellSize * ItemVisualScaleRatio / 100.0f);
	const FVector ItemVisualScale(ItemScale, ItemScale, ItemScale);

	for (int32 SlotIndex = 0; SlotIndex < ItemSlots.Num(); ++SlotIndex)
	{
		if (ItemSlots[SlotIndex].IsNone())
		{
			continue;
		}

		const FVector StartLocation = ResolveItemVisualStartLocation(SlotIndex);
		const FVector EndLocation = GetSlotLocalCenter(SlotIndex);
		const FVector ItemLocation = FMath::Lerp(StartLocation, EndLocation, MoveAlpha);
		ItemVisualInstances->AddInstance(FTransform(FRotator::ZeroRotator, ItemLocation, ItemVisualScale));
	}
}

float AConveyor::GetCurrentMoveAlpha() const
{
	if (SecondsPerGrid <= KINDA_SMALL_NUMBER)
	{
		return 1.0f;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return 1.0f;
	}

	const float RawAlpha = FMath::Clamp((World->GetTimeSeconds() - LastItemMoveWorldTime) / SecondsPerGrid, 0.0f, 1.0f);
	if (ItemVisualLerpExponent <= KINDA_SMALL_NUMBER)
	{
		return RawAlpha;
	}

	return FMath::Pow(RawAlpha, ItemVisualLerpExponent);
}

FVector AConveyor::GetCellLocalCenter(FIntPoint Cell) const
{
	// [OJJ F3.7-0] 셀별 로컬 Z 가산 — 아이템 보간(Lerp)/진입 외삽은 FVector 연산이라 Z가 자동
	// 통과. 평면(빈 배열)은 +0.0f로 기존과 수치 동일(㊉).
	const FVector Centroid = GetPathCentroidLocal();
	return FVector(
		(Cell.X * CellSize) + (CellSize * 0.5f) - Centroid.X,
		(Cell.Y * CellSize) + (CellSize * 0.5f) - Centroid.Y,
		ZOffset + ItemVisualZOffset + OJJ_GetPathCellLocalZByCell(Cell));
}

FVector AConveyor::GetSlotLocalCenter(int32 SlotIndex) const
{
	if (!OccupiedGridCells.IsValidIndex(SlotIndex))
	{
		return FVector(0.0f, 0.0f, ZOffset + ItemVisualZOffset);
	}

	return GetCellLocalCenter(OccupiedGridCells[SlotIndex]);
}

FVector AConveyor::GetIncomingItemLocalCenter() const
{
	if (OccupiedGridCells.Num() == 0)
	{
		return FVector(0.0f, 0.0f, ZOffset + ItemVisualZOffset);
	}

	if (OccupiedGridCells.Num() == 1)
	{
		return GetSlotLocalCenter(0);
	}

	const FVector FirstCenter = GetSlotLocalCenter(0);
	const FVector SecondCenter = GetSlotLocalCenter(1);
	return FirstCenter - (SecondCenter - FirstCenter);
}

FVector AConveyor::ResolveItemVisualStartLocation(int32 SlotIndex) const
{
	const FVector CurrentLocation = GetSlotLocalCenter(SlotIndex);
	if (!ItemVisualIds.IsValidIndex(SlotIndex))
	{
		return CurrentLocation;
	}

	const int32 VisualId = ItemVisualIds[SlotIndex];
	if (VisualId == INDEX_NONE)
	{
		return CurrentLocation;
	}

	const int32 PreviousSlotIndex = FindPreviousVisualSlotIndex(VisualId);
	if (PreviousSlotIndex != INDEX_NONE)
	{
		return GetSlotLocalCenter(PreviousSlotIndex);
	}

	if (SlotIndex == 0)
	{
		return GetIncomingItemLocalCenter();
	}

	return CurrentLocation;
}

int32 AConveyor::FindPreviousVisualSlotIndex(int32 VisualId) const
{
	if (VisualId == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < PreviousItemVisualIds.Num(); ++Index)
	{
		if (PreviousItemVisualIds[Index] == VisualId)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

bool AConveyor::HasVisibleItems() const
{
	for (const FName& ItemSlot : ItemSlots)
	{
		if (!ItemSlot.IsNone())
		{
			return true;
		}
	}

	return false;
}

FVector AConveyor::GetPathCentroidLocal() const
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
	Center.Z = 0.0f;
	return Center;
}

FVector AConveyor::GetDebugTextLocalLocation() const
{
	// 비주얼이 centroid 기준(RebuildVisuals)으로 그려지므로, 디버그텍스트도 centroid 기준 오프셋만 사용.
	return DebugTextOffset;
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
