// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MachineBase.h"
#include "PlanetEventManagerSubsystem.h" // EPlanetEventType (UFUNCTION 핸들러 파라미터) — 매니저는 tower.h를 include 안 해 순환 없음.
#include "OJJ_ProtectionTower.generated.h"

class UStaticMeshComponent;

/**
 * 자기폭풍(MagneticStorm) 전용 차폐장(Shield Generator) (OJJ 소유).
 *
 * 본 액터는 "차폐장"으로서 자신의 위치/반경/활성 상태만 소유한다. 실제 보호 판정
 * (어떤 머신이 어느 차폐장 반경 안에 있는가)은 PlanetEventManagerSubsystem이 거리 기반
 * (IsMachineShieldedFromMagneticStorm 등)으로 수행한다 — 이찬과 합의된 구조.
 *
 * 따라서 오버랩/콜리전/태그 관리는 하지 않는다. BeginPlay에서 이벤트 매니저에 자신을
 * 차폐장으로 등록(Register)하고, EndPlay에서 등록 해제(Unregister)할 뿐이다.
 *
 * ⚠️ 자기폭풍의 생산효율 저하 전용이다. SandStorm 등 다른 이벤트(내구도 데미지 등)는
 *    막지 않는다 — 판정 측에서 MagneticStorm 분기에만 차폐를 적용하기 때문.
 *
 * 상속: AMachineBase 파생 — 빌드 그리드 배치 시스템(OJJ_BuildController/OJJ_Grid)에
 *   머신으로 올리기 위함(APowerGridNode 패턴). 단 입출력 포트 0·전력 불필요·아이템 수신
 *   불가로 "비-생산 머신"이다(컨베이어 endpoint 제외는 GetInput/OutputPortCount==0 가드로 처리).
 *   루트/메시는 AMachineBase가 생성한 Root + MeshComponent를 그대로 사용한다(자체 생성 금지).
 */
UCLASS()
class WANTED_FACTORY_API AOJJ_ProtectionTower : public AMachineBase
{
	GENERATED_BODY()

public:
	AOJJ_ProtectionTower();

	// 비-생산 머신: 아이템을 받지 않는다(컨베이어/직접 투입 모두 거부) — APowerGridNode 패턴.
	virtual bool AddItem(FName ItemID, int32 Count) override;
	virtual bool CanReceiveConveyorItem(FName ItemID, int32 Count = 1) const override;

	// 차폐 반경(언리얼 단위). 기본 700 = 셀 7칸. 거리 판정은 이벤트 매니저가 이 값을 조회.
	UFUNCTION(BlueprintPure, Category = "Shield")
	float GetShieldRadius() const { return ShieldRadius; }

	// 차폐장 활성 여부. 수동 비활성(bIsShieldActive=false)이거나 파손(내구도 0, bDisableWhenBroken)
	// 이면 비활성 → 판정 측(PlanetEventManager)이 무시. 머신 전환으로 내구도가 생겼으므로(SandStorm
	// 데미지 등) 파손 시 차폐 중단 — APowerGridNode::IsPowerGridActive와 동일 패턴.
	UFUNCTION(BlueprintPure, Category = "Shield")
	bool IsShieldActive() const { return bIsShieldActive && !(isBroken() && bDisableWhenBroken); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 차폐 반경(언리얼 단위). 기본 1200 = 셀 12칸. 보호 판정(IsMachineShieldedFromMagneticStorm)·돔 스케일 공용 단일 출처.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield", meta = (ClampMin = "0.0"))
	float ShieldRadius = 1200.0f;

	// 차폐장 활성 상태.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield")
	bool bIsShieldActive = true;

	// PIE에서 차폐 반경을 디버그 구체로 표시(에디터 비주얼은 최소화 — 보고 참조).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield|Debug")
	bool bShowDebugRadius = false;

	// [#353] 에너지 실드 돔 비주얼. 루트에 부착, 메시=Sphere(기본 반경 50uu)·머티리얼=MI_EnergyShield.
	// 반경은 BeginPlay에서 GetShieldRadius()/50으로 스케일. 기본 숨김 — 실제 표시(활성/자기폭풍)는 후속.
	// NoCollision(거리 판정은 PlanetEventManager 전담). 동적 MID 미생성(상태등 MID 저장버그 교훈).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shield")
	TObjectPtr<UStaticMeshComponent> ShieldDomeComponent;

	// [#353] 돔 육안 디버그 — true면 BeginPlay에서 돔 visibility 강제 ON(반경 스케일 확인용). 기본 false(원복 불요).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield|Debug")
	bool bShowShieldDome = false;

private:
	// PlanetEventManagerSubsystem에 차폐장 등록/해제(BeginPlay/EndPlay에서 호출).
	void RegisterToEventManager();
	void UnregisterFromEventManager();

	// [#353] 자기폭풍 이벤트 구독 핸들러 — 공용 이벤트 델리게이트(MagneticStorm/SandStorm 공유)라 EventType로
	// MagneticStorm만 필터. DYNAMIC multicast(AddDynamic)라 UFUNCTION 필수.
	UFUNCTION()
	void HandleEventStarted(EPlanetEventType EventType, float Severity);
	UFUNCTION()
	void HandleEventEnded(EPlanetEventType EventType);

	// [#353] 돔 표시 단일 진입점 — bShowShieldDome(디버그) OR (자기폭풍 활성 && IsShieldActive()).
	// ⚠️ 돔 SetVisibility는 반드시 이 함수만 경유(분기 산재 방지).
	void UpdateDomeVisibility();

	// [#353] 자기폭풍 활성 캐시 — 이벤트 핸들러/BeginPlay 초기 동기화가 세팅, UpdateDomeVisibility가 읽음.
	bool bMagneticStormActive = false;
};
