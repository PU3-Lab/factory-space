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
	bool bAxisX = (Step % 2 == 0);
	int32 Width = FMath::Max(1, FoundationSize.Y);
	int32 LateralOrigin = (bAxisX ? CursorCell.Y : CursorCell.X) - (Width - 1) / 2;

	// 양방향 스캔(㉾ MaxAutoFitScanCells 한계). 단 격자 원점은 Thickness를 아는 클래스가 산출(F1-b 계약).
	const float SnapGridOriginZ = Grid.GetActorLocation().Z + Thickness;
	const int32 MaxScan = FMath::Max(1, MaxAutoFitScanCells);
	int32 DistNeg = 0, DistPos = 0;
	float ZNeg = 0.0f, ZPos = 0.0f;
	bool bNeg = OJJ_ScanNeighborLine(Grid, CursorCell, bAxisX, -1, LateralOrigin, Width,
		MaxScan, SnapGridOriginZ, DistNeg, ZNeg);
	bool bPos = OJJ_ScanNeighborLine(Grid, CursorCell, bAxisX, +1, LateralOrigin, Width,
		MaxScan, SnapGridOriginZ, DistPos, ZPos);

	// #261 한쪽-지면 하강 램프: 한쪽 분기에서 합성하면 true로 올려, fall-through 후 공유 블록이 만드는
	// Result에 bOneSideGroundRamp 표식을 단다(OJJ_ComputeSnapLift 높은끝 스냅 게이트).
	bool bResultOneSideGroundRamp = false;

	// #261 수직 엣지 보정: 스캔 축(bAxisX)이 R키로만 정해져 실제 Foundation이 직교 엣지에 있으면
	// bNeg=bPos=0이 되어 한쪽 분기에 진입조차 못 한다(스캔 축과 Foundation 접촉 변이 어긋난 경우).
	// 양방향(bNeg&&bPos) 자동 맞춤은 그대로 두고(게이트), 그 외에는 커서 중심 CDO 풋프린트 둘레
	// 4변을 직접 탐침해 정확히 한 변에만 Foundation이 닿으면 그 축/부호로 전환 후 재스캔한다.
	// 코너/모호(0개 또는 ≥2개)면 아무것도 바꾸지 않고 기존 고정 폴백/양방향 로직에 맡긴다(Codex #2).
	if (!(bNeg && bPos))
	{
		const FIntPoint ProbeSize = FoundationSize;
		const FIntPoint ProbeOrigin = AOJJ_Grid::OJJ_OriginFromCursorCellForSize(CursorCell, ProbeSize);

		float WZ = 0.0f, EZ = 0.0f, SZ = 0.0f, NZ = 0.0f;
		int32 WC = 0, EC = 0, SC = 0, NC = 0;
		const bool hasW = Grid.OJJ_GetDominantFoundationSurfaceZInRect(
			FIntPoint(ProbeOrigin.X - 1, ProbeOrigin.Y), FIntPoint(1, ProbeSize.Y), SnapGridOriginZ, WZ, WC) && WC > 0;
		const bool hasE = Grid.OJJ_GetDominantFoundationSurfaceZInRect(
			FIntPoint(ProbeOrigin.X + ProbeSize.X, ProbeOrigin.Y), FIntPoint(1, ProbeSize.Y), SnapGridOriginZ, EZ, EC) && EC > 0;
		const bool hasS = Grid.OJJ_GetDominantFoundationSurfaceZInRect(
			FIntPoint(ProbeOrigin.X, ProbeOrigin.Y - 1), FIntPoint(ProbeSize.X, 1), SnapGridOriginZ, SZ, SC) && SC > 0;
		const bool hasN = Grid.OJJ_GetDominantFoundationSurfaceZInRect(
			FIntPoint(ProbeOrigin.X, ProbeOrigin.Y + ProbeSize.Y), FIntPoint(ProbeSize.X, 1), SnapGridOriginZ, NZ, NC) && NC > 0;

		const int32 EdgeCount = (hasW ? 1 : 0) + (hasE ? 1 : 0) + (hasS ? 1 : 0) + (hasN ? 1 : 0);

		// 정확히 한 변에만 Foundation → 그 변의 축/부호로 전환 + 재스캔(Codex #2/#3 일관성).
		if (EdgeCount == 1)
		{
			bool bRescan = false;
			if (hasW)        // 서쪽(−X)
			{
				bAxisX = true;
				Width = FMath::Max(1, FoundationSize.Y);
				LateralOrigin = CursorCell.Y - (Width - 1) / 2;
				bRescan = OJJ_ScanNeighborLine(Grid, CursorCell, bAxisX, -1, LateralOrigin, Width,
					MaxScan, SnapGridOriginZ, DistNeg, ZNeg);
				if (bRescan) { bNeg = true; bPos = false; DistPos = 0; ZPos = 0.0f; }
			}
			else if (hasE)   // 동쪽(+X)
			{
				bAxisX = true;
				Width = FMath::Max(1, FoundationSize.Y);
				LateralOrigin = CursorCell.Y - (Width - 1) / 2;
				bRescan = OJJ_ScanNeighborLine(Grid, CursorCell, bAxisX, +1, LateralOrigin, Width,
					MaxScan, SnapGridOriginZ, DistPos, ZPos);
				if (bRescan) { bPos = true; bNeg = false; DistNeg = 0; ZNeg = 0.0f; }
			}
			else if (hasS)   // 남쪽(−Y)
			{
				bAxisX = false;
				Width = FMath::Max(1, FoundationSize.Y);
				LateralOrigin = CursorCell.X - (Width - 1) / 2;
				bRescan = OJJ_ScanNeighborLine(Grid, CursorCell, bAxisX, -1, LateralOrigin, Width,
					MaxScan, SnapGridOriginZ, DistNeg, ZNeg);
				if (bRescan) { bNeg = true; bPos = false; DistPos = 0; ZPos = 0.0f; }
			}
			else             // 북쪽(+Y)
			{
				bAxisX = false;
				Width = FMath::Max(1, FoundationSize.Y);
				LateralOrigin = CursorCell.X - (Width - 1) / 2;
				bRescan = OJJ_ScanNeighborLine(Grid, CursorCell, bAxisX, +1, LateralOrigin, Width,
					MaxScan, SnapGridOriginZ, DistPos, ZPos);
				if (bRescan) { bPos = true; bNeg = false; DistNeg = 0; ZNeg = 0.0f; }
			}
		}
	}

	// ② 무이웃(양쪽 다 없음) → 고정 램프 폴백(계획 §4): 베이스 풋프린트(CDO 크기·R이 축+부호 전부 결정),
	// Z는 기존 F3.5 ③ 엣지 스냅/씨앗 폴백 그대로(OJJ_ComputeSnapLift 무변경). 고립 램프 용도 보존(㉹).
	if (!bNeg && !bPos)
	{
		FOJJFoundationFitResult Result = Super::OJJ_ComputeHoverFootprint(Grid, CursorCell, RotationSteps);
		Result.DirectionSource = TEXT("방향 수동(R)");
		return Result;
	}

	// #261 한쪽 Foundation → 맨땅(지형) 하강 램프: 한쪽만 Foundation이 잡혔으면(bNeg != bPos)
	// 반대편(맨땅) 라인의 지형 Z를 합성해 두-이웃 자동 맞춤 산식(①, 110行+)을 그대로 재사용한다.
	//   · 지형 Z는 OJJ_ComputeFoundationSnapLift(올림-스냅 = SnapSteps×100, 격자 정합)로 산출 →
	//     Foundation SurfaceZ와 동일한 100uu 단 격자에 떨어지므로 ΔZ가 정확히 100의 정수배(①의 전제 유지).
	//   · 결정 B: 지형이 Foundation 윗면보다 낮지 않으면(하강 케이스 아님) 고정 폴백(Super) — 범위 외.
	//   · GroundZ 무효(베이크 전) → 고정 폴백(Super). 평면 폴백 없음(#249 패턴).
	//   · 합성 후 return 하지 않고 ①로 fall-through → GapLength/EffSize/Origin/RiseSteps(=실 ΔZ/스냅)/
	//     방향(ZNeg<=ZPos)/급경사 게이트/DirectionSource 전부 두-이웃 경로와 동일하게 재사용.
	//   · 등록은 풋프린트(틈) 한정이라 램프 셀만 Foundation이 되고 맨땅은 그대로 남는다.
	if (bNeg != bPos)
	{
		// (c) 누락(맨땅)측 지형 기준선 = 커서 한 칸 바깥의 W폭 라인(OJJ_ScanNeighborLine 미러).
		//     bNeg(Foundation이 −쪽) → 맨땅은 +쪽(CursorCell+1), bPos → 맨땅은 −쪽(CursorCell−1).
		const FIntPoint GroundLineOrigin = bNeg
			? (bAxisX ? FIntPoint(CursorCell.X + 1, LateralOrigin) : FIntPoint(LateralOrigin, CursorCell.Y + 1))
			: (bAxisX ? FIntPoint(CursorCell.X - 1, LateralOrigin) : FIntPoint(LateralOrigin, CursorCell.Y - 1));

		// (a) GroundZ 무효(미베이크/off-grid) → 고정 폴백(평면 폴백 없음 — #249 패턴). OJJ_GetRawTerrainSurfaceZ가
		//     베이크 유효성 + in-grid를 함께 검사해 무효면 false(public 단일원 — OJJ_HasValidGroundZData는 private).
		float GroundRawZ = 0.0f;
		if (!Grid.OJJ_GetRawTerrainSurfaceZ(GroundLineOrigin, GroundRawZ))
		{
			FOJJFoundationFitResult Result = Super::OJJ_ComputeHoverFootprint(Grid, CursorCell, RotationSteps);
			Result.DirectionSource = TEXT("방향 수동(R)");
			return Result;
		}

		// (d) 결정 B(#261 분기): 실제 하강 폭 = Foundation 윗면 − 실제 지면 GroundRawZ.
		//     "지면+Thickness"(슬래브 윗면 가정)로 산정하면 평지 슬래브에서 FoundationZ와 상쇄돼 ΔZ=0 오판
		//     (Codex #5 Thickness 결합) — 하강폭/flush/단수는 raw 지면(GroundRawZ) 기준으로 산정한다.
		const float FoundationZ = bNeg ? ZNeg : ZPos;
		const float DeltaReal = FoundationZ - GroundRawZ;

		// (d-1) flush(Foundation 윗면 == 실제 지면, ΔZ≈0): 램프 불필요 → INVALID 반환(수동 고정 램프 합성 안 함).
		//        배치는 OJJ_BuildController.cpp:1295-1298에서 bValid=false를 거부 → 진짜 스킵.
		if (DeltaReal <= KINDA_SMALL_NUMBER && DeltaReal >= -KINDA_SMALL_NUMBER)
		{
			FOJJFoundationFitResult Result = Super::OJJ_ComputeHoverFootprint(Grid, CursorCell, RotationSteps);
			Result.bValid = false;
			Result.FailReason = TEXT("Foundation flush with ground — no ramp needed");
			Result.DirectionSource = TEXT("방향 수동(R)");
			return Result;
		}
		// (d-2) 지면이 Foundation 윗면보다 높음(DeltaReal < −eps): 하강 케이스 아님 → 고정 폴백(범위 외).
		if (DeltaReal < -KINDA_SMALL_NUMBER)
		{
			FOJJFoundationFitResult Result = Super::OJJ_ComputeHoverFootprint(Grid, CursorCell, RotationSteps);
			Result.DirectionSource = TEXT("방향 수동(R)");
			return Result;
		}

		// (d-3) 하강(DeltaReal > eps): RiseSteps = ceil(ΔZ/100), 최소 1 (round 아님 — 낮은끝이 지면에 닿거나
		//        아래로 파고들어 float gap 없음, Codex #2). 합성 끝점 = FoundationZ − RiseSteps×100 (양자화) →
		//        공유 블록 round(|ZPos−ZNeg|/100)이 정확히 RiseSteps. PART D(OJJ_ComputeSnapLift 높은끝 스냅)가
		//        base = FoundationZ − RiseSteps×100을 잡아 HIGH end == FoundationZ 보장, 낮은끝은 지면 아래(수용).
		const int32 OneSideRiseSteps = FMath::Max(1, FMath::CeilToInt(
			DeltaReal / AOJJ_Grid::OJJ_FoundationSnapStep - KINDA_SMALL_NUMBER));
		const float SynthGroundZ = FoundationZ - OneSideRiseSteps * AOJJ_Grid::OJJ_FoundationSnapStep;

		// (e) 보행(완경사) 보장에 필요한 램프 길이 산출(단수 = OneSideRiseSteps).
		const float WalkLimit = FMath::Max(1.0f, OJJ_WalkableStepPerRow);
		const int32 WalkGap = FMath::CeilToInt(
			OneSideRiseSteps * AOJJ_Grid::OJJ_FoundationSnapStep / WalkLimit) + 1;
		const int32 DistFound = bNeg ? DistNeg : DistPos;
		// [#7 커서거리 제거] 길이는 커서거리(DistFound)와 무관하게 보행 슬로프로 지면까지 = (WalkGap-1)칸 고정.
		// GapLength = DistFound + DistGround - 1 인데, DistGround = WalkGap - DistFound 면 DistFound가 상쇄돼
		// GapLength = WalkGap-1 (커서 무관). Origin = CursorCell-(DistNeg-1) 은 DistFound가 파운데이션을 추적하므로
		// 이미 파운데이션 변에 앵커됨(커서 절대위치 무관). 커서가 멀어 DistFound>=WalkGap이면 DistGround<=0이 되나
		// GapLength에만 반영돼 길이를 줄일 뿐(WalkGap>=2 → GapLength>=1 보장). 기존 Max(1,..) 클램프가
		// DistFound>=WalkGap에서 GapLength=DistFound(커서거리)로 새던 "커서로 늘어남" 버그를 제거한다.
		const int32 DistGround = WalkGap - DistFound;

		// (f) 누락측 합성 Dist/Z 채움 → ①이 양쪽을 다 보게 한다. Z는 SynthGroundZ(FoundationZ 앵커).
		if (bNeg) { DistPos = DistGround; ZPos = SynthGroundZ; }  // Foundation −쪽, 맨땅 +쪽 합성
		else      { DistNeg = DistGround; ZNeg = SynthGroundZ; }  // Foundation +쪽, 맨땅 −쪽 합성

		// (f-2) #261 한쪽-지면 하강 램프 확정 → fall-through 후 Result에 표식(높은끝 스냅 게이트).
		bResultOneSideGroundRamp = true;

		// (g) return 없음 — 아래 ① 자동 맞춤 블록으로 fall-through.
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
	// F3.10 1칸 특례: 틈 1칸·Δ1단은 행간 구간이 0개지만 쐐기가 셀 안에서 100uu를 다 올린다(45°,
	// 보행 불가 — 컨베이어 전용). 등록은 PerCell R<2 폴백 = Z_low 평판 단일값 → 높은쪽 인접
	// ΔZ=100 ≤ 컨베이어 게이트(기본 100, OJJ_MaxConveyorStepZ). 일반식을 행간→칸당으로 재해석하면
	// D=2·Δ2(행간 200uu — 등록 계단의 인접 ΔZ가 컨베이어 게이트 초과 = 벨트 못 타는 램프)까지
	// 열리므로 정확히 (1칸, 1단)만 허용. 한계를 45로 내린 맵(보행 전용 정책)에선 특례도 같이
	// 닫힘 — SnapStep ≤ 한계 조건.
	const float StepLimit = FMath::Max(1.0f, MaxRampStepPerRow);
	const bool bSingleCellWedge = GapLength == 1 && Result.RiseSteps == 1
		&& AOJJ_Grid::OJJ_FoundationSnapStep <= StepLimit + KINDA_SMALL_NUMBER;
	if (Result.RiseSteps >= 1 && !bSingleCellWedge)
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
	// 1칸 특례는 행당 산식(÷(D−1))이 정의 안 됨 — 전용 라벨(0 나눗셈 가드 겸).
	if (bSingleCellWedge)
	{
		Result.DirectionSource = FString::Printf(
			TEXT("방향 자동(이웃 낮→높) — 1칸 쐐기 Δ%d단 (보행 불가 — 컨베이어 전용)"), Result.RiseSteps);
	}
	else if (Result.RiseSteps >= 1 && GapLength >= 2)
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
	// #261 한쪽-지면 하강 램프 표식 — OJJ_ComputeSnapLift가 높은끝(Foundation쪽) 엣지로 스냅하게 게이트.
	if (bResultOneSideGroundRamp)
	{
		Result.bOneSideGroundRamp = true;

		// 빗변 연장 기준: 낮은끝(r=0) 폭선 각 칸 GroundRaw 중 **최저** 월드 Z. 칼끝을 거기까지 연장하면
		// 폭 전체가 지면 이하(최저 칸은 정확히 닿고 높은 칸은 묻힘) — 어디서도 안 뜸. 낮은끝 행 매핑은
		// BuildPerCellSurfaceZ r-switch와 동일(step 0:LX=0 / 1:LY=0 / 2:LX=max / 3:LY=max). 한 칸이라도
		// GroundZ 유효면 채움(전 칸 무효 = 미베이크 → 미설정 → 소비자가 기존 단일평면 폴백, 회귀 0).
		const int32 ResStep = ((Result.EffectiveRotationSteps % 4) + 4) % 4;
		const int32 WidthCount = (ResStep % 2 == 0) ? Result.EffSize.Y : Result.EffSize.X;
		float LowestRaw = 0.0f; // 첫 유효 칸으로 시드(bAnyValid) — TNumericLimits 의존 회피.
		bool bAnyValid = false;
		for (int32 Col = 0; Col < WidthCount; ++Col)
		{
			FIntPoint LoCell = Result.Origin;
			switch (ResStep)
			{
			case 0: LoCell = FIntPoint(Result.Origin.X, Result.Origin.Y + Col); break;                       // 오르막 +X, 낮은끝 LX=0
			case 1: LoCell = FIntPoint(Result.Origin.X + Col, Result.Origin.Y); break;                       // 오르막 +Y, 낮은끝 LY=0
			case 2: LoCell = FIntPoint(Result.Origin.X + Result.EffSize.X - 1, Result.Origin.Y + Col); break; // 오르막 −X, 낮은끝 LX=max
			case 3: LoCell = FIntPoint(Result.Origin.X + Col, Result.Origin.Y + Result.EffSize.Y - 1); break; // 오르막 −Y, 낮은끝 LY=max
			}
			float CellRaw = 0.0f;
			if (Grid.OJJ_GetRawTerrainSurfaceZ(LoCell, CellRaw))
			{
				LowestRaw = bAnyValid ? FMath::Min(LowestRaw, CellRaw) : CellRaw;
				bAnyValid = true;
			}
		}
		if (bAnyValid)
		{
			Result.LoEndLowestGroundRaw = LowestRaw;
			Result.bLoEndLowestValid = true;
		}
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
	bPlacedOneSideGroundRamp = Fit.bOneSideGroundRamp; // #261 — OJJ_ComputeSnapLift 높은끝 스냅 게이트.
	// 빗변 연장: 낮은끝 폭선 최저 지형 Z 캐시(한쪽-지면 + 유효일 때만 — 비면 BuildWedge가 기존 단일평면).
	PlacedLoEndLowestGroundZ = Fit.LoEndLowestGroundRaw;
	bPlacedLoEndLowestValid = Fit.bOneSideGroundRamp && Fit.bLoEndLowestValid;
}

void AOJJ_RampFoundation::GetSaveState(
	int32& OutRiseSteps,
	bool& bOutOneSideGroundRamp,
	float& OutLoEndLowestGroundRaw,
	bool& bOutLoEndLowestValid) const
{
	OutRiseSteps = PlacedRiseSteps;
	bOutOneSideGroundRamp = bPlacedOneSideGroundRamp;
	OutLoEndLowestGroundRaw = PlacedLoEndLowestGroundZ;
	bOutLoEndLowestValid = bPlacedLoEndLowestValid;
}

void AOJJ_RampFoundation::ApplySaveState(
	int32 InRiseSteps,
	bool bInOneSideGroundRamp,
	float InLoEndLowestGroundRaw,
	bool bInLoEndLowestValid)
{
	PlacedRiseSteps = InRiseSteps;
	bPlacedOneSideGroundRamp = bInOneSideGroundRamp;
	PlacedLoEndLowestGroundZ = InLoEndLowestGroundRaw;
	bPlacedLoEndLowestValid = bInLoEndLowestValid;
	PlacedRotationSteps = ((FMath::RoundToInt(GetActorRotation().Yaw / 90.0f) % 4) + 4) % 4;
	PlacedClimbLengthCells = (PlacedRotationSteps % 2 == 0)
		? FMath::Max(1, FoundationSize.X)
		: FMath::Max(1, FoundationSize.Y);
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
		// 1행 램프는 셀별 계단 표현 불능(낮은끝 행 = 높은끝 행 — 셀당 SurfaceZ 1값) → 평판 단일값
		// 폴백. F3.10 1칸 쐐기는 이 폴백이 **정식 경로**: 장부 = Z_low(BaseSurfaceZ — 컨트롤러
		// TryPlaceFoundation 평탄 등록), 면/벨트 = 쐐기 빗변 훅(OJJ_GetVisualSurfaceZAtWorld, R=1 성립).
		// RiseSteps<1은 평지 퇴화(㉿).
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
	// #261 높은 끝(Foundation쪽) 바깥 인접 라인 — 낮은끝의 반대 변(한쪽-지면 램프 스냅용).
	FIntPoint HighLineOrigin = Origin;
	FIntPoint HighLineSize = EffSize;
	switch (Step)
	{
	case 0: // 낮은 끝 = 서쪽 변
		RowSize = FIntPoint(1, EffSize.Y);
		LineOrigin.X = Origin.X - 1;             LineSize = FIntPoint(1, EffSize.Y);
		HighLineOrigin.X = Origin.X + EffSize.X; HighLineSize = FIntPoint(1, EffSize.Y); // 높은끝 = 동쪽
		break;
	case 1: // 남쪽 변
		RowSize = FIntPoint(EffSize.X, 1);
		LineOrigin.Y = Origin.Y - 1;             LineSize = FIntPoint(EffSize.X, 1);
		HighLineOrigin.Y = Origin.Y + EffSize.Y; HighLineSize = FIntPoint(EffSize.X, 1); // 높은끝 = 북쪽
		break;
	case 2: // 동쪽 변
		RowOrigin.X = Origin.X + EffSize.X - 1;  RowSize = FIntPoint(1, EffSize.Y);
		LineOrigin.X = Origin.X + EffSize.X;     LineSize = FIntPoint(1, EffSize.Y);
		HighLineOrigin.X = Origin.X - 1;         HighLineSize = FIntPoint(1, EffSize.Y); // 높은끝 = 서쪽
		break;
	case 3: // 북쪽 변
		RowOrigin.Y = Origin.Y + EffSize.Y - 1;  RowSize = FIntPoint(EffSize.X, 1);
		LineOrigin.Y = Origin.Y + EffSize.Y;     LineSize = FIntPoint(EffSize.X, 1);
		HighLineOrigin.Y = Origin.Y - 1;         HighLineSize = FIntPoint(EffSize.X, 1); // 높은끝 = 남쪽
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

	// #261 한쪽-지면 하강 램프: 낮은끝(지면쪽)엔 Foundation 없어 위 엣지스냅 실패 → 높은끝(Foundation쪽) 엣지로
	// 스냅해 HIGH end == FoundationZ 보장. base = highNeighborZ - PlacedRiseSteps*100 (낮은끝은 지면 아래로 파고듦, 수용).
	// 한쪽-지면 auto 램프(bPlacedOneSideGroundRamp)만 — 양쪽/고정폴백은 위 낮은끝 스냅이 먼저 잡혀 무진입(격리).
	if (bPlacedOneSideGroundRamp)
	{
		float HighNeighborZ = 0.0f;
		int32 HighContact = 0;
		if (Grid.OJJ_GetDominantFoundationSurfaceZInRect(HighLineOrigin, HighLineSize, SnapGridOriginZ, HighNeighborZ, HighContact))
		{
			if (OutHeightSource) { *OutHeightSource = FString::Printf(TEXT("상속-높은끝(이웃 %d셀, rise=%d)"), HighContact, PlacedRiseSteps); }
			return (HighNeighborZ - PlacedRiseSteps * AOJJ_Grid::OJJ_FoundationSnapStep) - Thickness - Grid.GetActorLocation().Z;
		}
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
	// Foundation 4단계 §1 — 램프 Deck 틸트: 베이스 Deck 슬래브를 숨기지 않고 경사각만큼 틸트해
	// 보이는 경사면으로 쓴다. 보행/충돌은 검증된 쐐기(WedgeMesh) convex가 그대로 전담하되 시각은
	// 끈다(invisible collision body) — (1) 박막 틸트 슬래브 충돌 모험 회피 = 보행 회귀 0,
	// (2) 쐐기가 Deck 머티리얼을 복사해 색이 바뀌던 이슈는 쐐기를 안 그리므로 자동 해소(Deck는
	// 자기 머티리얼로 렌더). 장부/벨트 Z(OJJ_BuildPerCellSurfaceZ / OJJ_GetVisualSurfaceZAtWorld)는
	// 해석적 산식이라 시각과 독립 — 틸트가 그 기준을 안 건드린다.
	const float CellSize = OJJ_ResolveCellSize();
	// F3.6-1: 배치 확정값 우선(자동 맞춤 동적 길이/단수 — OJJ_NotifyFitResult가 저장),
	// 미확정(에디터 프리뷰/스폰 직후 OnConstruction)은 CDO 고정 램프 규격(기존과 동일).
	const int32 R = PlacedClimbLengthCells > 0 ? PlacedClimbLengthCells : FMath::Max(1, FoundationSize.X);
	const int32 Rise = PlacedClimbLengthCells > 0 ? PlacedRiseSteps : 1;
	// F3.8': 확정 step(미확정 에디터 프리뷰 0 — 역회전 항등). Cols 폭축 parity 산출에 먼저 필요해 위로 이동.
	const int32 Step = PlacedClimbLengthCells > 0 ? PlacedRotationSteps : 0;
	// [#7 버그 a] 폭(Cols)도 climb과 같은 parity로 축 선택 — 회전 램프(Step 1/3)는 폭축이 X다. 로드 후
	// FoundationSize가 비정사각 footprint로 바뀌면(SetFoundationSizeForSave) .Y 고정이 climb축을 집어 폭이
	// 좁아지던 버그 수정. 배치 직후(CDO 정사각)는 X=Y라 결과 동일(회귀 0).
	const int32 Cols = (Step % 2 == 0) ? FMath::Max(1, FoundationSize.Y) : FMath::Max(1, FoundationSize.X);
	const float SlabThickness = FMath::Max(1.0f, Thickness);

	// 충돌 바디 = 쐐기(시각 OFF, 충돌 유지). Rise<1(평지 브리지)은 쐐기 미생성(false) → Deck이 충돌 담당.
	const bool bWedgeBuilt = OJJ_BuildWedgeVisual(R, Cols, Rise, CellSize, Step);
	if (WedgeMesh)
	{
		// SetVisibility(false)는 렌더만 끄고 충돌/Visibility 트레이스는 유지 — 커서 스냅/보행 그대로.
		WedgeMesh->SetVisibility(false);
		if (!bWedgeBuilt)
		{
			WedgeMesh->ClearAllMeshSections();
			WedgeMesh->ClearCollisionConvexMeshes();
		}
	}
	// 계단 박스 폴백은 은퇴(Deck가 항상 면을 그림) — 잔여 인스턴스만 정리.
	if (StepMeshISM)
	{
		StepMeshISM->ClearInstances();
	}

	if (!SlabMesh || !SlabMesh->GetStaticMesh())
	{
		return;
	}
	SlabMesh->SetVisibility(true);
	// 보행은 쐐기 convex가 전담 → Deck은 시각 전용(NoCollision)으로 이중 충돌 회피.
	// 단 평지 브리지(쐐기 미생성)는 Deck이 보행 충돌도 담당(베이스 프로파일 = Pawn/Visibility Block).
	SlabMesh->SetCollisionEnabled(bWedgeBuilt ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);

	// 경사 기하: 저끝(로컬 −X) Z = 액터Z+Thickness, 고끝(로컬 +X) Z = +Rise×100 — 쐐기 빗변과 동일 평면.
	// 방향(yaw)은 액터가 90×step 회전(OJJ_BuildController.cpp:1385)하므로 Deck엔 pitch만(이중 yaw 금지).
	const float L = R * CellSize;
	const float RiseUU = Rise * AOJJ_Grid::OJJ_FoundationSnapStep;
	const float Theta = FMath::Atan2(RiseUU, L);                 // 경사각(라디안)
	const float Hyp = FMath::Sqrt(L * L + RiseUU * RiseUU);      // 빗변 길이 — 틸트 후 수평투영 = L(footprint 정합)

	// Deck를 빗변길이로 스케일(클라임=로컬 +X, 폭=로컬 +Y). 베이스 헬퍼로 평지(미틸트) 변환 산출 후 틸트 합성.
	FVector Scale, Offset;
	OJJ_ComputeDeckSlabTransform(SlabMesh->GetStaticMesh(), SlabMeshLocalRotation,
		Hyp, Cols * CellSize, SlabThickness, Scale, Offset);

	// 평지 변환된 Deck의 상면중심 P=(0,0,Thickness)를 피벗으로 pitch θ만큼 틸트 + 저끝 보정 위로 Rise/2.
	// 결과: 상면 [−L/2,T]→[+L/2,T+Rise](쐐기 빗변과 일치). +X(고끝)를 위로 = Unreal 양(+) pitch.
	const FQuat PitchQ(FRotator(FMath::RadiansToDegrees(Theta), 0.0f, 0.0f));
	const FQuat R0(SlabMeshLocalRotation);
	const FVector P(0.0f, 0.0f, SlabThickness);
	const FVector RelLoc = PitchQ.RotateVector(Offset) + P - PitchQ.RotateVector(P)
		+ FVector(0.0f, 0.0f, RiseUU * 0.5f);

	SlabMesh->SetRelativeRotation((PitchQ * R0).Rotator());
	SlabMesh->SetRelativeScale3D(Scale);
	SlabMesh->SetRelativeLocation(RelLoc);
}

void AOJJ_RampFoundation::UpdateLegVisual()
{
	// 램프 4단계 결정: 램프는 다리 없음(Deck 틸트만으로 충분 — 안 깔아도 어색하지 않음).
	// 베이스 UpdateLegVisual(4모서리 TrussTower 타일링)을 호출하지 않고, 상속 LegISM에 남은
	// 인스턴스만 정리한다(에디터에서 클래스 전환/재구성 시 잔여 다리 제거). 평지(베이스)는 무영향.
	if (LegISM)
	{
		LegISM->ClearInstances();
	}
}

FVector AOJJ_RampFoundation::OJJ_ClimbDirForStep(int32 RotationSteps)
{
	// 등록 r-switch(OJJ_BuildPerCellSurfaceZ)와 동일 규약 — 쐐기 꼭짓점/빗변 Z 조회의 방향 단일원.
	switch (((RotationSteps % 4) + 4) % 4)
	{
	default:
	case 0: return FVector(1.0f, 0.0f, 0.0f);
	case 1: return FVector(0.0f, 1.0f, 0.0f);
	case 2: return FVector(-1.0f, 0.0f, 0.0f);
	case 3: return FVector(0.0f, -1.0f, 0.0f);
	}
}

bool AOJJ_RampFoundation::OJJ_GetVisualSurfaceZAtWorld(const FVector& WorldPos, float& OutZ) const
{
	// F3.8'' 벨트 반행 편차 해소: 면(쐐기 빗변)의 보간 Z — 쐐기 꼭짓점 산식과 동일 수식·프레임
	// (칼끝 d=0 → T, 높은 끝 d=L → T+Rise). 장부(셀 계단)는 무변경 — "데이터=계단, 면·벨트=빗변".
	const int32 R = FMath::Max(1, PlacedClimbLengthCells > 0 ? PlacedClimbLengthCells : FoundationSize.X);
	const int32 Rise = PlacedClimbLengthCells > 0 ? PlacedRiseSteps : 1;
	if (Rise < 1)
	{
		return false; // 평지 브리지 — 면=장부(평탄), 보정 불필요.
	}
	const float CellSize = OJJ_ResolveCellSize();
	const float L = R * CellSize;
	if (L <= KINDA_SMALL_NUMBER)
	{
		return false;
	}
	const FVector ClimbDir = OJJ_ClimbDirForStep(PlacedRotationSteps);
	const FVector ToPos = WorldPos - GetActorLocation(); // 액터 = 풋프린트 중심(쐐기와 동일 기준)
	const float Alpha = FMath::Clamp(
		(ClimbDir.X * ToPos.X + ClimbDir.Y * ToPos.Y + L * 0.5f) / L, 0.0f, 1.0f);
	// 빗변 연장(한쪽-지면 램프) 무영향 — 연장은 같은 슬로프 직선을 칼끝 너머로 늘릴 뿐 footprint 구간
	// [−L/2,+L/2]의 표면 Z는 불변(칼끝 d=0 → T, 높은끝 d=L → T+Rise 그대로). 컨베이어는 등록 footprint
	// 셀 위에만 앉으므로(연장 돌출부는 footprint 밖) 이 수식이 연장 후에도 빗변 위 정확 — 가라앉음 없음.
	// L/기준 동기화 불필요(연장량을 여기 안 더하는 게 정답 — 더하면 벨트가 기존 빗변에서 어긋남).
	OutZ = GetActorLocation().Z + FMath::Max(1.0f, Thickness)
		+ Alpha * Rise * AOJJ_Grid::OJJ_FoundationSnapStep;
	return true;
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
	// 월드 오르막(낮은 끝 → 높은 끝) — 방향 단일원(OJJ_ClimbDirForStep: 등록 r-switch·빗변 Z 조회 공유).
	const FVector ClimbDir = OJJ_ClimbDirForStep(Step);
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
	FVector KnifeCenter = -ClimbDir * (L * 0.5f) + Up * T;              // 낮은 끝 경계(아랫단 상면)
	const FVector HighCenter = ClimbDir * (L * 0.5f) + Up * T;          // 높은 끝 경계(바닥 높이)
	const FVector Side = SideDir * (W * 0.5f);

	// 빗변 연장(한쪽-지면 램프만): 칼끝이 낮은끝 폭선 최저 지형 위에 떠 있으면(DownZ>0), 빗변 기울기
	// (Rise/L)를 그대로 유지한 채 칼끝을 −ClimbDir로 ExtL 더 빼고 Z를 DownZ만큼 낮춘다. 이러면 칼끝이
	// 최저 지형에 정확히 닿고(폭 전체 지면 이하 — 안 뜸), 빗변 평면이 지형을 가로질러 높은 칸은 묻힌다.
	// 핵심: 슬로프 선 자체는 불변(같은 직선을 연장만) → footprint 구간[−L/2,+L/2]의 표면 Z는 그대로라
	// 벨트(OJJ_GetVisualSurfaceZAtWorld)·등록 장부 무영향(컨베이어 안 가라앉음). 캐시 무효(양쪽/고정/
	// 미베이크)면 KnifeCenter 그대로 = 기존 단일평면(회귀 0). DownZ≤0(이미 묻힘/닿음)도 연장 0.
	if (bPlacedOneSideGroundRamp && bPlacedLoEndLowestValid && Rise > KINDA_SMALL_NUMBER)
	{
		const float ActorZ = GetActorLocation().Z;
		const float KnifeWorldZ = ActorZ + T;                  // 현 칼끝 월드 Z(슬로프 낮은끝)
		const float DownZ = KnifeWorldZ - PlacedLoEndLowestGroundZ; // >0 = 칼끝이 최저 지형 위로 뜸 → 연장
		if (DownZ > KINDA_SMALL_NUMBER)
		{
			const float ExtL = DownZ * (L / Rise);             // 기울기 Rise/L 유지 — 수평 연장량
			KnifeCenter = -ClimbDir * (L * 0.5f + ExtL) + Up * (T - DownZ);
		}
	}

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
