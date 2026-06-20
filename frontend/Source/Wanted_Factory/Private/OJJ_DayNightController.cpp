// Fill out your copyright notice in the Description page of Project Settings.

#include "OJJ_DayNightController.h"

#include "Components/LightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
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

	if (MagneticStormPostProcessVolume)
	{
		MagneticStormPostProcessVolume->BlendWeight = 0.0f;
	}
	if (SandStormPostProcessVolume)
	{
		SandStormPostProcessVolume->BlendWeight = 0.0f;
	}

	// 시간 소스 캐시. WorldSubsystem이 없는 월드(테스트 맵 등)에서도 크래시하지 않도록 가드.
	if (const UWorld* World = GetWorld())
	{
		EventManager = World->GetSubsystem<UPlanetEventManagerSubsystem>();
	}

	if (EventManager.IsValid())
	{
		EventManager->OnPlanetEventStarted.AddDynamic(this, &AOJJ_DayNightController::HandlePlanetEventStarted);
		EventManager->OnPlanetEventEnded.AddDynamic(this, &AOJJ_DayNightController::HandlePlanetEventEnded);

		const FPlanetEventState EventState = EventManager->GetEventState();
		VisualEventType = EventState.Type;
		VisualEventSeverity = EventState.Severity;
		CurrentEventVisualAlpha = EventState.Type == EPlanetEventType::None ? 0.0f : EventState.Severity;
	}

	CacheBaseVisualState();
	if (VisualEventType != EPlanetEventType::None)
	{
		SpawnEventNiagara(VisualEventType);
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

void AOJJ_DayNightController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EventManager.IsValid())
	{
		EventManager->OnPlanetEventStarted.RemoveDynamic(this, &AOJJ_DayNightController::HandlePlanetEventStarted);
		EventManager->OnPlanetEventEnded.RemoveDynamic(this, &AOJJ_DayNightController::HandlePlanetEventEnded);
	}

	ClearEventNiagara();

	if (MagneticStormPostProcessVolume)
	{
		MagneticStormPostProcessVolume->BlendWeight = 0.0f;
	}
	if (SandStormPostProcessVolume)
	{
		SandStormPostProcessVolume->BlendWeight = 0.0f;
	}

	Super::EndPlay(EndPlayReason);
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
	ApplySunRotation(SunPitch, DeltaSeconds);
	ApplyMoon(CurrentSunPitch);
	ApplyStars(CurrentSunPitch);
	ApplyPlanetEventVisuals(DeltaSeconds);
}

void AOJJ_DayNightController::ApplySunRotation(float TargetSunPitch, float DeltaSeconds)
{
	// 실질 변화가 없으면 트랜스폼 갱신 생략(디버그 고정/정지 시각 등).
	if (!bHasInitializedRotation)
	{
		CurrentSunPitch = TargetSunPitch;
		bHasInitializedRotation = true;
	}
	else
	{
		CurrentSunPitch = FMath::FInterpTo(CurrentSunPitch, TargetSunPitch, DeltaSeconds, RotationInterpSpeed);
	}

	if (FMath::IsNearlyEqual(CurrentSunPitch, LastSunPitch, 0.01f) &&
		FMath::IsNearlyEqual(SunYaw, LastSunYaw, 0.01f))
	{
		return;
	}

	SunLight->SetActorRotation(FRotator(CurrentSunPitch, SunYaw, 0.0f));
	LastSunPitch = CurrentSunPitch;
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
	// 연속 시작(점프 없음). TwilightBlend 구간에 걸쳐 0↔MoonIntensity 선형 페이드.
	const float NightFactor = ComputeNightFactor(SunPitch);
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

float AOJJ_DayNightController::ComputeNightFactor(float SunPitch) const
{
	// SunPitch>0 = 태양이 지평선 아래(밤). 일몰 순간(0)부터 TwilightBlend 폭(90°×값)에 걸쳐 0→1 선형 상승.
	// 달빛/별이 공유 → 둘의 페이드 타이밍이 항상 일치. 0 division 가드.
	const float Denom = FMath::Max(KINDA_SMALL_NUMBER, 90.0f * TwilightBlend);
	return FMath::Clamp(SunPitch / Denom, 0.0f, 1.0f);
}

void AOJJ_DayNightController::ApplyStars(float SunPitch)
{
	if (!StarCollection)
	{
		return; // 별은 선택 기능 — MPC 미지정이면 조용히 skip(태양/달만 동작).
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 낮 0 / 밤 1(달빛과 동일한 박명 페이드)에 상한을 곱한 목표 강도. 낮엔 0이라 별이 완전히 사라진다.
	const float TargetStarIntensity = MaxStarIntensity * ComputeNightFactor(SunPitch);

	// skip-if-unchanged: 강도 변화가 미미하면 MPC 갱신 생략(매 프레임 머티리얼 파라미터 push 방지).
	if (FMath::IsNearlyEqual(TargetStarIntensity, LastStarIntensity, 0.001f))
	{
		return;
	}

	UKismetMaterialLibrary::SetScalarParameterValue(World, StarCollection, StarIntensityParam, TargetStarIntensity);
	LastStarIntensity = TargetStarIntensity;
}

void AOJJ_DayNightController::HandlePlanetEventStarted(EPlanetEventType EventType, float Severity)
{
	VisualEventType = EventType;
	VisualEventSeverity = FMath::Clamp(Severity, 0.0f, 1.0f);
	SpawnEventNiagara(EventType);
}

void AOJJ_DayNightController::HandlePlanetEventEnded(EPlanetEventType EventType)
{
	VisualEventType = EPlanetEventType::None;
	VisualEventSeverity = 0.0f;
	ClearEventNiagara();
}

void AOJJ_DayNightController::CacheBaseVisualState()
{
	if (SunLight)
	{
		if (const ULightComponent* SunComponent = SunLight->GetLightComponent())
		{
			BaseSunLightColor = SunComponent->GetLightColor();
			BaseSunIntensity = SunComponent->Intensity;
		}
	}

	if (EventFog)
	{
		if (const UExponentialHeightFogComponent* FogComponent = EventFog->GetComponent())
		{
			BaseFogDensity = FogComponent->FogDensity;
			BaseFogInscatteringColor = FogComponent->FogInscatteringLuminance;
		}
	}
}

void AOJJ_DayNightController::ApplyPlanetEventVisuals(float DeltaSeconds)
{
	const float TargetAlpha = VisualEventType == EPlanetEventType::None ? 0.0f : VisualEventSeverity;
	CurrentEventVisualAlpha = FMath::FInterpTo(CurrentEventVisualAlpha, TargetAlpha, DeltaSeconds, EventVisualInterpSpeed);

	FLinearColor TargetSunColor = BaseSunLightColor;
	FLinearColor TargetFogColor = BaseFogInscatteringColor;
	float TargetFogDensity = BaseFogDensity;
	float TargetSunIntensity = BaseSunIntensity;
	FVector TargetCameraColorScale = FVector(1.0f, 1.0f, 1.0f);

	if (VisualEventType == EPlanetEventType::MagneticStorm)
	{
		TargetSunColor = FLinearColor::LerpUsingHSV(BaseSunLightColor, MagneticStormSunColor, CurrentEventVisualAlpha);
		TargetFogColor = FLinearColor::LerpUsingHSV(BaseFogInscatteringColor, MagneticStormFogColor, CurrentEventVisualAlpha);
		TargetFogDensity = BaseFogDensity + MagneticStormFogDensityBonus * CurrentEventVisualAlpha;
		TargetSunIntensity = FMath::Lerp(BaseSunIntensity, BaseSunIntensity * MagneticStormSunIntensityScale, CurrentEventVisualAlpha);
		TargetCameraColorScale = FMath::Lerp(FVector(1.0f, 1.0f, 1.0f), MagneticStormCameraColorScale, CurrentEventVisualAlpha);
	}
	else if (VisualEventType == EPlanetEventType::SandStorm)
	{
		TargetSunColor = FLinearColor::LerpUsingHSV(BaseSunLightColor, SandStormSunColor, CurrentEventVisualAlpha);
		TargetFogColor = FLinearColor::LerpUsingHSV(BaseFogInscatteringColor, SandStormFogColor, CurrentEventVisualAlpha);
		TargetFogDensity = BaseFogDensity + SandStormFogDensityBonus * CurrentEventVisualAlpha;
		TargetSunIntensity = FMath::Lerp(BaseSunIntensity, BaseSunIntensity * SandStormSunIntensityScale, CurrentEventVisualAlpha);
		TargetCameraColorScale = FMath::Lerp(FVector(1.0f, 1.0f, 1.0f), SandStormCameraColorScale, CurrentEventVisualAlpha);
	}

	if (MagneticStormPostProcessVolume)
	{
		MagneticStormPostProcessVolume->BlendWeight =
			VisualEventType == EPlanetEventType::MagneticStorm ? CurrentEventVisualAlpha : 0.0f;
	}
	if (SandStormPostProcessVolume)
	{
		SandStormPostProcessVolume->BlendWeight =
			VisualEventType == EPlanetEventType::SandStorm ? CurrentEventVisualAlpha : 0.0f;
	}

	if (SunLight)
	{
		if (ULightComponent* SunComponent = SunLight->GetLightComponent())
		{
			SunComponent->SetLightColor(TargetSunColor);
			SunComponent->SetIntensity(TargetSunIntensity);
		}
	}

	if (EventFog && EventFog->GetComponent())
	{
		EventFog->GetComponent()->SetFogInscatteringColor(TargetFogColor);
		EventFog->GetComponent()->SetFogDensity(TargetFogDensity);
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();
		if (!PlayerController || !PlayerController->PlayerCameraManager)
		{
			continue;
		}

		PlayerController->PlayerCameraManager->SetDesiredColorScale(TargetCameraColorScale, 0.0f);
	}
}

void AOJJ_DayNightController::SpawnEventNiagara(EPlanetEventType EventType)
{
	ClearEventNiagara();

	UNiagaraSystem* SystemToSpawn = nullptr;
	FVector SpawnOffset = FVector::ZeroVector;
	FVector WorldOffset = FVector::ZeroVector;
	FVector WorldScale = FVector(1.0f, 1.0f, 1.0f);
	if (EventType == EPlanetEventType::MagneticStorm)
	{
		SystemToSpawn = MagneticStormNiagara;
		SpawnOffset = MagneticStormNiagaraOffset;
		WorldOffset = MagneticStormNiagaraWorldOffset;
		WorldScale = MagneticStormNiagaraWorldScale;
	}
	else if (EventType == EPlanetEventType::SandStorm)
	{
		SystemToSpawn = SandStormNiagara;
		SpawnOffset = SandStormNiagaraOffset;
		WorldOffset = SandStormNiagaraWorldOffset;
		WorldScale = SandStormNiagaraWorldScale;
	}

	if (!SystemToSpawn)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (bSpawnPlanetEventNiagaraInWorld)
	{
		ActiveEventNiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			SystemToSpawn,
			GetActorLocation() + WorldOffset,
			FRotator::ZeroRotator,
			WorldScale,
			true,
			true,
			ENCPoolMethod::None,
			false);
		return;
	}

	APlayerController* PlayerController = World->GetFirstPlayerController();
	APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!PlayerPawn || !PlayerPawn->GetRootComponent())
	{
		return;
	}

	ActiveEventNiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
		SystemToSpawn,
		PlayerPawn->GetRootComponent(),
		NAME_None,
		SpawnOffset,
		FRotator::ZeroRotator,
		EAttachLocation::KeepRelativeOffset,
		false);
}

void AOJJ_DayNightController::ClearEventNiagara()
{
	if (ActiveEventNiagaraComponent)
	{
		ActiveEventNiagaraComponent->DestroyComponent();
		ActiveEventNiagaraComponent = nullptr;
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
