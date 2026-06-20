// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlanetEventManagerSubsystem.h"
#include "OJJ_DayNightController.generated.h"

class ADirectionalLight;
class AExponentialHeightFog;
class APostProcessVolume;
class UPlanetEventManagerSubsystem;
class UMaterialParameterCollection;
class UNiagaraComponent;
class UNiagaraSystem;

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
 *    연결(에디터 체크). 본 컨트롤러는 태양 Pitch를 제어하고, 선택적으로 달빛(MoonLight)을 함께 제어한다.
 *
 * 밤 가시성(선택): MoonLight(별도 Directional Light)를 지정하면 태양과 180° 반대 위상으로 회전시키고,
 *    밤(태양이 지평선 아래)일 때만 MoonIntensity로 은은한 달빛을 켠다(낮엔 강도 0). 일몰 직후 TwilightBlend
 *    구간에 걸쳐 0↔MoonIntensity로 선형 페이드해 점프를 없앤다(일몰 순간 SunPitch=0 → 강도 0에서 연속 시작).
 *    ⚠️ MoonLight는 하늘에 영향을 주면 안 되므로 Atmosphere Sun Light 체크를 해제할 것(태양만 하늘 담당).
 *    런타임 강도/회전 반영을 위해 Mobility=Movable 필요. 미지정이면 달빛 로직 전체 skip(태양만 동작).
 */
UCLASS()
class WANTED_FACTORY_API AOJJ_DayNightController : public AActor
{
	GENERATED_BODY()

public:
	AOJJ_DayNightController();

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Night", meta = (ClampMin = "0.1"))
	float RotationInterpSpeed = 1.0f;

	// 디버그/미리보기용 진행률 오버라이드. -1 = 비활성(실제 시각 사용). 0~1이면 그 시각으로 태양을 고정
	// (예: 0.25 정오, 0.5 일몰, 0.75 한밤). 에디터/PIE에서 특정 시각 라이팅 확인용.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Night|Debug", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float DebugProgressOverride = -1.0f;

	// 밤 달빛(선택). 지정 시 태양과 반대 위상으로 회전 + 밤에만 점등. null이면 달빛 로직 전체 skip.
	// ⚠️ 하늘에 영향 주지 않도록 이 라이트는 Atmosphere Sun Light 체크를 해제할 것. Mobility=Movable 필요.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Night|Moon")
	TObjectPtr<ADirectionalLight> MoonLight;

	// 한밤(태양이 지평선 아래로 충분히 내려간 상태)에서의 달빛 강도(절대값, lux). 0이면 사실상 달빛 꺼짐.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Night|Moon", meta = (ClampMin = "0.0"))
	float MoonIntensity = 1.0f;

	// 박명 페이드 폭. 태양이 지평선 아래로 (90°×이 값)만큼 내려가면 달빛이 최대(MoonIntensity)에 도달하고
	// 그 사이는 선형 페이드. 0.05 ≈ 4.5° → 일몰 직후 부드럽게 점등. 0이면 일몰 순간 즉시 최대(점프).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Night|Moon", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float TwilightBlend = 0.05f;

	// 밤하늘 별 강도 연동(선택). 절차적 별 머티리얼이 참조하는 Material Parameter Collection을 지정하면,
	// 매 틱 NightFactor(낮 0 / 밤 1, 달빛과 동일한 박명 페이드)를 StarIntensityParam 스칼라에 써준다.
	// 미지정이면 별 로직 전체 skip(태양/달만 동작). 돔 메시 + 절차적 별 머티리얼은 에디터 측 에셋.
	// ⚠️ 별 돔은 카메라를 따라다니되 회전 고정 + Collision=NoCollision(빌드 레이캐스트 통과)으로 둘 것.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Night|Stars")
	TObjectPtr<UMaterialParameterCollection> StarCollection;

	// StarCollection 안에서 별 전체 강도를 받는 스칼라 파라미터 이름. 머티리얼이 같은 이름으로 샘플해야 함.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Night|Stars")
	FName StarIntensityParam = TEXT("StarIntensity");

	// 한밤(NightFactor=1)에서의 별 강도 상한. 0~1 기준. 머티리얼 emissive 배율과 곱해진다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Night|Stars", meta = (ClampMin = "0.0"))
	float MaxStarIntensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Visual")
	TObjectPtr<AExponentialHeightFog> EventFog;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Visual", meta = (ClampMin = "0.1"))
	float EventVisualInterpSpeed = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Visual")
	FLinearColor MagneticStormSunColor = FLinearColor(0.42f, 0.72f, 0.82f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Visual")
	FLinearColor SandStormSunColor = FLinearColor(1.0f, 0.72f, 0.45f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Visual")
	FLinearColor MagneticStormFogColor = FLinearColor(0.18f, 0.34f, 0.42f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Visual")
	FLinearColor SandStormFogColor = FLinearColor(0.78f, 0.58f, 0.35f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Visual")
	FVector MagneticStormCameraColorScale = FVector(0.92f, 0.97f, 1.03f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Visual")
	FVector SandStormCameraColorScale = FVector(1.1f, 0.9f, 0.72f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Visual", meta = (ClampMin = "0.0"))
	float MagneticStormFogDensityBonus = 0.005f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Visual", meta = (ClampMin = "0.0"))
	float SandStormFogDensityBonus = 0.035f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Visual", meta = (ClampMin = "0.0"))
	float MagneticStormSunIntensityScale = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Visual", meta = (ClampMin = "0.0"))
	float SandStormSunIntensityScale = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Visual")
	TObjectPtr<UNiagaraSystem> MagneticStormNiagara;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Visual")
	TObjectPtr<UNiagaraSystem> SandStormNiagara;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Visual")
	FVector MagneticStormNiagaraOffset = FVector(20.0f, 0.0f, 70.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Visual")
	FVector SandStormNiagaraOffset = FVector(40.0f, 0.0f, 60.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Visual")
	bool bSpawnPlanetEventNiagaraInWorld = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Visual")
	FVector MagneticStormNiagaraWorldOffset = FVector(0.0f, 0.0f, 500.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Visual")
	FVector SandStormNiagaraWorldOffset = FVector(0.0f, 0.0f, 400.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Visual")
	FVector MagneticStormNiagaraWorldScale = FVector(25.0f, 25.0f, 10.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Visual")
	FVector SandStormNiagaraWorldScale = FVector(25.0f, 25.0f, 10.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Post Process")
	TObjectPtr<APostProcessVolume> MagneticStormPostProcessVolume;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Post Process")
	TObjectPtr<APostProcessVolume> SandStormPostProcessVolume;

private:
	// 시간 소스. BeginPlay에서 캐시(WorldSubsystem이라 월드 수명과 동일). 미존재 월드 가드.
	TWeakObjectPtr<UPlanetEventManagerSubsystem> EventManager;

	// SunLight 미지정 경고를 매 프레임 도배하지 않도록 1회만 출력.
	bool bWarnedMissingLight = false;

	// 직전에 적용한 태양 회전. 실질 변화가 없으면 SetActorRotation을 건너뛴다(디버그 고정 등에서 불필요한
	// 트랜스폼 갱신 방지). 첫 프레임은 반드시 적용되도록 도달 불가능한 값으로 초기화.
	float LastSunPitch = TNumericLimits<float>::Max();
	float LastSunYaw = TNumericLimits<float>::Max();

	// 직전에 적용한 달 회전/강도. 달은 SunYaw 축을 공유하지만, 태양 캐시와 갱신 순서가 엮이지 않도록
	// 전용 캐시를 둔다(태양 적용이 LastSunYaw를 먼저 갱신해 달 갱신을 가리는 버그 방지).
	float LastMoonPitch = TNumericLimits<float>::Max();
	float LastMoonYaw = TNumericLimits<float>::Max();
	float LastMoonIntensity = TNumericLimits<float>::Max();
	float CurrentSunPitch = 0.0f;
	bool bHasInitializedRotation = false;

	// 직전에 적용한 별 강도. skip-if-unchanged용(변화 없으면 MPC 갱신 생략).
	float LastStarIntensity = TNumericLimits<float>::Max();
	EPlanetEventType VisualEventType = EPlanetEventType::None;
	float VisualEventSeverity = 0.0f;
	float CurrentEventVisualAlpha = 0.0f;
	FLinearColor BaseSunLightColor = FLinearColor::White;
	float BaseSunIntensity = 10.0f;
	FLinearColor BaseFogInscatteringColor = FLinearColor::White;
	float BaseFogDensity = 0.0f;
	TObjectPtr<UNiagaraComponent> ActiveEventNiagaraComponent;

	// progress(0~1)를 태양 Pitch(도)로 변환. Pitch = -90 * sin(progress * 2π).
	static float ProgressToSunPitch(float Progress01);

	// 현재 적용할 진행률(디버그 오버라이드 우선, 아니면 서브시스템 폴링). 소스 없으면 false 반환.
	bool ResolveProgress(float& OutProgress01) const;

	// 태양 회전 적용(skip-if-unchanged 내장).
	void ApplySunRotation(float TargetSunPitch, float DeltaSeconds);

	// 달 회전+강도 적용. 태양과 반대 위상, 밤에만 점등. MoonLight 미지정이면 no-op.
	void ApplyMoon(float SunPitch);

	// 밤 정도(0=낮/일몰순간 ~ 1=한밤). SunPitch>0(태양 지평선 아래)에서 TwilightBlend 폭에 걸쳐 선형 상승.
	// 달빛과 별 강도가 공유하는 단일 정의(점프 없는 박명 페이드).
	float ComputeNightFactor(float SunPitch) const;

	// 별 강도 적용. NightFactor×MaxStarIntensity를 StarCollection의 StarIntensityParam에 기록. 미지정이면 no-op.
	void ApplyStars(float SunPitch);

	UFUNCTION()
	void HandlePlanetEventStarted(EPlanetEventType EventType, float Severity);

	UFUNCTION()
	void HandlePlanetEventEnded(EPlanetEventType EventType);

	void CacheBaseVisualState();
	void ApplyPlanetEventVisuals(float DeltaSeconds);
	void SpawnEventNiagara(EPlanetEventType EventType);
	void ClearEventNiagara();
};
