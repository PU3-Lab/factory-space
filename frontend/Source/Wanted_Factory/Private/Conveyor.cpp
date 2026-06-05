// Fill out your copyright notice in the Description page of Project Settings.

#include "Conveyor.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "FactoryManagerSubsystem.h"
#include "MachineBase.h"
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
}

void AConveyor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	RefreshItemVisualInstances();
}

void AConveyor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	RebuildVisuals();
	RefreshItemVisualInstances();
	UpdateDebugStateText();
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

	RebuildVisuals();
	RefreshItemVisualInstances();
	UpdateDebugStateText();
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
	OccupiedGridCells.Reset();
	SourceMachine.Reset();
	TargetMachine.Reset();
	ItemSlots.Reset();
	PreviousItemSlots.Reset();
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
		// 직각만 코너로 인정: prev==next는 직선, prev+next==0은 U턴(반대방향) → 코너 아님(직선 흐름 처리).
		const bool bIsRightAngle = PreviousDirection != NextDirection
			&& (PreviousDirection + NextDirection) != FIntPoint::ZeroValue;
		const bool bIsCorner = bHasPrevious && bHasNext && bIsRightAngle;
		const FIntPoint VisualDirection = bHasNext ? NextDirection : PreviousDirection;

		const FVector LocalLocation(
			(CurrentCell.X * CellSize) + (CellSize * 0.5f) - Centroid.X,
			(CurrentCell.Y * CellSize) + (CellSize * 0.5f) - Centroid.Y,
			ZOffset);

		if (bIsCorner)
		{
			// ㄱ자 코너 메시: XZ평면 수직 벽 → Roll 90(X축)로 XY바닥에 눕히고 진행면을 위로.
			// 합성 YawQuat * RollQuat → Roll(눕히기) 먼저, Yaw(격자 코너 방향) 나중.
			const FQuat YawQuat(FRotator(0.0f, CornerToYaw90(PreviousDirection, NextDirection) + CornerBaseYaw, 0.0f));
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
		const FQuat YawQuat(FRotator(0.0f, DirectionToYaw(VisualDirection) + StraightBaseYaw, 0.0f));
		const FQuat RollQuat(FRotator(0.0f, 0.0f, StraightRoll));
		const FQuat StraightQuat = YawQuat * RollQuat;
		// 메시가 셀 한 칸에 맞게 제작됨 → 균일 스케일이라 회전 왜곡 0. ClampMin은 에디터 전용이라 런타임 방어.
		const float StraightUniform = FMath::Max(0.01f, StraightScaleMultiplier);
		const FVector StraightScale(StraightUniform, StraightUniform, StraightUniform);
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

	PreviousItemSlots = ItemSlots;
	LastItemMoveWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
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
		LastItemMoveWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastItemMoveWorldTime;
		RefreshItemVisualInstances();
		UpdateDebugStateText();
		return;
	}

	PreviousItemSlots = ItemSlots;

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

	LastItemMoveWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastItemMoveWorldTime;
	RefreshItemVisualInstances();
	UpdateDebugStateText();
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
	const FVector Centroid = GetPathCentroidLocal();
	return FVector(
		(Cell.X * CellSize) + (CellSize * 0.5f) - Centroid.X,
		(Cell.Y * CellSize) + (CellSize * 0.5f) - Centroid.Y,
		ZOffset + ItemVisualZOffset);
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
	if (!PreviousItemSlots.IsValidIndex(SlotIndex))
	{
		return CurrentLocation;
	}

	if (PreviousItemSlots[SlotIndex] == ItemSlots[SlotIndex])
	{
		return CurrentLocation;
	}

	if (SlotIndex > 0
		&& PreviousItemSlots.IsValidIndex(SlotIndex - 1)
		&& !PreviousItemSlots[SlotIndex - 1].IsNone())
	{
		return GetSlotLocalCenter(SlotIndex - 1);
	}

	if (SlotIndex == 0)
	{
		return GetIncomingItemLocalCenter();
	}

	return CurrentLocation;
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
