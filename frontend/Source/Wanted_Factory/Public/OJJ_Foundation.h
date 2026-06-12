// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OJJ_Foundation.generated.h"

class AOJJ_Grid;
class UStaticMeshComponent;

// Foundation 호버/배치 풋프린트 산출 결과(F3.6-0, 결정 ㉽ — f3_6_autofit_ramp_plan.md §2).
// 자동 맞춤 램프(F3.6-1)는 풋프린트가 "커서+그리드 상태의 함수"라 컨트롤러의 CDO 정적 산출을
// 훅으로 대체 — 호버와 배치가 같은 훅을 호출해 "미리보기 = 실제 배치" 단일원을 유지한다.
struct FOJJFoundationFitResult
{
	// lower-left origin / 회전 적용된 월드 풋프린트 크기 — 기존 컨트롤러 정적 산출과 동일 의미.
	FIntPoint Origin = FIntPoint::ZeroValue;
	FIntPoint EffSize = FIntPoint(1, 1);

	// 총 상승 단수 k(셀별 SurfaceZ 산식의 상승 = k×100uu). 고정 램프 = 1(평판은 미사용),
	// 자동 맞춤(F3.6-1)이 이웃 ΔZ로 채움.
	int32 RiseSteps = 1;

	// 양쪽 이웃 스캔으로 산출된 자동 맞춤 풋프린트 여부 — F3.6-1 전까지 항상 false.
	bool bAutoFit = false;

	// false = 풋프린트 구성 불가(자동 맞춤 경사 한계 미달 등) — 배치 거부 + FailReason 로그.
	// 호버 프리뷰 빨강 강제는 F3.6-1에서 연결. 베이스/고정 램프는 항상 true(회귀 0).
	bool bValid = true;
	FString FailReason;
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

	// 배치 확정 직후 BuildController가 호출 — EndPlay 대칭 해제용 그리드 보관 + 비주얼 확정 갱신.
	void OJJ_NotifyPlacedOnGrid(AOJJ_Grid* Grid);

	// 풋프린트 산출 훅(F3.6-0, 결정 ㉽ — ㉲와 같은 "산식은 클래스 책임" 패턴). 베이스 = CDO 크기
	// 정적 산출(홀수 step X/Y 스왑 + lower-left origin 공통 수식) — 기존 컨트롤러 산출과 동작 동일.
	// 자동 맞춤 램프(F3.6-1)가 override해 회전 축 양방향 이웃 스캔 풋프린트로 대체한다.
	// 호버(UpdateFoundationHover)와 배치(PlaceFoundationAtCursor)가 같은 훅을 호출해야 함.
	virtual FOJJFoundationFitResult OJJ_ComputeHoverFootprint(const AOJJ_Grid& Grid, FIntPoint CursorCell,
		int32 RotationSteps) const;

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

	// FoundationSize/Thickness/그리드 CellSize에 맞춰 슬래브 스케일·위치 갱신.
	// 액터 원점 = 풋프린트 중심(GetFoundationPlacementLocation 계약) → XY 오프셋 0, 상면 = 액터 Z + Thickness
	// (액터 상대라 F2-4 스냅 리프트에도 수식 불변 — 컨트롤러가 액터째 들어 올림).
	// F3-2: virtual — 램프 등 비평탄 파생이 계단 비주얼로 교체(OnConstruction/NotifyPlacedOnGrid가 디스패치).
	virtual void UpdateSlabVisual();

	// 그리드 CellSize 역산(public GridToWorld 경유 — 등록 그리드 우선, 폴백 월드 첫 그리드, 기본 100).
	// UpdateSlabVisual에서 추출(F3-2) — 파생 비주얼도 동일 규칙 공유.
	float OJJ_ResolveCellSize() const;

private:
	// EndPlay 대칭 해제용(TryPlaceFoundation 성공 시 OJJ_NotifyPlacedOnGrid로 세팅). 미등록이면 무효.
	TWeakObjectPtr<AOJJ_Grid> RegisteredGrid;
};
