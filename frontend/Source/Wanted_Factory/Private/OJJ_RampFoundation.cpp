// Fill out your copyright notice in the Description page of Project Settings.

#include "OJJ_RampFoundation.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "OJJ_Grid.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AOJJ_RampFoundation::AOJJ_RampFoundation()
{
	// 계단 박스 ISM — SlabMesh(베이스 생성자)와 동일 충돌 프로파일: 걷기(Pawn Block) +
	// 커서 스냅(Visibility Block) + 카메라 간섭 0. 베이크 ↓트레이스는 TActorIterator<AOJJ_Foundation>
	// ignore가 파생 클래스도 커버(OJJ_Grid 베이크 :720).
	StepMeshISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("StepMeshISM"));
	StepMeshISM->SetupAttachment(RootComponent);
	StepMeshISM->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	StepMeshISM->SetCollisionObjectType(ECC_WorldStatic);
	StepMeshISM->SetCollisionResponseToAllChannels(ECR_Ignore);
	StepMeshISM->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	StepMeshISM->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	StepMeshISM->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	StepMeshISM->SetCanEverAffectNavigation(false);
	StepMeshISM->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		StepMeshISM->SetStaticMesh(CubeMesh.Object);
	}

	// 쐐기 ProcMesh(F3.8) — 계단 ISM과 동일 충돌 프로파일 미러(걷기 Pawn Block + 커서 Visibility
	// Block + Camera Ignore). 충돌은 Convex 단순 충돌(쐐기 = 볼록체, complex-as-simple 비사용) —
	// 경사면 걷기 가부는 CharacterMovement WalkableFloorAngle(기본 ≈44.77°)이 자연 판정: 행당
	// 100uu(45°) 급경사는 못 걷고(F3.7' 보행 불가 경고와 동근거), 완경사는 매끈하게 걸어 오름.
	WedgeMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WedgeMesh"));
	WedgeMesh->SetupAttachment(RootComponent);
	WedgeMesh->bUseComplexAsSimpleCollision = false;
	WedgeMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	WedgeMesh->SetCollisionObjectType(ECC_WorldStatic);
	WedgeMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	WedgeMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	WedgeMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	WedgeMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	WedgeMesh->SetCanEverAffectNavigation(false);
	WedgeMesh->SetCastShadow(false);
}

namespace
{
	// 자동 맞춤 한 방향 이웃 스캔(F3.6-1): Cursor에서 축 ±방향으로 1..MaxScan 거리의 폭 Width 라인을
	// 지배 단 조회(OJJ_GetDominantFoundationSurfaceZInRect — 단 격자 위만 후보, 비격자 램프 중간 행
	// 자동 제외). 첫 성공 라인의 거리/SurfaceZ 반환. 비격자 라인을 건너뛴 풋프린트가 그 위를 덮는
	// 경우는 CanPlaceFoundation 겹침 거부가 막는다(빨강).
	bool OJJ_ScanNeighborLine(const AOJJ_Grid& Grid, FIntPoint Cursor, bool bAxisX, int32 Sign,
		int32 LateralOrigin, int32 Width, int32 MaxScan, float SnapGridOriginZ,
		int32& OutDistance, float& OutSurfaceZ)
	{
		for (int32 Distance = 1; Distance <= MaxScan; ++Distance)
		{
			const FIntPoint LineOrigin = bAxisX
				? FIntPoint(Cursor.X + Sign * Distance, LateralOrigin)
				: FIntPoint(LateralOrigin, Cursor.Y + Sign * Distance);
			const FIntPoint LineSize = bAxisX ? FIntPoint(1, Width) : FIntPoint(Width, 1);
			float LineZ = 0.0f;
			int32 ContactCells = 0;
			if (Grid.OJJ_GetDominantFoundationSurfaceZInRect(LineOrigin, LineSize, SnapGridOriginZ,
				LineZ, ContactCells))
			{
				OutDistance = Distance;
				OutSurfaceZ = LineZ;
				return true;
			}
		}
		return false;
	}
}

FOJJFoundationFitResult AOJJ_RampFoundation::OJJ_ComputeHoverFootprint(const AOJJ_Grid& Grid,
	FIntPoint CursorCell, int32 RotationSteps) const
{
	// 축 = R키(입력 step parity — ㊁: 자동 맞춤에서 R은 축만, 부호는 이웃이 결정).
	// 폭 W = CDO FoundationSize.Y 고정(㊀), 측면 정렬 = 베이스 origin 공통 수식의 수직 성분과 동일.
	const int32 Step = ((RotationSteps % 4) + 4) % 4;
	const bool bAxisX = (Step % 2 == 0);
	const int32 Width = FMath::Max(1, FoundationSize.Y);
	const int32 LateralOrigin = (bAxisX ? CursorCell.Y : CursorCell.X) - (Width - 1) / 2;

	// 양방향 스캔(㉾ MaxAutoFitScanCells 한계). 단 격자 원점은 Thickness를 아는 클래스가 산출(F1-b 계약).
	const float SnapGridOriginZ = Grid.GetActorLocation().Z + Thickness;
	const int32 MaxScan = FMath::Max(1, MaxAutoFitScanCells);
	int32 DistNeg = 0, DistPos = 0;
	float ZNeg = 0.0f, ZPos = 0.0f;
	const bool bNeg = OJJ_ScanNeighborLine(Grid, CursorCell, bAxisX, -1, LateralOrigin, Width,
		MaxScan, SnapGridOriginZ, DistNeg, ZNeg);
	const bool bPos = OJJ_ScanNeighborLine(Grid, CursorCell, bAxisX, +1, LateralOrigin, Width,
		MaxScan, SnapGridOriginZ, DistPos, ZPos);

	// ② 한쪽/무이웃 → 고정 램프 폴백(계획 §4): 베이스 풋프린트(CDO 크기·R이 축+부호 전부 결정),
	// Z는 기존 F3.5 ③ 엣지 스냅/씨앗 폴백 그대로(OJJ_ComputeSnapLift 무변경). 고립 램프 용도 보존(㉹).
	if (!bNeg || !bPos)
	{
		FOJJFoundationFitResult Result = Super::OJJ_ComputeHoverFootprint(Grid, CursorCell, RotationSteps);
		Result.DirectionSource = TEXT("방향 수동(R)");
		return Result;
	}

	// ① 자동 맞춤(계획 §1): 풋프린트 = 두 이웃 라인 사이 틈 전체(D칸 × W — 커서는 틈/측면 선택만).
	FOJJFoundationFitResult Result;
	Result.bAutoFit = true;
	const int32 GapLength = DistNeg + DistPos - 1;
	Result.EffSize = bAxisX ? FIntPoint(GapLength, Width) : FIntPoint(Width, GapLength);
	Result.Origin = bAxisX
		? FIntPoint(CursorCell.X - (DistNeg - 1), LateralOrigin)
		: FIntPoint(LateralOrigin, CursorCell.Y - (DistNeg - 1));

	// ΔZ = k단: 두 이웃 모두 단 격자 위(스캔 게이트)라 차이는 정확히 100의 정수배 — Round는 float 보정.
	// 부호(낮→높)는 이웃이 자동 결정(㊁): 축 X — 서쪽이 낮으면 +X(step 0)/동쪽이 낮으면 −X(step 2),
	// 축 Y — 남쪽이 낮으면 +Y(step 1)/북쪽이 낮으면 −Y(step 3). k=0은 평지 브리지(㉿ — 부호 무의미).
	const bool bNegIsLow = ZNeg <= ZPos;
	Result.RiseSteps = FMath::RoundToInt(FMath::Abs(ZPos - ZNeg) / AOJJ_Grid::OJJ_FoundationSnapStep);
	Result.EffectiveRotationSteps = bAxisX ? (bNegIsLow ? 0 : 2) : (bNegIsLow ? 1 : 3);

	// 행간 계단 검증(F3.7' 개정): 한계 = MaxRampStepPerRow 프로퍼티(보행 기준 45 고정에서 완화 —
	// 짧은 틈 급경사 수용). 틈의 D−1 구간이 k×100을 분담, 미달이면 빨강 + 최소 칸수 사유(㊂).
	// Δ1단 1칸은 산식상 불능(단일 행이 양 끝 동시 정합 불가) — RequiredIntervals ≥ 1이 자동 거부.
	const float StepLimit = FMath::Max(1.0f, MaxRampStepPerRow);
	if (Result.RiseSteps >= 1)
	{
		const int32 RequiredIntervals = FMath::CeilToInt(
			Result.RiseSteps * AOJJ_Grid::OJJ_FoundationSnapStep / StepLimit - KINDA_SMALL_NUMBER);
		if (GapLength - 1 < RequiredIntervals)
		{
			Result.bValid = false;
			Result.FailReason = FString::Printf(TEXT("틈 %d칸 < 최소 %d칸(Δ%d단, 행간 계단 ≤ %.0fuu)"),
				GapLength, RequiredIntervals + 1, Result.RiseSteps, StepLimit);
		}
	}

	// 방향 출처(㊁) + 보행 가능 여부(F3.7' 개정 — 거부 대신 경고: 플레이어가 알고 깔게).
	if (Result.RiseSteps >= 1 && GapLength >= 2)
	{
		const float StepPerRow =
			Result.RiseSteps * AOJJ_Grid::OJJ_FoundationSnapStep / (float)(GapLength - 1);
		Result.DirectionSource = FString::Printf(
			TEXT("방향 자동(이웃 낮→높) — 틈 %d칸, Δ%d단, 행당 %.0fuu (%s)"),
			GapLength, Result.RiseSteps, StepPerRow,
			StepPerRow <= OJJ_WalkableStepPerRow + KINDA_SMALL_NUMBER
				? TEXT("보행 가능")
				: TEXT("보행 불가 — 컨베이어 전용"));
	}
	else
	{
		Result.DirectionSource = FString::Printf(TEXT("방향 자동(이웃 낮→높) — 틈 %d칸, Δ%d단"),
			GapLength, Result.RiseSteps);
	}
	return Result;
}

void AOJJ_RampFoundation::OJJ_NotifyFitResult(const FOJJFoundationFitResult& Fit)
{
	// 비주얼용 확정값(시각=데이터 단일원): 오르는 방향 길이 = EffSize의 유효 step 축(등록 산식과 동일
	// parity 규칙). 고정 램프 폴백도 같은 경로로 들어와(길이 = FoundationSize.X, 1단) 회귀 0.
	const int32 Step = ((Fit.EffectiveRotationSteps % 4) + 4) % 4;
	PlacedClimbLengthCells = FMath::Max(1, (Step % 2 == 0) ? Fit.EffSize.X : Fit.EffSize.Y);
	PlacedRiseSteps = FMath::Max(0, Fit.RiseSteps);
	PlacedRotationSteps = Step; // F3.8' — 쐐기 월드 방향 규약 + 액터 yaw 역회전용.
}

bool AOJJ_RampFoundation::OJJ_BuildPerCellSurfaceZ(FIntPoint EffSize, int32 RotationSteps, float BaseSurfaceZ,
	int32 RiseSteps, TArray<float>& OutCellZs) const
{
	// R = 오르는 방향(로컬 +X) 길이 = EffSize의 해당 월드 축(step 0/2 → X, 1/3 → Y — F3-0 스왑 규칙).
	// F3.6-0 일반화: 고정 램프에선 FoundationSize.X와 동치(스왑이 정확히 상쇄 — 동작 비트 동일)이고,
	// 동적 풋프린트(F3.6-1 자동 맞춤)에선 틈 길이 D를 그대로 따른다. r 산출은 월드 축 기준 switch.
	const int32 Step = ((RotationSteps % 4) + 4) % 4;
	const int32 R = (Step % 2 == 0) ? EffSize.X : EffSize.Y;
	if (R < 2 || RiseSteps < 1 || EffSize.X < 1 || EffSize.Y < 1)
	{
		// 1행 램프는 경사 불능, RiseSteps<1은 평지 퇴화(㉿) — 평판 단일값 경로 폴백.
		return false;
	}

	OutCellZs.SetNumUninitialized(EffSize.X * EffSize.Y);
	for (int32 LX = 0; LX < EffSize.X; ++LX)
	{
		for (int32 LY = 0; LY < EffSize.Y; ++LY)
		{
			// 풋프린트 로컬(월드 축) → 오르는 방향 행 r — f3 계획 §보강 산식.
			int32 r = 0;
			switch (Step)
			{
			case 0: r = LX; break;                       // 로컬 +X가 월드 +X
			case 1: r = LY; break;                       // 월드 +Y
			case 2: r = (EffSize.X - 1) - LX; break;     // 월드 −X
			case 3: r = (EffSize.Y - 1) - LY; break;     // 월드 −Y
			}
			// 양 끝 정합(턱 0): r=0 → BaseSurfaceZ 정확, r=R−1 → +RiseSteps×100 정확(float 오차 0 —
			// (R−1)/(R−1)=1). RiseSteps=1이면 F3-2 산식과 비트 동일.
			OutCellZs[LX * EffSize.Y + LY] =
				BaseSurfaceZ + ((float)r / (float)(R - 1)) * RiseSteps * AOJJ_Grid::OJJ_FoundationSnapStep;
		}
	}
	return true;
}

float AOJJ_RampFoundation::OJJ_ComputeSnapLift(const AOJJ_Grid& Grid, FIntPoint Origin, FIntPoint EffSize,
	int32 RotationSteps, FString* OutHeightSource) const
{
	// 낮은 끝(r=0) 기준 좌표: 행(풋프린트 안 — 씨앗 폴백용)과 바깥 인접 라인(엣지 스냅용)을 함께 산출.
	const int32 Step = ((RotationSteps % 4) + 4) % 4;
	FIntPoint RowOrigin = Origin;   // 낮은 끝 행(풋프린트 안)
	FIntPoint RowSize = EffSize;
	FIntPoint LineOrigin = Origin;  // 낮은 끝 바깥 인접 라인(이웃 판)
	FIntPoint LineSize = EffSize;
	switch (Step)
	{
	case 0: // 낮은 끝 = 서쪽 변
		RowSize = FIntPoint(1, EffSize.Y);
		LineOrigin.X = Origin.X - 1;             LineSize = FIntPoint(1, EffSize.Y);
		break;
	case 1: // 남쪽 변
		RowSize = FIntPoint(EffSize.X, 1);
		LineOrigin.Y = Origin.Y - 1;             LineSize = FIntPoint(EffSize.X, 1);
		break;
	case 2: // 동쪽 변
		RowOrigin.X = Origin.X + EffSize.X - 1;  RowSize = FIntPoint(1, EffSize.Y);
		LineOrigin.X = Origin.X + EffSize.X;     LineSize = FIntPoint(1, EffSize.Y);
		break;
	case 3: // 북쪽 변
		RowOrigin.Y = Origin.Y + EffSize.Y - 1;  RowSize = FIntPoint(EffSize.X, 1);
		LineOrigin.Y = Origin.Y + EffSize.Y;     LineSize = FIntPoint(EffSize.X, 1);
		break;
	}

	// F3.5 ③: 이웃 엣지 스냅 — 낮은 끝이 이웃 판과 같은 단(턱 0), 높은 끝은 +100이라 높은 쪽
	// 이웃과도 자동 일치(단 간격 고정). stale 이웃은 그리드 조회의 weak 검증이 거름.
	const float SnapGridOriginZ = Grid.GetActorLocation().Z + Thickness;
	float NeighborZ = 0.0f;
	int32 ContactCells = 0;
	if (Grid.OJJ_GetDominantFoundationSurfaceZInRect(LineOrigin, LineSize, SnapGridOriginZ, NeighborZ, ContactCells))
	{
		if (OutHeightSource)
		{
			*OutHeightSource = FString::Printf(TEXT("상속-엣지(이웃 접촉 %d셀)"), ContactCells);
		}
		return NeighborZ - Thickness - Grid.GetActorLocation().Z;
	}

	// 씨앗 폴백(고립 램프 = 지형 오르기): 낮은 끝 행 지형 스냅(F3-2 Codex ② — 풋프린트 전체 max를
	// 쓰면 단차 지형에서 낮은 끝이 떠 턱 0이 깨짐). 중간 행 지형 묻힘 가능성은 PIE 관찰 항목.
	if (OutHeightSource)
	{
		*OutHeightSource = TEXT("씨앗(지형, 낮은 끝 행)");
	}
	return Grid.OJJ_ComputeFoundationSnapLift(RowOrigin, RowSize, Thickness);
}

void AOJJ_RampFoundation::UpdateSlabVisual()
{
	// 베이스 슬래브(단일 박스)는 램프 형상과 안 맞음 — 숨기고 충돌도 끔(쐐기/계단이 전담).
	if (SlabMesh)
	{
		SlabMesh->SetVisibility(false);
		SlabMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	const float CellSize = OJJ_ResolveCellSize();
	// F3.6-1: 배치 확정값 우선(자동 맞춤 동적 길이/단수 — OJJ_NotifyFitResult가 저장),
	// 미확정(에디터 프리뷰/스폰 직후 OnConstruction)은 CDO 고정 램프 규격(기존과 동일).
	const int32 R = PlacedClimbLengthCells > 0 ? PlacedClimbLengthCells : FMath::Max(1, FoundationSize.X);
	const int32 Rise = PlacedClimbLengthCells > 0 ? PlacedRiseSteps : 1;
	const int32 Cols = FMath::Max(1, FoundationSize.Y);
	const float SlabThickness = FMath::Max(1.0f, Thickness);

	// F3.8: 쐐기 우선 — 성공 시 계단 ISM은 비움(인스턴스 0 = 충돌 셰이프 0, 토글 불필요).
	// F3.8': 확정 step 전달(미확정 에디터 프리뷰 0 — 역회전 항등, 기존 프리뷰와 동일).
	const int32 Step = PlacedClimbLengthCells > 0 ? PlacedRotationSteps : 0;
	if (OJJ_BuildWedgeVisual(R, Cols, Rise, CellSize, Step))
	{
		if (StepMeshISM)
		{
			StepMeshISM->ClearInstances();
		}
		return;
	}

	// 폴백: 기존 계단 박스(쐐기 실패). 쐐기 잔여 정리. Rise 0(평지 브리지)은 의도된 퇴화라 무경고 —
	// 경고는 비정상 실패(컴포넌트 미생성 등)에만.
	if (WedgeMesh)
	{
		WedgeMesh->ClearAllMeshSections();
		WedgeMesh->ClearCollisionConvexMeshes();
	}
	if (Rise >= 1)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[RampFoundation] 쐐기 생성 불가(R=%d, Rise=%d단) — 계단 박스 폴백"), R, Rise);
	}
	if (!StepMeshISM)
	{
		return;
	}

	// 로컬 공간 계단: 액터 원점 = 풋프린트 중심, 액터 Z + Thickness = Z_low(낮은 단 상면).
	// 행 r 박스 상면(로컬) = Thickness + (r/(R−1))×Rise×100 — 등록 SurfaceZ와 동일 산식(시각=데이터
	// 단일원, Rise=0 평지 브리지는 전 행 동일 = 평판 등록과 일치).
	// 두께는 Thickness 고정(행 사이 측면 갭은 F2 결정 ⑥ 프로토 수용과 동일 정책).
	StepMeshISM->ClearInstances();
	for (int32 r = 0; r < R; ++r)
	{
		const float TopLocalZ = SlabThickness
			+ (R > 1 ? ((float)r / (float)(R - 1)) * Rise * AOJJ_Grid::OJJ_FoundationSnapStep : 0.0f);
		const FVector StepLocation(
			(r + 0.5f) * CellSize - R * CellSize * 0.5f, 0.0f, TopLocalZ - SlabThickness * 0.5f);
		const FVector StepScale(CellSize / 100.0f, Cols * CellSize / 100.0f, SlabThickness / 100.0f);
		StepMeshISM->AddInstance(FTransform(FRotator::ZeroRotator, StepLocation, StepScale));
	}
}

bool AOJJ_RampFoundation::OJJ_BuildWedgeVisual(int32 ClimbCells, int32 WidthCells, int32 RiseSteps, float CellSize,
	int32 RotationSteps)
{
	if (!WedgeMesh || RiseSteps < 1 || ClimbCells < 1 || WidthCells < 1 || CellSize <= KINDA_SMALL_NUMBER)
	{
		return false; // Rise 0(평지 브리지)은 의도된 퇴화 — 호출자가 계단(평탄 박스) 폴백.
	}

	// F3.8' 방향 규약(수직 어긋남 교정): 기하를 등록 산식(OJJ_BuildPerCellSurfaceZ r-switch)과 같은
	// **월드 축** 오르막 방향(0:+X/1:+Y/2:−X/3:−Y)으로 만들고, 액터에 적용될 yaw(+90°×step)의 정확한
	// 역회전(−90°×step)을 정점·노멀에 선적용한다. R(−θ)=R(θ)⁻¹이라 합성이 항등 — 최종 월드 기하가
	// yaw 회전 규약 가정과 무관하게 항상 등록 데이터의 방향과 일치(시각=데이터 단일원).
	// step 0은 역회전 항등 → 이전 구현과 비트 동일(회귀 기준점).
	// 피벗 = 풋프린트 중심, 액터 Z + Thickness = Z_low. 양 끝 정합 — 칼끝(낮은 끝, Z=T) = 아랫단
	// 상면 엣지, 수직 끝면 상단(Z=T+Rise) = 윗단 상면. 빗변은 연속 경사 — 등록 데이터는 셀당
	// 계단(㉰)이라 셀 중심 기준 최대 반행 시각 편차는 비주얼 근사로 수용.
	const int32 Step = ((RotationSteps % 4) + 4) % 4;
	FVector ClimbDir(1.0f, 0.0f, 0.0f); // 월드 오르막(낮은 끝 → 높은 끝) — 등록 r-switch와 동일 규약.
	switch (Step)
	{
	case 0: ClimbDir = FVector(1.0f, 0.0f, 0.0f); break;
	case 1: ClimbDir = FVector(0.0f, 1.0f, 0.0f); break;
	case 2: ClimbDir = FVector(-1.0f, 0.0f, 0.0f); break;
	case 3: ClimbDir = FVector(0.0f, -1.0f, 0.0f); break;
	}
	// 폭 축 = Up×ClimbDir(step별 +Y/−X/−Y/+X): 위치는 ±Side 대칭이라 부호 무관이지만 **와인딩은
	// 라벨 순서에 의존** — ClimbDir×SideDir=+Z가 전 step에서 유지돼야 면 조립(K0=−Side, K1=+Side)의
	// 전면 방향이 보존된다(패리티만 쓰면 step 1·2에서 반전 — Codex F3.8' ④ BUG 수정).
	const FVector SideDir = FVector::CrossProduct(FVector::UpVector, ClimbDir);

	const float L = ClimbCells * CellSize;
	const float W = WidthCells * CellSize;
	const float T = FMath::Max(1.0f, Thickness);
	const float Rise = RiseSteps * AOJJ_Grid::OJJ_FoundationSnapStep;

	// 액터 yaw의 정확한 역회전 — 정점/노멀에 선적용(위 규약 주석).
	const FQuat InvActorYaw(FRotator(0.0f, -90.0f * Step, 0.0f));
	const FVector Up(0.0f, 0.0f, 1.0f);
	const FVector KnifeCenter = -ClimbDir * (L * 0.5f) + Up * T;        // 낮은 끝 경계(아랫단 상면)
	const FVector HighCenter = ClimbDir * (L * 0.5f) + Up * T;          // 높은 끝 경계(바닥 높이)
	const FVector Side = SideDir * (W * 0.5f);

	// 기하 꼭짓점 6(월드 프레임 → 역회전) — K=칼끝(낮은 끝), B=높은 끝 바닥, U=높은 끝 상단
	// (0=−Side, 1=+Side — 좌/우 역할은 면 와인딩 순서로만 쓰여 회전해도 보존).
	const FVector K0 = InvActorYaw.RotateVector(KnifeCenter - Side);
	const FVector K1 = InvActorYaw.RotateVector(KnifeCenter + Side);
	const FVector B0 = InvActorYaw.RotateVector(HighCenter - Side);
	const FVector B1 = InvActorYaw.RotateVector(HighCenter + Side);
	const FVector U0 = InvActorYaw.RotateVector(HighCenter - Side + Up * Rise);
	const FVector U1 = InvActorYaw.RotateVector(HighCenter + Side + Up * Rise);

	// 렌더 정점은 면별 복제(공유하면 면 노멀이 섞임): 쿼드 3 + 삼각 2 = 18정점/8삼각형.
	// 와인딩 = 바깥에서 봤을 때 시계방향(UE 전면 규약). AddQuad(A,B,C,D) → (A,B,C),(C,B,D).
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	Vertices.Reserve(18); Triangles.Reserve(24); Normals.Reserve(18); UVs.Reserve(18);

	auto AddQuad = [&](const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& N)
	{
		const int32 Base = Vertices.Num();
		Vertices.Append({ A, B, C, D });
		Normals.Append({ N, N, N, N });
		UVs.Append({ FVector2D(0, 0), FVector2D(0, 1), FVector2D(1, 0), FVector2D(1, 1) });
		Triangles.Append({ Base + 0, Base + 1, Base + 2, Base + 2, Base + 1, Base + 3 });
	};
	auto AddTri = [&](const FVector& A, const FVector& B, const FVector& C, const FVector& N)
	{
		const int32 Base = Vertices.Num();
		Vertices.Append({ A, B, C });
		Normals.Append({ N, N, N });
		UVs.Append({ FVector2D(0, 0), FVector2D(0, 1), FVector2D(1, 1) });
		Triangles.Append({ Base + 0, Base + 1, Base + 2 });
	};

	// 면 노멀: 월드 프레임에서 산출 후 정점과 동일한 역회전 — 빗변은 경사 방향 d=ClimbDir·L+Up·Rise와
	// 수직(내적 −Rise·L + L·Rise = 0), 위쪽(+Z 성분). 면 와인딩은 K/B/U 역할 기준이라 회전 불변.
	const FVector SlopeNormal = InvActorYaw.RotateVector((Up * L - ClimbDir * Rise).GetSafeNormal());
	const FVector BottomNormal = InvActorYaw.RotateVector(-Up);
	const FVector EndNormal = InvActorYaw.RotateVector(ClimbDir);
	const FVector SidePosNormal = InvActorYaw.RotateVector(SideDir);
	const FVector SideNegNormal = InvActorYaw.RotateVector(-SideDir);
	AddQuad(K0, K1, U0, U1, SlopeNormal);      // 빗변 상면
	AddQuad(K0, B0, K1, B1, BottomNormal);     // 바닥(Z=T 수평면, 아래 향함)
	AddQuad(B0, U0, B1, U1, EndNormal);        // 수직 끝면(높은 끝 = 윗단 옆면)
	AddTri(K1, B1, U1, SidePosNormal);         // 옆면 +Side
	AddTri(K0, U0, B0, SideNegNormal);         // 옆면 −Side

	WedgeMesh->ClearAllMeshSections();
	WedgeMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs,
		TArray<FLinearColor>(), TArray<FProcMeshTangent>(), /*bCreateCollision=*/false);

	// 충돌 = Convex 단순 충돌(쐐기는 볼록체) — 기하 꼭짓점 6개로 hull 생성. complex-as-simple
	// 비사용(생성자 설정)이라 걷기/스윕이 이 convex를 탄다.
	WedgeMesh->ClearCollisionConvexMeshes();
	WedgeMesh->AddCollisionConvexMesh({ K0, K1, B0, B1, U0, U1 });

	// 머티리얼: 기존 슬래브 지정 그대로 미러(F1-b 임시 비주얼 정책 유지).
	if (SlabMesh)
	{
		WedgeMesh->SetMaterial(0, SlabMesh->GetMaterial(0));
	}
	return true;
}
