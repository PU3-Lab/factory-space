// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OJJ_HologramPathBuildUpComponent.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * [#3 확장] 컨베이어/파이프(ISM 세그먼트) "시작→끝 길이 방향" 홀로그램 빌드업.
 * 머신과 동일한 (A) 프록시 방식: 실제 세그먼트 ISM은 무수정(진짜 룩 100% 표시), 소스 ISM의 인스턴스를 전부
 * 복제한 프록시 ISM에 M_Hologram_BuildUp_Path(Translucent)을 입혀 경계(Progress) 앞쪽(미완)을 홀로그램으로
 * 덮고, 뒤(차오른 쪽)는 투명 → 실제 노출. 머티리얼이 월드위치를 경로축(PathStart/PathDir/PathLength)에 투영해
 * 시작→끝 마스크를 만든다(per-instance 커스텀데이터 미사용 → 파이프 액체 데이터 무접촉).
 * Progress 0→1 후 프록시 제거(실제는 안 건드려 원복 불필요).
 *
 * ⚠️ 정리 안전망: ① Progress=1 완료 ② Duration 초과 timeout ③ EndPlay. (프록시 제거만 — 실제 머티 무손상)
 * ⚠️ PathHologramMaterial 미지정이면 무동작(배치 정상).
 */
UCLASS(ClassGroup = (OJJ), meta = (BlueprintSpawnableComponent))
class WANTED_FACTORY_API UOJJ_HologramPathBuildUpComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOJJ_HologramPathBuildUpComponent();

	// 소스 ISM들을 복제한 프록시 ISM 생성 + Progress 0→1. PathStart/End는 월드 좌표(경로 시작/끝).
	void StartBuildUp(const TArray<UInstancedStaticMeshComponent*>& SourceISMs, const FVector& PathStart, const FVector& PathEnd);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hologram")
	TObjectPtr<UMaterialInterface> PathHologramMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hologram", meta = (ClampMin = "0.01"))
	float Duration = 1.0f;

	// z-fighting 방지용 프록시 인스턴스 스케일 배수(실제보다 약간 크게). 머티리얼 WPO로 띄우면 1.0.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hologram", meta = (ClampMin = "1.0"))
	float ProxyScaleMultiplier = 1.02f;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MID;

	// 생성한 프록시 ISM들(소스 ISM 1개당 1개). 완료/정리 시 DestroyComponent.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Proxies;

	float Elapsed = 0.0f;
	bool bRunning = false;

	// 프록시 제거 + 정리 + 틱 off (완료/timeout/EndPlay 공용 — 멱등). 실제 ISM은 안 건드림.
	void Finish();
};
