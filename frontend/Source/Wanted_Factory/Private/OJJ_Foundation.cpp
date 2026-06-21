// Fill out your copyright notice in the Description page of Project Settings.

#include "OJJ_Foundation.h"

#include "Components/InstancedStaticMeshComponent.h"
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

	// [Deck] 윗면 비주얼 = Deck 메시(엔진 Cube 대체). 머티리얼은 에셋에 포함돼 SetStaticMesh로 함께 적용.
	// 고스트(OJJ_ShowGhostForFoundation)는 CDO GetSlabMesh()를 읽으므로 이 교체로 고스트도 자동 Deck.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DeckMesh(TEXT("/Game/Assets/Platform/Deck/StaticMeshes/Deck.Deck"));
	if (DeckMesh.Succeeded())
	{
		SlabMesh->SetStaticMesh(DeckMesh.Object);
	}

	// [Deck step2] 4모서리 다리(TrussTower) ISM — 평지·램프 공용. 충돌 없음(시각 지지대; 걷기 충돌은 데크 슬래브).
	LegISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LegISM"));
	LegISM->SetupAttachment(RootComponent);
	LegISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LegISM->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> TrussMesh(TEXT("/Game/Assets/Platform/TrussTower/StaticMeshes/TrussTower.TrussTower"));
	if (TrussMesh.Succeeded())
	{
		LegISM->SetStaticMesh(TrussMesh.Object);
	}
}

void AOJJ_Foundation::OJJ_ComputeDeckSlabTransform(const UStaticMesh* Mesh, const FRotator& Rot,
	float TargetX, float TargetY, float Thickness, FVector& OutScale, FVector& OutOffset)
{
	OutScale = FVector::OneVector;
	OutOffset = FVector::ZeroVector;
	if (!Mesh)
	{
		return;
	}
	const FBox Box = Mesh->GetBoundingBox();
	const FVector Native = Box.GetSize();
	// 엔진 Cube(100^3) 가정 폐기 — 실제 바운즈로 footprint(XY)=Target, 두께(Z)=Thickness 맞춤(per-axis).
	OutScale = FVector(
		TargetX / FMath::Max(Native.X, 1.0f),
		TargetY / FMath::Max(Native.Y, 1.0f),
		Thickness / FMath::Max(Native.Z, 1.0f));
	// 회전·스케일 후 AABB로 피벗 보정: XY는 박스중심을 원점에, Z는 윗면을 +Thickness에(상면 높이 불변).
	const FBox Eff = Box.TransformBy(FTransform(Rot, FVector::ZeroVector, OutScale));
	const FVector C = Eff.GetCenter();
	OutOffset = FVector(-C.X, -C.Y, Thickness - Eff.Max.Z);
}

void AOJJ_Foundation::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateSlabVisual();
	UpdateLegVisual();
}

void AOJJ_Foundation::GetSaveState(
	int32& OutRiseSteps,
	bool& bOutOneSideGroundRamp,
	float& OutLoEndLowestGroundRaw,
	bool& bOutLoEndLowestValid) const
{
	OutRiseSteps = 0;
	bOutOneSideGroundRamp = false;
	OutLoEndLowestGroundRaw = 0.0f;
	bOutLoEndLowestValid = false;
}

void AOJJ_Foundation::ApplySaveState(
	int32 InRiseSteps,
	bool bInOneSideGroundRamp,
	float InLoEndLowestGroundRaw,
	bool bInLoEndLowestValid)
{
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
	if (!SlabMesh || !SlabMesh->GetStaticMesh())
	{
		return;
	}

	const float CellSize = OJJ_ResolveCellSize();
	const int32 SizeX = FMath::Max(1, FoundationSize.X);
	const int32 SizeY = FMath::Max(1, FoundationSize.Y);
	const float SlabThickness = FMath::Max(1.0f, Thickness);

	// [Deck] 실제 Deck 바운즈로 footprint(8셀)×Thickness 맞춤(공용 헬퍼 = 고스트와 정합). 액터 원점 = 풋프린트
	// 중심 → XY 박스중심 정렬, 윗면 = 액터 Z + Thickness **불변**(머신 배치·사다리 step-off·등반 기준이라 절대 유지).
	FVector Scale, Offset;
	OJJ_ComputeDeckSlabTransform(SlabMesh->GetStaticMesh(), SlabMeshLocalRotation,
		SizeX * CellSize, SizeY * CellSize, SlabThickness, Scale, Offset);
	SlabMesh->SetRelativeRotation(SlabMeshLocalRotation);
	SlabMesh->SetRelativeScale3D(Scale);
	SlabMesh->SetRelativeLocation(Offset);
}

void AOJJ_Foundation::UpdateLegVisual()
{
	if (!LegISM || !LegISM->GetStaticMesh())
	{
		return;
	}

	const float CellSize = OJJ_ResolveCellSize();
	const int32 SizeX = FMath::Max(1, FoundationSize.X);
	const int32 SizeY = FMath::Max(1, FoundationSize.Y);
	// footprint 꼭짓점(가장자리)까지 = Size/2 × CellSize (8셀 → 400uu). 다리는 굵기만큼 안으로 들어와 바깥면이 꼭짓점.
	const float FootHalfX = SizeX * 0.5f * CellSize;
	const float FootHalfY = SizeY * 0.5f * CellSize;

	// 굵기는 XY만 스케일(Z=1 고정 → SegZ/타일링 단위 보존). 높이 그대로, 가로/세로만 굵게.
	const float LegThick = FMath::Max(LegMeshScaleMultiplier, 0.01f);
	const FVector LegScale(LegThick, LegThick, 1.0f);
	const FBox Eff = LegISM->GetStaticMesh()->GetBoundingBox().TransformBy(
		FTransform(LegMeshLocalRotation, FVector::ZeroVector, LegScale));
	const float SegZ = Eff.GetSize().Z;        // 1세그 세로높이(Z 스케일 1 → 네이티브 = 타일링 단위, 굵기 무관)

	// 3단계: 모서리별 지형까지 타일링(사다리 Overshoot 패턴). DeckBottom(액터 Z)에서 각 모서리 지형Z까지 N세그 —
	// 위(상대 0=DeckBottom)는 딱, 남는 건 지형 아래로 박힘(맵 박힘 OK). 4모서리 독립(울퉁불퉁 대응).
	// 코너 깊이 = 다리 실제 월드 XY의 라이브 하향 트레이스(OJJ_TraceTerrainZAtWorldXY, 베이크 ignore: 머신/컨베이어/
	// WaterArea/Foundation). ⚠️ 셀 대표값(baked 5점 최고점)은 급경사 벽 코너를 못 잡아 얕게 걸침 → 실제 XY 트레이스.
	const AOJJ_Grid* Grid = RegisteredGrid.Get();
	const FVector ActorLoc = GetActorLocation();
	bool bAnyUnbaked = false;

	LegISM->ClearInstances();
	const float SignX[4] = { -1.0f, +1.0f, -1.0f, +1.0f };
	const float SignY[4] = { -1.0f, -1.0f, +1.0f, +1.0f };

	// 4코너 로컬 XY(굵기만큼 안으로, 바깥면=꼭짓점) + 월드 XY 선산출 → 1회 일괄 트레이스(ignore 목록 중복 구축 회피).
	float PosX[4], PosY[4];
	TArray<FVector2D> CornerWorldXYs;
	CornerWorldXYs.Reserve(4);
	for (int32 i = 0; i < 4; ++i)
	{
		PosX[i] = (SignX[i] > 0.0f) ? (FootHalfX - Eff.Max.X) : (-FootHalfX - Eff.Min.X);
		PosY[i] = (SignY[i] > 0.0f) ? (FootHalfY - Eff.Max.Y) : (-FootHalfY - Eff.Min.Y);
		const FVector LegWorld = GetActorTransform().TransformPosition(FVector(PosX[i], PosY[i], 0.0f));
		CornerWorldXYs.Add(FVector2D(LegWorld.X, LegWorld.Y));
	}

	TArray<float> CornerTerrainZ;
	TArray<bool> CornerHit;
	if (Grid)
	{
		Grid->OJJ_TraceTerrainZAtWorldXY(CornerWorldXYs, CornerTerrainZ, CornerHit);
	}

	for (int32 i = 0; i < 4; ++i)
	{
		// 이 모서리 다리 높이 = DeckBottom(ActorZ) − 코너 지형Z. 미등록·트레이스 미스(void)면 폴백 1세그.
		float Height = SegZ;
		if (Grid && CornerHit.IsValidIndex(i) && CornerHit[i])
		{
			Height = FMath::Max(ActorLoc.Z - CornerTerrainZ[i], SegZ);
		}
		else
		{
			bAnyUnbaked = true;
		}

		// 사다리 패턴: 위(DeckBottom=상대 0)에 딱, 아래로 N세그(N*SegZ ≥ Height → 마지막 세그가 지형/그 아래).
		const int32 N = FMath::Max(1, FMath::CeilToInt(Height / SegZ));
		for (int32 j = 0; j < N; ++j)
		{
			// 세그 j 윗면 = 상대 −j*SegZ → 인스턴스 Z = −j*SegZ − Eff.Max.Z. j=0 윗면 = DeckBottom.
			const float InstZ = -static_cast<float>(j) * SegZ - Eff.Max.Z;
			LegISM->AddInstance(FTransform(LegMeshLocalRotation, FVector(PosX[i], PosY[i], InstZ), LegScale));
		}
	}

	if (bAnyUnbaked)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Leg] 지형 트레이스 미스(void/바닥 없음) — 일부 다리 폴백 1세그(공중 가능). 그리드 미등록 또는 다리 아래 지형 없음."));
	}
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
	// 베이스 = 수동 방향(입력 step 그대로 — F3.6-1 ㊁). 평판은 방향 무의미라 출처 문자열은 비움.
	Result.EffectiveRotationSteps = RotationSteps;
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
	UpdateLegVisual();
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
