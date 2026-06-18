// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OJJ_Foundation.generated.h"

class AOJJ_Grid;
class UStaticMesh;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;

// Foundation 호버/배치 풋프린트 산출 결과(F3.6-0, 결정 ㉽ — f3_6_autofit_ramp_plan.md §2).
// 자동 맞춤 램프(F3.6-1)는 풋프린트가 "커서+그리드 상태의 함수"라 컨트롤러의 CDO 정적 산출을
// 훅으로 대체 — 호버와 배치가 같은 훅을 호출해 "미리보기 = 실제 배치" 단일원을 유지한다.
struct FOJJFoundationFitResult
{
	// lower-left origin / 회전 적용된 월드 풋프린트 크기 — 기존 컨트롤러 정적 산출과 동일 의미.
	FIntPoint Origin = FIntPoint::ZeroValue;
	FIntPoint EffSize = FIntPoint(1, 1);

	// 총 상승 단수 k(셀별 SurfaceZ 산식의 상승 = k×100uu). 고정 램프 = 1(평판은 미사용),
	// 자동 맞춤(F3.6-1)이 이웃 ΔZ로 채움. 0 = 평지 브리지(㉿ — 퍼셀 훅이 false로 평판 경로 퇴화).
	int32 RiseSteps = 1;

	// 배치에 실제 적용할 회전 step(F3.6-1, 결정 ㊁) — 스냅/퍼셀 산식/액터 yaw가 이 값을 쓴다.
	// 베이스/고정 램프 = 입력 step 그대로(수동 — R이 축+부호). 자동 맞춤 = 축은 입력에서,
	// 부호(낮→높 방향)는 양쪽 이웃 Z 비교로 자동 결정해 덮어씀.
	int32 EffectiveRotationSteps = 0;

	// 양쪽 이웃 스캔으로 산출된 자동 맞춤 풋프린트 여부(F3.6-1).
	bool bAutoFit = false;

	// 방향 출처(㊁ 보강 — 자동/수동 이원화 UX 방어): "방향 자동(이웃 낮→높)…" vs "방향 수동(R)".
	// 비어 있으면 방향 무의미(평판) — 컨트롤러가 호버/배치 로그에 표시.
	FString DirectionSource;

	// false = 풋프린트 구성 불가(자동 맞춤 경사 한계 미달 등) — 배치 거부 + FailReason 로그.
	// 호버 프리뷰 빨강 강제는 F3.6-1에서 연결. 베이스/고정 램프는 항상 true(회귀 0).
	bool bValid = true;
	FString FailReason;

	// #261 한쪽-지면 하강 램프 표식 — OJJ_ComputeSnapLift 높은끝 스냅 게이트.
	bool bOneSideGroundRamp = false;

	// 빗변 연장: 한쪽-지면 램프 낮은끝(r=0) 폭선 각 칸 GroundRaw 중 **최저** 월드 Z. 칼끝을 이 Z까지
	// −ClimbDir로 연장(빗변 기울기 유지)해 단일평면이 지형과 교차 — 높은 칸 묻힘·최저 칸 닿음(안 뜸).
	// bOneSideGroundRamp이고 한 칸이라도 GroundZ 유효일 때만 채워진다(bLoEndLowestValid).
	float LoEndLowestGroundRaw = 0.0f;
	bool bLoEndLowestValid = false;
};

/**
 * Foundation(기초) 액터 — F1-b. AActor 직속(설계 §7-1 — AMachineBase 상속 금지):
 * 머신 계약(레시피/버퍼/내구도/전력)이 전부 불필요하고, 머신은 점유(차단) 모델인데 Foundation은
 * 커버리지(허가) 모델이라 방향이 반대다.
 *
 * 그리드 등록/해제는 F1-a API(AOJJ_Grid::TryPlaceFoundation / RemoveFoundation) 경유 —
 * 배치는 BuildController의 Foundation 빌드 모드가 수행한다. WaterArea(BeginPlay 자가 등록)와 달리
 * 자가 등록하지 않음: 빌드 모드 스폰 전용이라 등록 주체가 항상 컨트롤러(레벨 사전 배치는 후속 결정).
 *
 * 높이는 F2-4 스냅: 컨트롤러가 액터를 N×100uu(그리드 OJJ_ComputeFoundationSnapLift) 들어 올려 배치 —
 * 상면 Z = 액터 Z + Thickness = 평면 + Thickness + N×100. 평탄 지대 N=0(F1 동작과 동일).
 */
UCLASS()
class WANTED_FACTORY_API AOJJ_Foundation : public AActor
{
	GENERATED_BODY()

public:
	AOJJ_Foundation();

	// 에디터 배치/프로퍼티 변경 시 슬래브 비주얼이 즉시 따라오도록 갱신(WaterArea 패턴).
	virtual void OnConstruction(const FTransform& Transform) override;

	FIntPoint GetFoundationSize() const { return FoundationSize; }
	float GetThickness() const { return Thickness; }

	// 고스트 프리뷰(#187)가 슬래브 메시를 액터 spawn 없이 그리기 위한 접근자(CDO에서 StaticMesh 취득).
	UStaticMeshComponent* GetSlabMesh() const { return SlabMesh; }

	// [Deck] 슬래브 메시 로컬 회전 보정 — 고스트가 배치와 같은 회전을 쓰도록 노출.
	FRotator GetSlabMeshLocalRotation() const { return SlabMeshLocalRotation; }

	// [Deck] Deck 슬래브 메시를 footprint(TargetX×TargetY uu) × Thickness에 맞추는 스케일 + 피벗/회전 보정
	// 오프셋(XY는 박스중심을 원점에, Z는 윗면을 +Thickness에)을 산출. 배치(UpdateSlabVisual)·고스트 공용 —
	// "미리보기 = 실제 배치" 정합을 한 곳에서 보장. (회전 0 기준 정확; 비0 회전 시 per-axis 스케일은 근사.)
	static void OJJ_ComputeDeckSlabTransform(const UStaticMesh* Mesh, const FRotator& Rot,
		float TargetX, float TargetY, float Thickness, FVector& OutScale, FVector& OutOffset);

	// 배치 확정 직후 BuildController가 호출 — EndPlay 대칭 해제용 그리드 보관 + 비주얼 확정 갱신.
	void OJJ_NotifyPlacedOnGrid(AOJJ_Grid* Grid);

	// 풋프린트 산출 훅(F3.6-0, 결정 ㉽ — ㉲와 같은 "산식은 클래스 책임" 패턴). 베이스 = CDO 크기
	// 정적 산출(홀수 step X/Y 스왑 + lower-left origin 공통 수식) — 기존 컨트롤러 산출과 동작 동일.
	// 자동 맞춤 램프(F3.6-1)가 override해 회전 축 양방향 이웃 스캔 풋프린트로 대체한다.
	// 호버(UpdateFoundationHover)와 배치(PlaceFoundationAtCursor)가 같은 훅을 호출해야 함.
	virtual FOJJFoundationFitResult OJJ_ComputeHoverFootprint(const AOJJ_Grid& Grid, FIntPoint CursorCell,
		int32 RotationSteps) const;

	// 배치 확정 풋프린트 통지(F3.6-1) — 컨트롤러가 spawn 직후(등록 전) 호출. CDO 고정 크기와 다른
	// 동적 풋프린트(자동 맞춤 길이/상승 단수)를 비주얼이 쓸 수 있게 액터에 저장하는 용도.
	// 베이스 no-op(평판 비주얼은 FoundationSize로 충분 — 회귀 0).
	virtual void OJJ_NotifyFitResult(const FOJJFoundationFitResult& Fit) {}

	// 비주얼 면 Z 조회(F3.8'' — 면이 장부와 다른 클래스용). false = 면=장부(평판 — 호출자가 장부
	// SurfaceZ 사용). 램프가 override해 쐐기 빗변 보간 Z 반환. **경계: 장부(FoundationCells의 셀별
	// SurfaceZ = 셀 계단)는 배치 검증/걸침/걷기의 진실원으로 무변경 — 이 훅은 벨트 비주얼 Z 전용
	// 소비("데이터=계단, 면·벨트=빗변").**
	virtual bool OJJ_GetVisualSurfaceZAtWorld(const FVector& WorldPos, float& OutZ) const { return false; }

	// 셀별 SurfaceZ 산식 훅(F3-2, 결정 ㉲ — 산식은 클래스 책임). false = 평탄(전 셀 동일 — 단일값 등록
	// 경로 사용, 배열 미생성). 램프 등 비평탄 파생이 override해 EffSize×회전 기준 배열을 채우면
	// 컨트롤러가 OJJ_TryPlaceFoundationPerCell로 등록(그리드가 불변식 검증).
	// BaseSurfaceZ = 낮은 단 상면(평면 + Thickness + 스냅 리프트). 인덱싱 = (LX×EffSize.Y + LY).
	// RiseSteps = 총 상승 단수 k(F3.6-0 일반화 — 풋프린트 훅 결과를 컨트롤러가 전달, 고정 램프 1).
	virtual bool OJJ_BuildPerCellSurfaceZ(FIntPoint EffSize, int32 RotationSteps, float BaseSurfaceZ,
		int32 RiseSteps, TArray<float>& OutCellZs) const
	{
		return false;
	}

	// 높이 산출 훅(F3.5 — 높이 결정 우선순위의 단일 진입점, 반환 = 평면 기준 리프트):
	// ① 인접 Foundation 있으면 높이 상속(평면 확장 — 지형 무시·클리핑 허용, 새티스팩토리 정합)
	// ② 고립 첫 장이면 지형 씨앗(F2-4 산식 존속). 램프는 ③ 낮은 끝 엣지 스냅(오버라이드).
	// OutHeightSource: 배치 로그용 출처 문자열("상속(이웃 접촉 y셀)"/"씨앗(지형)" — 결정 ㉷ 보강).
	virtual float OJJ_ComputeSnapLift(const AOJJ_Grid& Grid, FIntPoint Origin, FIntPoint EffSize,
		int32 RotationSteps, FString* OutHeightSource = nullptr) const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 풋프린트(셀 단위). §5-1 합의 8×8 — 프로토 체감 후 조정 가능하도록 프로퍼티로 노출.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Foundation", meta = (ClampMin = "1"))
	FIntPoint FoundationSize = FIntPoint(8, 8);

	// 슬래브 두께(uu) — 상면 Z = 그리드 평면 Z + Thickness. TryPlaceFoundation의 SurfaceZ 산출에 사용.
	// 45 = F2-3 확정: 캐릭터 MaxStepHeight 기본값(45) 이하라 평지→Foundation 도보 진입 성립
	// (50은 불가). F2-4 높이 스냅의 BaseLift로도 쓰임(상면 = 평면 + Thickness + N×100).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Foundation", meta = (ClampMin = "1.0"))
	float Thickness = 45.0f;

	// 임시 비주얼 — 엔진 Cube 스케일(F1-b 결정점 ③). NoCollision: 커서/베이크/머신 호버 트레이스 간섭 0.
	// 걷기 충돌·전용 메시/머티리얼은 F1-c(머신 Z 안착)와 함께.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Foundation")
	TObjectPtr<UStaticMeshComponent> SlabMesh;

	// [Deck] 슬래브(Deck) 메시 로컬 회전 보정(deg). Deck 임포트 축이 틀어졌을 때(예: 두께축이 Z 아님) 세움.
	// 기본 0(평평한 deck은 보정 불필요 가정) — DIAG 로그로 축 확인 후 필요시 다이얼(사다리 전례).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foundation")
	FRotator SlabMeshLocalRotation = FRotator::ZeroRotator;

	// [Deck step2] 4모서리 다리(TrussTower) ISM — 평지·램프 공용. 충돌 없음(시각 지지대; 걷기 충돌은 데크 슬래브).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Foundation")
	TObjectPtr<UInstancedStaticMeshComponent> LegISM;

	// [Deck step2] 다리 메시 로컬 회전 보정(deg). TrussTower 세로축이 Z 아니면(누움) 세움. 기본 0 — DIAG로 확인 후 다이얼.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foundation")
	FRotator LegMeshLocalRotation = FRotator::ZeroRotator;

	// [Deck step2] 다리 굵기 배율 — **XY만** 스케일(Z=1 고정 → 세로높이/SegZ·타일링 단위 보존). 기본 1.5
	// (네이티브 XY≈40 → ≈60cm). 적정 굵기 PIE 보고 다이얼. ⚠️ Z는 안 키움(타일링 단위 틀어짐 방지).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foundation", meta = (ClampMin = "0.01"))
	float LegMeshScaleMultiplier = 1.5f;

	// FoundationSize/Thickness/그리드 CellSize에 맞춰 슬래브 스케일·위치 갱신.
	// 액터 원점 = 풋프린트 중심(GetFoundationPlacementLocation 계약) → XY 오프셋 0, 상면 = 액터 Z + Thickness
	// (액터 상대라 F2-4 스냅 리프트에도 수식 불변 — 컨트롤러가 액터째 들어 올림).
	// F3-2: virtual — 램프 등 비평탄 파생이 계단 비주얼로 교체(OnConstruction/NotifyPlacedOnGrid가 디스패치).
	virtual void UpdateSlabVisual();

	// [Deck step2] 4모서리 다리(TrussTower) ISM 갱신 — 평지·램프 공용(OnConstruction/NotifyPlacedOnGrid에서 호출).
	// 2단계: 4모서리 셀 중심에 고정 1세그. 3단계에서 모서리별 지형Z까지 타일링(사다리 Overshoot 정렬 재활용).
	void UpdateLegVisual();

	// 그리드 CellSize 역산(public GridToWorld 경유 — 등록 그리드 우선, 폴백 월드 첫 그리드, 기본 100).
	// UpdateSlabVisual에서 추출(F3-2) — 파생 비주얼도 동일 규칙 공유.
	float OJJ_ResolveCellSize() const;

private:
	// EndPlay 대칭 해제용(TryPlaceFoundation 성공 시 OJJ_NotifyPlacedOnGrid로 세팅). 미등록이면 무효.
	TWeakObjectPtr<AOJJ_Grid> RegisteredGrid;
};
