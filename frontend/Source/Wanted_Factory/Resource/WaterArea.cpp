// Fill out your copyright notice in the Description page of Project Settings.

#include "WaterArea.h"

#include "Wanted_Factory.h"
#include "OJJ_Grid.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
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

	// Pawn 진입 차단 볼륨 — 영역 전체를 덮는 보이지 않는 벽.
	WaterBlockingVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("WaterBlockingVolume"));
	WaterBlockingVolume->SetupAttachment(Root);
	// 콜리전 설계(핵심): Pawn만 Block, 그 외 전 채널 Ignore. 커스텀 프리셋 대신 명시적 응답 설정.
	// ⚠️ Visibility/Camera Ignore — 빌드 커서 트레이스/베이크 ↓트레이스가 박스 천장을 잡으면 안 됨.
	//    (베이크는 AResourceBase(=WaterArea) 액터를 통째로 ignore하므로 이중 안전.)
	WaterBlockingVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);  // 캐릭터 무브먼트 sweep 차단엔 Query면 충분
	WaterBlockingVolume->SetCollisionObjectType(ECC_WorldStatic);
	WaterBlockingVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	WaterBlockingVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	WaterBlockingVolume->SetCanEverAffectNavigation(false);
	WaterBlockingVolume->SetGenerateOverlapEvents(false);
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

	// Pawn 차단 박스: XY는 영역 전체, Z는 BlockingWallHeight(바닥→위 벽). 절대값 set이라 OnConstruction
	// 재진입에도 중복/누적 없음(박스는 생성자 1회 생성). 중심 XY=플레인과 동일, Z중심=높이/2 → 바닥에서 위로.
	if (WaterBlockingVolume)
	{
		const float HalfH = FMath::Max(1.0f, BlockingWallHeight) * 0.5f;
		WaterBlockingVolume->SetBoxExtent(FVector(SizeX * CellSize * 0.5f, SizeY * CellSize * 0.5f, HalfH), /*bUpdateOverlaps=*/false);
		WaterBlockingVolume->SetRelativeLocation(FVector(OffsetX, OffsetY, HalfH));
	}

	// 머티리얼을 MID로 래핑해 FlowVelocity를 주입(강마다 방향 다름). OnConstruction 경로로도 호출되므로
	// 에디터 배치/FlowVelocity 변경 즉시 프리뷰에 반영된다. 부모 머티리얼이 바뀐 경우에만 MID 재생성.
	if (WaterMaterial)
	{
		if (!WaterMID || WaterMID->Parent != WaterMaterial)
		{
			WaterMID = UMaterialInstanceDynamic::Create(WaterMaterial, this);
		}
		if (WaterMID)
		{
			// M_River의 VectorParameter "FlowVelocity"에 주입. 동명 파라미터가 없으면 조용히 무시(에러 아님).
			WaterMID->SetVectorParameterValue(
				TEXT("FlowVelocity"),
				FLinearColor(FlowVelocity.X, FlowVelocity.Y, 0.0f, 0.0f));
			WaterPlaneMesh->SetMaterial(0, WaterMID);
		}
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
