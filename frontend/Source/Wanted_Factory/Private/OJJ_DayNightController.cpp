// Fill out your copyright notice in the Description page of Project Settings.

#include "OJJ_DayNightController.h"

#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "PlanetEventManagerSubsystem.h"
#include "Wanted_Factory.h"

AOJJ_DayNightController::AOJJ_DayNightController()
{
	// 시각에 맞춰 매 프레임 태양 Pitch를 폴링/적용한다(연속 회전).
	PrimaryActorTick.bCanEverTick = true;
}

void AOJJ_DayNightController::BeginPlay()
{
	Super::BeginPlay();

	// 시간 소스 캐시. WorldSubsystem이 없는 월드(테스트 맵 등)에서도 크래시하지 않도록 가드.
	if (const UWorld* World = GetWorld())
	{
		EventManager = World->GetSubsystem<UPlanetEventManagerSubsystem>();
	}

	if (!EventManager.IsValid())
	{
		LOG_OJJ_W(TEXT("PlanetEventManagerSubsystem을 찾지 못했습니다. 디버그 오버라이드 외에는 태양 회전이 동작하지 않습니다."));
	}
}

void AOJJ_DayNightController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bEnabled)
	{
		return;
	}

	if (!SunLight)
	{
		if (!bWarnedMissingLight)
		{
			LOG_OJJ_W(TEXT("SunLight가 지정되지 않았습니다. 레벨에서 Directional Light를 SunLight에 연결하세요."));
			bWarnedMissingLight = true;
		}
		return;
	}

	float Progress01 = 0.0f;
	if (!ResolveProgress(Progress01))
	{
		return; // 시간 소스도 없고 디버그 오버라이드도 비활성 → 무동작.
	}

	const float Pitch = ProgressToSunPitch(Progress01);

	// 실질 변화가 없으면 트랜스폼 갱신 생략(디버그 고정/정지 시각 등).
	if (FMath::IsNearlyEqual(Pitch, LastAppliedPitch) && FMath::IsNearlyEqual(SunYaw, LastAppliedYaw))
	{
		return;
	}

	SunLight->SetActorRotation(FRotator(Pitch, SunYaw, 0.0f));
	LastAppliedPitch = Pitch;
	LastAppliedYaw = SunYaw;
}

bool AOJJ_DayNightController::ResolveProgress(float& OutProgress01) const
{
	// 디버그 오버라이드(0~1)가 켜져 있으면 실제 시각보다 우선 — 특정 시각 라이팅 미리보기용.
	if (DebugProgressOverride >= 0.0f)
	{
		OutProgress01 = FMath::Clamp(DebugProgressOverride, 0.0f, 1.0f);
		return true;
	}

	if (!EventManager.IsValid())
	{
		return false;
	}

	OutProgress01 = EventManager->GetDayProgress01();
	return true;
}

float AOJJ_DayNightController::ProgressToSunPitch(float Progress01)
{
	// progress 0=일출(0°), 0.25=정오(-90°), 0.5=일몰(0°), 0.75=한밤(+90°, 지평선 아래).
	return -90.0f * FMath::Sin(Progress01 * UE_TWO_PI);
}
