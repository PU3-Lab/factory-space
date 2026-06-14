// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OJJ_StarDome.generated.h"

class UStaticMeshComponent;

/**
 * 밤하늘 별 돔 (#185). 카메라를 따라다니되 회전은 고정인 거대한 역방향 구(球) — 안쪽 면에 절차적 별
 * 머티리얼(M_Stars, Two-Sided/Unlit)을 입혀 무한 원거리 별하늘처럼 보이게 한다.
 *
 * 설계:
 *  - 매 틱 액터 위치를 0번 플레이어 카메라 위치로 옮긴다(회전은 건드리지 않음 → 별자리 고정).
 *  - 카메라가 항상 돔 중심에 있으므로 시차(parallax)가 없어 별이 원거리에 박힌 것처럼 보인다.
 *  - Collision=NoCollision: 빌드 배치 레이캐스트/플레이어 이동을 가리지 않는다.
 *  - 그림자 끔: 거대한 구가 씬 라이팅에 그림자를 드리우지 않도록.
 *
 * 밤에만 보이는 강도는 머티리얼이 MPC_Sky의 StarIntensity 스칼라를 참조하며, AOJJ_DayNightController가
 *   매 틱 NightFactor(낮 0/밤 1)를 그 스칼라에 기록한다(이 액터는 강도 제어를 하지 않음 — 표시만 담당).
 *
 * ⚠️ SkyAtmosphere/안개와의 정합 및 원거리 오브젝트 가림 여부는 PIE 시각 확인 후 DomeScale로 튜닝.
 */
UCLASS()
class WANTED_FACTORY_API AOJJ_StarDome : public AActor
{
	GENERATED_BODY()

public:
	AOJJ_StarDome();

	virtual void Tick(float DeltaSeconds) override;
	// 에디터/스폰 시점 모두에서 DomeScale을 액터 스케일에 반영(에디터 뷰포트에서도 큰 돔이 보이게).
	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

	// 별을 그리는 구 메시(루트). 기본 엔진 Sphere를 크게 키워 사용. 머티리얼은 Two-Sided라야 안쪽이 보인다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Star Dome")
	TObjectPtr<UStaticMeshComponent> DomeMesh;

	// 돔 반경 배율. 엔진 Sphere 반경 50cm 기준 → 반경 = 50 × DomeScale (cm). 씬의 가장 먼 지형보다 충분히
	// 커야 원거리 오브젝트를 가리지 않는다. L_Planet extent ≈ 1km(half 504m) → 1500 ≈ 반경 750m로 감쌈.
	// far clip은 무한(reversed-Z)이라 더 키워도 렌더됨(PIE에서 튜닝).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star Dome", meta = (ClampMin = "1.0"))
	float DomeScale = 1500.0f;

	// 카메라 추적 on/off. 끄면 제자리 고정(에디터 미리보기 등).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star Dome")
	bool bFollowCamera = true;
};
