// Fill out your copyright notice in the Description page of Project Settings.

#include "WaterArea.h"

#include "Wanted_Factory.h"
#include "OJJ_Grid.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AWaterArea::AWaterArea()
{
	PrimaryActorTick.bCanEverTick = false;

	// Water는 무한 수원 — 펌프가 ConsumeResource로 차감해도 고갈되지 않도록 강제(스펙: 무제한·무한).
	// form=liquid는 DataTable(ResourceData)에서 지정. bIsInfinite는 액터 프로퍼티라 여기서 기본 보장한다
	// (AResourceBase 기본값 false → 미설정 시 유한 소진 버그).
	bIsInfinite = true;

	// 강 비주얼 플레인(영역 전체 한 장). 점유는 그리드가 담당하므로 충돌은 끈다(클릭/오버랩 간섭 방지).
	WaterPlaneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WaterPlane"));
	WaterPlaneMesh->SetupAttachment(Root);
	WaterPlaneMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WaterPlaneMesh->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		WaterPlaneMesh->SetStaticMesh(PlaneMesh.Object);
	}
}

void AWaterArea::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateWaterVisual();
}

void AWaterArea::UpdateWaterVisual()
{
	if (!WaterPlaneMesh)
	{
		return;
	}

	// 그리드 CellSize는 protected라 직접 못 읽으므로 public GridToWorld로 역산(없으면 기본 100).
	float CellSize = 100.0f;
	if (const UWorld* World = GetWorld())
	{
		if (const AOJJ_Grid* Grid = Cast<AOJJ_Grid>(UGameplayStatics::GetActorOfClass(World, AOJJ_Grid::StaticClass())))
		{
			const float Derived = (Grid->GridToWorld(FIntPoint(1, 0)) - Grid->GridToWorld(FIntPoint(0, 0))).X;
			if (Derived > KINDA_SMALL_NUMBER)
			{
				CellSize = Derived;
			}
		}
	}

	const int32 SizeX = FMath::Max(1, AreaSizeInCells.X);
	const int32 SizeY = FMath::Max(1, AreaSizeInCells.Y);

	// 엔진 기본 Plane은 100uu 정사각형(중심 원점). 영역 전체(SizeX×SizeY 셀)를 한 장으로 스케일.
	WaterPlaneMesh->SetRelativeScale3D(FVector(SizeX * CellSize / 100.0f, SizeY * CellSize / 100.0f, 1.0f));

	// 액터 원점(좌하단 셀 중심 — RegisterToGrid가 런타임에 스냅)을 기준으로 영역 중심으로 이동 + z오프셋.
	const float OffsetX = (SizeX - 1) * CellSize * 0.5f;
	const float OffsetY = (SizeY - 1) * CellSize * 0.5f;
	WaterPlaneMesh->SetRelativeLocation(FVector(OffsetX, OffsetY, VisualZOffset));

	if (WaterMaterial)
	{
		WaterPlaneMesh->SetMaterial(0, WaterMaterial);
	}
}

void AWaterArea::RegisterToGrid()
{
	// 그리드 점유는 서버 권위에서만 기록(부모/OJJ_RegisterActorCells 규약과 동일).
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AOJJ_Grid* Grid = Cast<AOJJ_Grid>(UGameplayStatics::GetActorOfClass(World, AOJJ_Grid::StaticClass()));
	if (!Grid)
	{
		LOG_OJJ_W(TEXT("WaterArea RegisterToGrid skipped: no AOJJ_Grid in world. Water=%s"), *GetName());
		return;
	}

	// 좌하단 origin 셀. XY를 셀 중심으로 스냅(점유 셀과 시각 정합). Z는 보존.
	const FIntPoint Origin = Grid->WorldToGrid(GetActorLocation());
	const FVector OriginCenter = Grid->GridToWorld(Origin);
	SetActorLocation(FVector(OriginCenter.X, OriginCenter.Y, GetActorLocation().Z));

	// AreaSizeInCells 범위의 셀 수집(최소 1x1 보장).
	const int32 SizeX = FMath::Max(1, AreaSizeInCells.X);
	const int32 SizeY = FMath::Max(1, AreaSizeInCells.Y);
	TArray<FIntPoint> Cells;
	Cells.Reserve(SizeX * SizeY);
	for (int32 X = 0; X < SizeX; ++X)
	{
		for (int32 Y = 0; Y < SizeY; ++Y)
		{
			Cells.Add(Origin + FIntPoint(X, Y));
		}
	}

	// 다중 셀 일괄 등록. 한 셀이라도 충돌/off-grid면 전체 실패(OJJ_RegisterActorCells 원자성).
	if (Grid->OJJ_RegisterActorCells(this, Cells))
	{
		RegisteredOrigin = Origin;
		bRegistered = true;

		// 런타임 셀 스냅 + 실제 그리드 CellSize 반영(에디터 OnConstruction 시 그리드가 없었을 경우 대비).
		UpdateWaterVisual();
	}
	else
	{
		LOG_OJJ_W(
			TEXT("WaterArea RegisterToGrid failed (cell occupied or off-grid): Water=%s Origin=(%d,%d) Size=(%d,%d)"),
			*GetName(), Origin.X, Origin.Y, SizeX, SizeY);
	}
}

void AWaterArea::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 다중 셀 대칭 해제: 등록 origin 셀 하나로 OJJ_RemoveActorAt → 액터가 점유한 전체 셀을 일괄 정리
	// (OJJ_Grid가 OJJ_ActorToCells 역맵으로 전체 셀을 제거). 광맥 때 Claim 누수 잡았던 등록/해제 대칭 원칙.
	if (bRegistered)
	{
		if (UWorld* World = GetWorld())
		{
			if (AOJJ_Grid* Grid = Cast<AOJJ_Grid>(UGameplayStatics::GetActorOfClass(World, AOJJ_Grid::StaticClass())))
			{
				Grid->OJJ_RemoveActorAt(RegisteredOrigin);
			}
		}
		bRegistered = false;
	}

	Super::EndPlay(EndPlayReason);
}
