// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OJJ_DayNightController.generated.h"

class ADirectionalLight;
class UPlanetEventManagerSubsystem;

/**
 * 낮밤 사이클 — PlanetEventManagerSubsystem의 게임 내 시각에 맞춰 태양(Directional Light)을 회전시키는
 * 레벨 배치형 컨트롤러 (OJJ 소유).
 *
 * 설계 원칙:
 *  - 서브시스템(UPlanetEventManagerSubsystem) 코드는 무수정. GetDayProgress01()만 읽는다(읽기 전용 구독).
 *  - 시각→태양 Pitch 매핑은 서브시스템의 페이즈 정의에 충실:
 *      progress 0.0 = 일출(낮 시작), 0.25 = 정오, 0.5 = 일몰(밤 시작), 0.5~1.0 = 태양 지평선 아래.
 *      Pitch = -90 * sin(progress * 2π)
 *        progress 0    → sin 0   = 0   → Pitch   0° (수평, 일출)
 *        progress 0.25 → sin π/2 = 1   → Pitch -90° (빛이 수직 하강, 정오)
 *        progress 0.5  → sin π   = 0   → Pitch   0° (수평, 일몰)
 *        progress 0.75 → sin 3π/2= -1  → Pitch +90° (빛이 위로, 한밤 — 태양은 지평선 아래)
 *  - 전환 델리게이트(OnDayPhaseChanged)는 사이클당 2회뿐이라 연속 회전엔 부적합 → Tick 폴링.
 *  - 회전 속도는 서브시스템 설정(720초/사이클 기본)에서 360°/720초 = 0.5°/s로 충분히 부드러움.
 *
 * ⚠️ 멀티플레이어: 시간 소스(WorldSubsystem)가 현재 비복제(서버/클라 독립)다. 본 컨트롤러는 회전을
 *    각 클라에서 로컬 계산하는 시각 효과로만 동작하므로, 시간 소스가 동기화되지 않으면 클라마다
 *    낮밤이 어긋날 수 있다 — 서브시스템 소유자가 DaySeconds를 복제하면 자동으로 정합해진다.
 *
 * 와이어링: SunLight 레퍼런스를 레벨의 Directional Light로 지정해야 동작(미지정 시 경고 1회 후 무동작).
 *    하늘색/앰비언트까지 따라오게 하려면 해당 Directional Light를 SkyAtmosphere의 "Atmosphere Sun Light"로
 *    연결(에디터 체크). 본 컨트롤러는 태양 Pitch만 제어한다.
 */
UCLASS()
class WANTED_FACTORY_API AOJJ_DayNightController : public AActor
{
	GENERATED_BODY()

public:
	AOJJ_DayNightController();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	// 회전 대상 태양광. 레벨의 Directional Light를 지정(필수). 미지정 시 경고 1회 후 무동작.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Night")
	TObjectPtr<ADirectionalLight> SunLight;

	// 마스터 스위치. false면 회전을 적용하지 않는다(레벨에서 끄고 켜기).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Night")
	bool bEnabled = true;

	// 해 뜨는 방향(방위각). 레벨에서 태양이 뜨고 지는 축을 조정. Pitch만 시간으로 돌고 Yaw는 고정.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Night")
	float SunYaw = 0.0f;

	// 디버그/미리보기용 진행률 오버라이드. -1 = 비활성(실제 시각 사용). 0~1이면 그 시각으로 태양을 고정
	// (예: 0.25 정오, 0.5 일몰). 에디터/PIE에서 특정 시각 라이팅 확인용.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Night|Debug", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float DebugProgressOverride = -1.0f;

private:
	// 시간 소스. BeginPlay에서 캐시(WorldSubsystem이라 월드 수명과 동일). 미존재 월드 가드.
	TWeakObjectPtr<UPlanetEventManagerSubsystem> EventManager;

	// SunLight 미지정 경고를 매 프레임 도배하지 않도록 1회만 출력.
	bool bWarnedMissingLight = false;

	// 직전 프레임에 적용한 회전. 실질 변화가 없으면 SetActorRotation을 건너뛴다(디버그 고정 등에서 불필요한
	// 트랜스폼 갱신 방지). 첫 프레임은 반드시 적용되도록 도달 불가능한 값으로 초기화.
	float LastAppliedPitch = TNumericLimits<float>::Max();
	float LastAppliedYaw = TNumericLimits<float>::Max();

	// progress(0~1)를 태양 Pitch(도)로 변환. Pitch = -90 * sin(progress * 2π).
	static float ProgressToSunPitch(float Progress01);

	// 현재 적용할 진행률(디버그 오버라이드 우선, 아니면 서브시스템 폴링). 소스 없으면 false 반환.
	bool ResolveProgress(float& OutProgress01) const;
};
