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

	// 임시 비주얼: 엔진 Cube(100uu, 중심 피벗). F1-c에서 충돌 활성화(결정점 ③ 해제):
	//  - Pawn Block: 걷기 — 캐릭터 무브먼트는 쿼리 스윕이라 QueryOnly로 충분(WaterArea 차단 볼륨과 동일 근거).
	//  - Visibility Block: 커서가 슬래브 상면에 스냅 → Foundation 위 머신 호버/배치 가능
	//    (BuildController 표면 게이트가 bHitFoundation을 허용하도록 함께 확장됨).
	//  - 베이크 ↓트레이스도 Visibility지만 BakeBuildableCells가 Foundation을 ignore(이중 안전).
	//  - Camera Ignore: 빌드/줌 카메라 충돌 간섭 방지.
	SlabMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SlabMesh"));
	SlabMesh->SetupAttachment(RootComponent);
	SlabMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SlabMesh->SetCollisionObjectType(ECC_WorldStatic);
	SlabMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	SlabMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	SlabMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	SlabMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
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

float AOJJ_Foundation::OJJ_ResolveCellSize() const
{
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
	return CellSize;
}

void AOJJ_Foundation::UpdateSlabVisual()
{
	if (!SlabMesh)
	{
		return;
	}

	const float CellSize = OJJ_ResolveCellSize();
	const int32 SizeX = FMath::Max(1, FoundationSize.X);
	const int32 SizeY = FMath::Max(1, FoundationSize.Y);
	const float SlabThickness = FMath::Max(1.0f, Thickness);

	// 액터 원점 = 풋프린트 중심(GetFoundationPlacementLocation 계약) → XY 오프셋 0.
	// Cube(100uu, 중심 피벗): Z중심 = Thickness/2 → 상면이 정확히 평면 + Thickness.
	SlabMesh->SetRelativeScale3D(FVector(SizeX * CellSize / 100.0f, SizeY * CellSize / 100.0f, SlabThickness / 100.0f));
	SlabMesh->SetRelativeLocation(FVector(0.0f, 0.0f, SlabThickness * 0.5f));
}

FOJJFoundationFitResult AOJJ_Foundation::OJJ_ComputeHoverFootprint(const AOJJ_Grid& Grid, FIntPoint CursorCell,
	int32 RotationSteps) const
{
	// 베이스 = 기존 컨트롤러 정적 산출 그대로(F3.6-0 회귀 0): 홀수 step이면 X/Y 스왑(F3-0 ㉱ —
	// EffectiveSize와 동일 parity 규칙) + lower-left origin 공통 수식. Grid는 베이스에서 미사용 —
	// 자동 맞춤(F3.6-1) 이웃 스캔의 입력으로 시그니처에 미리 포함.
	FOJJFoundationFitResult Result;
	Result.EffSize = ((RotationSteps % 2) != 0) ? FIntPoint(FoundationSize.Y, FoundationSize.X) : FoundationSize;
	Result.Origin = AOJJ_Grid::OJJ_OriginFromCursorCellForSize(CursorCell, Result.EffSize);
	return Result;
}

float AOJJ_Foundation::OJJ_ComputeSnapLift(const AOJJ_Grid& Grid, FIntPoint Origin, FIntPoint EffSize,
	int32 RotationSteps, FString* OutHeightSource) const
{
	// F3.5 ①: 이웃 상속 — 평면 확장이 기본(지형 클리핑 허용). 상속값은 이웃 SurfaceZ =
	// 평면+Thickness+N×100 이라 자동으로 단 격자 위 — 리프트(평면 기준)로 환산만 하면 됨.
	// stale 이웃은 그리드 조회가 weak 검증으로 거름(유령 단 차단).
	// 단 격자 원점(평면+Thickness)은 Thickness를 아는 클래스가 산출해 전달 — 그리드는 기계적 필터만.
	const float SnapGridOriginZ = Grid.GetActorLocation().Z + Thickness;
	float NeighborZ = 0.0f;
	int32 ContactCells = 0;
	if (Grid.OJJ_GetNeighborFoundationSurfaceZ(Origin, EffSize, SnapGridOriginZ, NeighborZ, ContactCells))
	{
		if (OutHeightSource)
		{
			*OutHeightSource = FString::Printf(TEXT("상속(이웃 접촉 %d셀)"), ContactCells);
		}
		return NeighborZ - Thickness - Grid.GetActorLocation().Z;
	}

	// ② 고립 첫 장 = 씨앗: 지형 스냅(F2-4 산식 — 풋프린트 전체 max GroundZ). 회전은 평탄에 무의미.
	if (OutHeightSource)
	{
		*OutHeightSource = TEXT("씨앗(지형)");
	}
	return Grid.OJJ_ComputeFoundationSnapLift(Origin, EffSize, Thickness);
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
