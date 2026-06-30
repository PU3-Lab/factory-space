// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OJJ_DemolishEffectActor.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UOJJ_HologramBuildUpComponent;

/**
 * [철거 빌드다운] 철거 시 역방향(위→아래) 홀로그램 디졸브를 재생하는 자체소멸 프록시 액터.
 *
 * 철거 대상 액터는 즉시 Destroy된다(그리드 부기/장부 해제는 본체 그대로). 빌드업 컴포넌트는 대상 액터에 붙어
 * 대상이 사라지면 같이 사라지므로, 연출만 이 독립 액터로 분리한다 — 빌드업과 동일한
 * UOJJ_HologramBuildUpComponent(bDissolveOut + 색 오버라이드)로 대상 메시(단일 SMC/ISM)를 복제해 디졸브 후
 * 자신을 Destroy(bDestroyOwnerOnFinish). 그리드 장부 미등록(연출 전용 — 점유 안 함, 재배치 영향 0).
 * 머티리얼/메시 무효면 즉시 self Destroy(잔존 0).
 */
UCLASS()
class WANTED_FACTORY_API AOJJ_DemolishEffectActor : public AActor
{
	GENERATED_BODY()

public:
	AOJJ_DemolishEffectActor();

	// 철거 연출 시작 — SourceMesh(단일 SMC/ISM)를 복제해 역방향 빌드다운 재생 후 자체 Destroy.
	// 호출자(BuildController)가 대상 Destroy '직전'에 호출 → 메시/트랜스폼/인스턴스를 동기 복제(대상 파괴와 무관히 연출 지속).
	void StartBuildDown(UStaticMeshComponent* SourceMesh, UMaterialInterface* HologramMaterial, const FLinearColor& Color, float Duration);

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> Root;

	UPROPERTY()
	TObjectPtr<UOJJ_HologramBuildUpComponent> Effect;
};
