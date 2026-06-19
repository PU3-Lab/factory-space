// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OJJ_Foundation.h"
#include "OJJ_RampFoundation.generated.h"

class UInstancedStaticMeshComponent;
class UProceduralMeshComponent;

/**
 * 램프 Foundation (F3-2, 계획 ㉰㉱㉲ — f3_slope_link_plan.md) — 낮은 단에서 +1단(100uu)으로 오르는
 * 경사 Foundation. 단차 100uu(도보 불가)를 행당 100/(R−1)uu 계단으로 분해해 캐릭터가 걸어 오른다.
 *
 * 경사 표현 = 계단 근사(㉰): 셀별 SurfaceZ(r) = Z_low + (r/(R−1))×100, r = 오르는 방향 행,
 * R = FoundationSize.X(오르는 방향 길이 — 로컬 +X). 양 끝 행(r=0, r=R−1)은 인접 평판 단의
 * SurfaceZ와 정확 일치(턱 0) — 걸침 배치/도보는 기존 균일 규칙(OJJ_GetUniformSurfaceZ)이 자동
 * 허용하고, 중간 경사 행 위 머신/컨베이어는 이높이 비교가 자동 거부한다(추가 규칙 0).
 *
 * 오르는 방향 = 로컬 +X. 월드 방향은 배치 회전 step(F3-0 EffSize/yaw 인프라)이 결정:
 * step 0=+X / 1=+Y / 2=−X / 3=−Y. 등록(셀별 SurfaceZ는 월드 풋프린트 인덱싱)과 비주얼(로컬
 * 계단 + 액터 yaw)이 같은 회전 규약을 공유해야 함 — OJJ_BuildPerCellSurfaceZ 참조.
 */
UCLASS()
class WANTED_FACTORY_API AOJJ_RampFoundation : public AOJJ_Foundation
{
	GENERATED_BODY()

public:
	AOJJ_RampFoundation();

	// 자동 맞춤 풋프린트(F3.6-1, 계획 §1·§4 — f3_6_autofit_ramp_plan.md): 회전 축(R키 — 축만, ㊁)
	// 양방향으로 이웃 Foundation 라인을 스캔해 ① 양쪽 발견 → 틈 D칸 × 폭 W(㊀ — CDO FoundationSize.Y)
	// 풋프린트 + ΔZ=k단 자동 산출(부호 = 이웃 낮→높 자동), 행간 계단 ≤ MaxRampStepPerRow 검증(미달 시
	// bValid=false + 최소 칸수 사유) ② 한쪽/무이웃 → 베이스 폴백(고정 램프 — R이 축+부호, F3.5 ③
	// 엣지 스냅/씨앗은 기존 그대로). ΔZ=0은 평지 브리지(㉿ — RiseSteps 0).
	// F3.10: 틈 1칸·Δ1단은 특례 허용(1칸 45° 쐐기, 보행 불가 — 컨베이어 전용) — 장부는 Z_low 평판
	// 단일값(OJJ_BuildPerCellSurfaceZ R<2 폴백), 면/벨트는 쐐기 빗변 훅. Δ≥2단 1칸은 기존대로 거부.
	virtual FOJJFoundationFitResult OJJ_ComputeHoverFootprint(const AOJJ_Grid& Grid, FIntPoint CursorCell,
		int32 RotationSteps) const override;

	// 배치 확정 풋프린트 저장(F3.6-1) — 자동 맞춤의 동적 길이/상승 단수를 계단 비주얼이 쓰도록.
	virtual void OJJ_NotifyFitResult(const FOJJFoundationFitResult& Fit) override;

	// 쐐기 빗변 면의 보간 Z(F3.8'' — 벨트 반행 편차 해소): 쐐기 꼭짓점과 같은 프레임/수식
	// (OJJ_ClimbDirForStep + L/Rise/Thickness — 단일원). 평지 브리지(Rise 0)는 false(면=장부 평탄).
	virtual bool OJJ_GetVisualSurfaceZAtWorld(const FVector& WorldPos, float& OutZ) const override;

	// 계단 근사 셀별 SurfaceZ(결정 ㉲ — 산식은 클래스 책임, 그리드는 불변식만 검증).
	// R<2(경사 불능) 또는 RiseSteps<1(상승 없음 — ㉿ 평지 퇴화)면 false — 평판 단일값 경로 폴백.
	// F3.6-0: R = EffSize의 오르는 방향 축(고정 램프에선 FoundationSize.X와 동치 — 스왑 규칙),
	// 총 상승 = RiseSteps×100 — 동적 풋프린트(F3.6-1 자동 맞춤)에 자동 정합.
	virtual bool OJJ_BuildPerCellSurfaceZ(FIntPoint EffSize, int32 RotationSteps, float BaseSurfaceZ,
		int32 RiseSteps, TArray<float>& OutCellZs) const override;

	// Z_low 결정(F3.5 ③): 낮은 끝 바깥 인접 라인의 이웃 SurfaceZ에 엣지 스냅(낮은 끝 = 이웃,
	// 높은 끝 = +100 — 단 간격 고정이라 높은 쪽 이웃과도 자동 일치). 이웃 없으면 낮은 끝(r=0) 행
	// 지형 씨앗 폴백(F3-2 Codex ② 유지 — 고립 램프 = 지형 오르기 용도).
	virtual float OJJ_ComputeSnapLift(const AOJJ_Grid& Grid, FIntPoint Origin, FIntPoint EffSize,
		int32 RotationSteps, FString* OutHeightSource = nullptr) const override;

protected:
	// 램프 4단계: 베이스 Deck 슬래브를 숨기지 않고 경사각만큼 틸트해 보이는 경사면으로 사용
	// (보행/충돌은 쐐기 convex 전담, 시각 OFF). 계단 박스 폴백 은퇴.
	virtual void UpdateSlabVisual() override;

	// 램프 4단계 결정: 램프는 다리 없음(Deck 틸트만 — 안 깔아도 어색하지 않음). 베이스 4모서리 다리
	// 로직을 오버라이드해 상속 LegISM의 인스턴스를 비운다(평지는 베이스 그대로 다리 유지).
	virtual void UpdateLegVisual() override;

	// 자동 맞춤 이웃 스캔 한계(㉾ — 축 방향 한쪽당 최대 라인 수). 초과 틈은 미검출 → 고정 램프 폴백.
	// 프리뷰 셀 수 최악(2×32−1)×8 = 504 < 그리드 가드 4096 — 폭주 없음.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Foundation", meta = (ClampMin = "1"))
	int32 MaxAutoFitScanCells = 32;

	// 자동 맞춤 행간 계단 거부 한계(uu) — F3.7' 개정: 보행 기준 45 고정 → 프로퍼티. 기본 100 =
	// Δ1단 틈 2칸(행당 100uu)까지 허용 — 짧은 틈 급경사(보행 불가, 컨베이어 전용 램프)를 플레이어
	// 선택으로 수용(사용자 결정 2026-06-12). 보행 가능 여부는 거부 대신 호버/배치 로그가 경고.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Foundation", meta = (ClampMin = "1.0"))
	float MaxRampStepPerRow = 100.0f;

	// 보행 가능 행간 계단 기준(uu) — 캐릭터 MaxStepHeight 기본 45(F2-3 Thickness 45와 동일 근거).
	// 거부 게이트가 아니라 로그 경고 기준(배치 가부는 MaxRampStepPerRow가 결정).
	static constexpr float OJJ_WalkableStepPerRow = 45.0f;

	// 배치 확정값(OJJ_NotifyFitResult — 자동 맞춤 동적 비주얼용). 0 = 미확정(CDO 고정 램프 규격 사용:
	// 길이 FoundationSize.X / 1단) — 에디터 프리뷰(OnConstruction)와 기존 고정 램프 경로 회귀 0.
	int32 PlacedClimbLengthCells = 0;
	int32 PlacedRiseSteps = 1;
	// #261 한쪽-지면 하강 램프 표식(OJJ_NotifyFitResult가 Fit.bOneSideGroundRamp로 세팅) —
	// OJJ_ComputeSnapLift 높은끝(Foundation쪽) 엣지 스냅 게이트. 양쪽/고정폴백은 false라 무진입(격리).
	bool bPlacedOneSideGroundRamp = false;
	// 빗변 연장: 낮은끝 폭선의 최저 지형 월드 Z(OJJ_NotifyFitResult가 Fit.LoEndLowestGroundRaw로 캐시).
	// OJJ_BuildWedgeVisual이 칼끝을 이 Z까지 −ClimbDir로 연장(기울기 유지)해 평면이 지형에 닿게 한다.
	// bPlacedOneSideGroundRamp(한쪽-지면)이고 유효일 때만 — 양쪽/고정 램프는 미진입(회귀 0).
	float PlacedLoEndLowestGroundZ = 0.0f;
	bool bPlacedLoEndLowestValid = false;
	// 배치 확정 회전 step(F3.8' — 쐐기가 등록 r-switch와 같은 월드 방향 규약으로 기하를 만들고
	// 액터 yaw 역회전을 선적용하는 데 사용). 미확정(에디터 프리뷰) 0 = 역회전 항등.
	int32 PlacedRotationSteps = 0;

	// 계단 박스 비주얼/충돌 — SlabMesh와 동일 충돌 프로파일(Pawn/Visibility Block, Camera Ignore).
	// F3.8부터 쐐기(WedgeMesh) 실패 시 폴백 전용.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Foundation")
	TObjectPtr<UInstancedStaticMeshComponent> StepMeshISM;

	// 쐐기 비주얼(F3.8 — 에디터 수동 모델링 대신 코드 생성). 충돌 = Convex 단순 충돌
	// (쐐기는 볼록체 — complex-as-simple 비사용), 채널은 계단 ISM 프로파일 미러.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Foundation")
	TObjectPtr<UProceduralMeshComponent> WedgeMesh;

	// 오르막 월드 방향 단일원(F3.8''): 등록 r-switch와 동일 규약(0:+X/1:+Y/2:−X/3:−Y) —
	// 쐐기 꼭짓점(OJJ_BuildWedgeVisual)과 빗변 Z 조회(OJJ_GetVisualSurfaceZAtWorld)가 이 한 곳을 공유.
	static FVector OJJ_ClimbDirForStep(int32 RotationSteps);

	// 쐐기 메시 생성 — 꼭짓점 6(칼끝 2 + 높은 끝 바닥 2 + 높은 끝 상단 2), 면 5(빗변 상면/바닥/
	// 수직 끝면/옆면 2). 양 끝 정합: 칼끝 = 아랫단 상면 엣지(Z=Thickness), 수직 끝면 상단 = 윗단
	// 상면(+Rise). F3.8' 방향 규약: 기하를 등록 r-switch와 같은 **월드 축** 방향(0:+X/1:+Y/2:−X/
	// 3:−Y = 오르막)으로 만들고 액터 yaw(+90°×step)의 정확한 역회전을 정점에 선적용 — 합성 항등이라
	// yaw 규약 가정과 무관하게 월드 기하 = 등록 데이터(이전 "로컬 +X = 오르막 + yaw 위임" 가정에서
	// 방향별 수직 어긋남 관찰 — 가정 의존 제거). false = 생성 불가(RiseSteps<1 등) — 계단 폴백.
	bool OJJ_BuildWedgeVisual(int32 ClimbCells, int32 WidthCells, int32 RiseSteps, float CellSize,
		int32 RotationSteps);
};
