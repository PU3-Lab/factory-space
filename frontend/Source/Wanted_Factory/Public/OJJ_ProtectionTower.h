// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OJJ_ProtectionTower.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UOJJ_ProtectionSubsystem;

/**
 * 자기폭풍(MagneticStorm) 전용 보호 건물 (OJJ 소유).
 *
 * USphereComponent(ProtectionRange) 범위 안에 들어온 머신(AMachineBase / ADummyMachineBase)을
 * UOJJ_ProtectionSubsystem에 등록하여 "MagneticProtected" 태그가 부여되도록 한다.
 *
 * ⚠️ 자기폭풍(MagneticStorm)의 생산효율 저하 전용이다. SandStorm 등 다른 이벤트(내구도
 *    데미지 등)는 막지 않는다 — 태그 가드는 이찬 이벤트 적용부의 MagneticStorm 분기에서만
 *    소비될 예정이기 때문.
 *
 * 상속 판단: BuildController는 TSubclassOf<AMachineBase>를 스폰하지만, 본 타워는 레시피/포트/
 * 인벤토리/상태머신/그리드 점유 등 머신 도메인 로직이 전혀 필요 없고 오히려 충돌하므로
 * AActor 직속으로 둔다(상세 근거는 작업 보고 참조). 배치는 에디터/별도 빌드 경로로 처리.
 */
UCLASS()
class WANTED_FACTORY_API AOJJ_ProtectionTower : public AActor
{
	GENERATED_BODY()

public:
	AOJJ_ProtectionTower();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 루트 겸 비주얼. 메시는 BP에서 지정(임시 큐브 가능).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Protection")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	// 보호 범위 오버랩 감지용 구체.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Protection")
	TObjectPtr<USphereComponent> ProtectionRange;

	// 보호 반경(언리얼 단위). 기본 500 = 셀 5칸.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Protection", meta = (ClampMin = "0.0"))
	float ProtectionRadius = 500.0f;

	UFUNCTION()
	void OnRangeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnRangeEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
	// 이 타워가 현재 보호 중인 머신 추적(EndPlay/철거 시 일괄 해제용).
	// 약참조라 파괴된 머신은 자동 무효화된다. GC 도달성 불필요 → UPROPERTY 미사용.
	TSet<TWeakObjectPtr<AActor>> ProtectedMachines;

	UOJJ_ProtectionSubsystem* GetProtectionSubsystem() const;

	// OtherActor가 보호 대상 머신인지 판별(AMachineBase 또는 ADummyMachineBase, 자기 자신 제외).
	bool IsProtectableMachine(AActor* OtherActor) const;
};
