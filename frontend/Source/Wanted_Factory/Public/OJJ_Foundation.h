// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OJJ_Foundation.generated.h"

class AOJJ_Grid;
class UStaticMeshComponent;

/**
 * Foundation(기초) 액터 — F1-b. AActor 직속(설계 §7-1 — AMachineBase 상속 금지):
 * 머신 계약(레시피/버퍼/내구도/전력)이 전부 불필요하고, 머신은 점유(차단) 모델인데 Foundation은
 * 커버리지(허가) 모델이라 방향이 반대다.
 *
 * 그리드 등록/해제는 F1-a API(AOJJ_Grid::TryPlaceFoundation / RemoveFoundation) 경유 —
 * 배치는 BuildController의 Foundation 빌드 모드가 수행한다. WaterArea(BeginPlay 자가 등록)와 달리
 * 자가 등록하지 않음: 빌드 모드 스폰 전용이라 등록 주체가 항상 컨트롤러(레벨 사전 배치는 후속 결정).
 *
 * F1은 높이 1단 고정: 상면 Z = 그리드 평면 Z + Thickness. 지형/엣지 높이 스냅은 F2.
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

private:
	// FoundationSize/Thickness/그리드 CellSize에 맞춰 슬래브 스케일·위치 갱신.
	// 액터 원점 = 풋프린트 중심(GetFoundationPlacementLocation 계약) → XY 오프셋 0, 상면 = 평면 + Thickness.
	void UpdateSlabVisual();

	// EndPlay 대칭 해제용(TryPlaceFoundation 성공 시 OJJ_NotifyPlacedOnGrid로 세팅). 미등록이면 무효.
	TWeakObjectPtr<AOJJ_Grid> RegisteredGrid;
};
