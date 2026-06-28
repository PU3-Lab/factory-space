// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OJJ_HologramBuildUpComponent.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * [#3 점진 건설] 건물 배치 시 "아래→위 홀로그램 빌드업" 연출 (머신·파운데이션 공용 — 둘 다 AActor 직속이라
 * 공통 베이스가 없어 컴포넌트로 재사용). 실제 메시는 무수정·100% 표시.
 *
 * 동작: 소스 메시를 복제한 프록시에 M_Hologram_BuildUp(Masked)을 입혀 경계(Progress) 위는 시안 홀로그램으로
 * 덮고(약간 크게 = 실제 가림), 아래는 OpacityMask로 투명 → 실제 노출. Progress 0→1이면 위 덮임 영역이
 * 위로 줄며 아래부터 드러나고, Progress=1에서 프록시를 제거한다.
 *
 * 신규 배치 전용: AOJJ_BuildController의 배치 함수(PlaceMachine/PlaceFoundationAtCursor)가 StartBuildUp을
 * 호출한다. 로드(FactorySaveSubsystem) 경로는 이 컴포넌트를 안 거치므로 로드 시 전체 건물이 디졸브되지 않는다.
 *
 * ⚠️ HologramMaterial 미지정(에디터에서 M_Hologram_BuildUp 제작 전)이면 StartBuildUp이 무동작 — 배치는 정상.
 */
UCLASS(ClassGroup = (OJJ), meta = (BlueprintSpawnableComponent))
class WANTED_FACTORY_API UOJJ_HologramBuildUpComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOJJ_HologramBuildUpComponent();

	// 빌드업 시작 — Source 메시를 복제한 프록시 생성 + Progress 0→1 애니. HologramMaterial/Source 무효면 무동작(안전).
	void StartBuildUp(UStaticMeshComponent* Source);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// M_Hologram_BuildUp (Masked). 미지정이면 효과 비활성(배치 정상). BuildController가 주입하거나 에디터 지정.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hologram")
	TObjectPtr<UMaterialInterface> HologramMaterial;

	// 빌드업 지속(초).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hologram", meta = (ClampMin = "0.01"))
	float Duration = 1.0f;

	// z-fighting 방지용 프록시 스케일 배수(실제보다 약간 크게 = 경계 위 영역을 확실히 가림). 머티리얼 WPO로 띄우면 1.0.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hologram", meta = (ClampMin = "1.0"))
	float ProxyScaleMultiplier = 1.02f;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Proxy;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MID;

	float Elapsed = 0.0f;
	bool bRunning = false;

	// 프록시/타임라인 정리 + 틱 off (완료/취소/EndPlay 공용).
	void Finish();
};
