// Fill out your copyright notice in the Description page of Project Settings.

#include "OJJ_DayNightController.h"

#include "Components/LightComponent.h"
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

	// MoonLight가 지정됐는데 Movable이 아니면 런타임 회전/강도 변경이 조용히 무시된다 → 1회 경고(디버깅 도움).
	if (MoonLight)
	{
		if (const ULightComponent* MoonComp = MoonLight->GetLightComponent())
		{
			if (MoonComp->Mobility != EComponentMobility::Movable)
			{
				LOG_OJJ_W(TEXT("MoonLight의 Mobility가 Movable이 아닙니다. 런타임 달빛 회전/강도 변경이 반영되지 않습니다."));
			}
		}
	}

	// 디버그 오버라이드가 켜진 채 출하/실행되면 태양이 고정돼 낮밤이 흐르지 않는다 → 멈춘 태양의 원인을
	// 즉시 찾게 해주는 안전망. 출하 전 -1로 되돌릴 것.
	if (DebugProgressOverride >= 0.0f)
	{
		LOG_OJJ_W(TEXT("디버그 오버라이드 활성 상태로 실행 중 — 낮밤이 흐르지 않습니다(DebugProgressOverride=%.2f, -1로 되돌리세요)."), DebugProgressOverride);
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

	const float SunPitch = ProgressToSunPitch(Progress01);
	ApplySunRotation(SunPitch);
	ApplyMoon(SunPitch);
}

void AOJJ_DayNightController::ApplySunRotation(float SunPitch)
{
	// 실질 변화가 없으면 트랜스폼 갱신 생략(디버그 고정/정지 시각 등).
	if (FMath::IsNearlyEqual(SunPitch, LastSunPitch) && FMath::IsNearlyEqual(SunYaw, LastSunYaw))
	{
		return;
	}

	SunLight->SetActorRotation(FRotator(SunPitch, SunYaw, 0.0f));
	LastSunPitch = SunPitch;
	LastSunYaw = SunYaw;
}

void AOJJ_DayNightController::ApplyMoon(float SunPitch)
{
	if (!MoonLight)
	{
		return; // 달빛은 선택 기능 — 미지정이면 조용히 skip(태양만 동작).
	}

	// 태양과 180° 반대 위상: 태양이 지평선 위(SunPitch<0, 낮)면 달은 아래, 태양이 아래(밤)면 달은 위.
	const float MoonPitch = -SunPitch;

	// 밤(SunPitch>0 = 태양이 지평선 아래)일 때만 점등. 일몰 순간 SunPitch=0 → NightFactor=0 → 강도 0에서
	// 연속 시작(점프 없음). TwilightBlend 구간에 걸쳐 0↔MoonIntensity 선형 페이드. 0 division 가드.
	const float Denom = FMath::Max(KINDA_SMALL_NUMBER, 90.0f * TwilightBlend);
	const float NightFactor = FMath::Clamp(SunPitch / Denom, 0.0f, 1.0f);
	const float TargetIntensity = MoonIntensity * NightFactor;

	// 회전: skip-if-unchanged(달 전용 캐시 — 태양 Yaw 갱신과 무관하게 판단).
	if (!FMath::IsNearlyEqual(MoonPitch, LastMoonPitch) || !FMath::IsNearlyEqual(SunYaw, LastMoonYaw))
	{
		MoonLight->SetActorRotation(FRotator(MoonPitch, SunYaw, 0.0f));
		LastMoonPitch = MoonPitch;
		LastMoonYaw = SunYaw;
	}

	// 강도: skip-if-unchanged. 컴포넌트가 Movable이어야 런타임 반영됨(체크리스트 참조). 컴포넌트를 얻지
	// 못하면 캐시를 갱신하지 않아 다음 프레임에 재시도.
	if (!FMath::IsNearlyEqual(TargetIntensity, LastMoonIntensity))
	{
		if (ULightComponent* MoonComp = MoonLight->GetLightComponent())
		{
			MoonComp->SetIntensity(TargetIntensity);
			LastMoonIntensity = TargetIntensity;
		}
	}
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
	// progress 0=0시(+90°), 0.25=6시(0°), 0.5=12시(-90°), 0.75=18시(0°).
	return 90.0f * FMath::Cos(Progress01 * UE_TWO_PI);
}
