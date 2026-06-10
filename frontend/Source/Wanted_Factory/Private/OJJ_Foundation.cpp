// Fill out your copyright notice in the Description page of Project Settings.

#include "OJJ_Foundation.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "OJJ_Grid.h"
#include "UObject/ConstructorHelpers.h"

AOJJ_Foundation::AOJJ_Foundation()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// 임시 비주얼: 엔진 Cube(100uu, 중심 피벗). NoCollision — 커서 트레이스(머신 호버의 bHitMachine
	// 게이트)/베이크 ↓트레이스/배치 클릭에 간섭 0. 걷기 충돌은 F1-c에서 전용 메시와 함께.
	SlabMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SlabMesh"));
	SlabMesh->SetupAttachment(RootComponent);
	SlabMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SlabMesh->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		SlabMesh->SetStaticMesh(CubeMesh.Object);
	}
}

void AOJJ_Foundation::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateSlabVisual();
}

void AOJJ_Foundation::UpdateSlabVisual()
{
	if (!SlabMesh)
	{
		return;
	}

	// 그리드 CellSize는 protected — WaterArea와 동일하게 public GridToWorld로 역산. 단 등록된 그리드를
	// 우선 사용(Codex F1-b #2: 멀티 그리드에서 GetActorOfClass가 다른 그리드를 집어 CellSize가 어긋나는
	// 것 방지). 미등록(스폰 직후/에디터 프리뷰)일 때만 월드 첫 그리드 폴백, 그것도 없으면 기본 100.
	float CellSize = 100.0f;
	const AOJJ_Grid* Grid = RegisteredGrid.Get();
	if (!Grid)
	{
		if (const UWorld* World = GetWorld())
		{
			Grid = Cast<AOJJ_Grid>(UGameplayStatics::GetActorOfClass(World, AOJJ_Grid::StaticClass()));
		}
	}
	if (Grid)
	{
		const float Derived = (Grid->GridToWorld(FIntPoint(1, 0)) - Grid->GridToWorld(FIntPoint(0, 0))).X;
		if (Derived > KINDA_SMALL_NUMBER)
		{
			CellSize = Derived;
		}
	}

	const int32 SizeX = FMath::Max(1, FoundationSize.X);
	const int32 SizeY = FMath::Max(1, FoundationSize.Y);
	const float SlabThickness = FMath::Max(1.0f, Thickness);

	// 액터 원점 = 풋프린트 중심(GetFoundationPlacementLocation 계약) → XY 오프셋 0.
	// Cube(100uu, 중심 피벗): Z중심 = Thickness/2 → 상면이 정확히 평면 + Thickness.
	SlabMesh->SetRelativeScale3D(FVector(SizeX * CellSize / 100.0f, SizeY * CellSize / 100.0f, SlabThickness / 100.0f));
	SlabMesh->SetRelativeLocation(FVector(0.0f, 0.0f, SlabThickness * 0.5f));
}

void AOJJ_Foundation::OJJ_NotifyPlacedOnGrid(AOJJ_Grid* Grid)
{
	RegisteredGrid = Grid;
	// 배치 시점 그리드의 실제 CellSize 확정 반영(스폰 시 OnConstruction에서 그리드를 못 찾았을 경우 대비).
	UpdateSlabVisual();
}

void AOJJ_Foundation::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 등록/해제 대칭(WaterArea의 EndPlay 패턴). RemoveFoundation은 위에 건물이 있으면 거부하지만,
	// 정상 철거 흐름(F1-b')은 호출 전 검증하고, 월드 teardown에선 거부돼도 커버리지 데이터가 월드와 함께
	// 소멸 + write 경로 sweep이 stale을 정리하므로 안전 — 로그만 남긴다.
	if (AOJJ_Grid* Grid = RegisteredGrid.Get())
	{
		if (HasAuthority())
		{
			FString OutReason;
			if (!Grid->RemoveFoundation(this, OutReason) && !OutReason.IsEmpty())
			{
				UE_LOG(LogTemp, Verbose, TEXT("[Foundation] EndPlay 해제 생략: %s"), *OutReason);
			}
		}
		RegisteredGrid = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}
