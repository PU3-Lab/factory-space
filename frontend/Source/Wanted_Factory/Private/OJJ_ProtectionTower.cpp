// Fill out your copyright notice in the Description page of Project Settings.

#include "OJJ_ProtectionTower.h"

#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "PlanetEventManagerSubsystem.h"
#include "UObject/ConstructorHelpers.h"
#include "Wanted_Factory.h"

AOJJ_ProtectionTower::AOJJ_ProtectionTower()
{
	PrimaryActorTick.bCanEverTick = true;

	// 비-생산 머신 설정 (APowerGridNode 패턴). 루트/메시는 AMachineBase가 생성하므로 여기서 만들지 않는다.
	MachineType = TEXT("MagneticShield");
	bNeedPower = false;
	bDisableWhenBroken = true;

	// 포트가 없어 입출력 버퍼 디버그 텍스트("Input None / Output None")가 무의미 → 끔.
	bShowDebugBufferText = false;

	// GridSize and ports come from the MagneticShield MachineTable row.

	// [#353] 에너지 실드 돔 — 루트에 부착. 메시=Sphere(기본 반경 50uu), 머티리얼=MI_EnergyShield(에셋 직접:
	// OJJ_StarDome/OJJ_Foundation처럼 /Game/ ConstructorHelpers 관례). 동적 MID 미생성(상태등 MID 저장버그 교훈).
	// 충돌 없음(거리 판정은 PlanetEventManager 전담), 기본 숨김 — 표시는 BeginPlay에서 bShowShieldDome로 게이트.
	ShieldDomeComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShieldDomeComponent"));
	ShieldDomeComponent->SetupAttachment(RootComponent);
	ShieldDomeComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShieldDomeComponent->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DomeMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (DomeMesh.Succeeded())
	{
		ShieldDomeComponent->SetStaticMesh(DomeMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DomeMat(TEXT("/Game/OJJ/Materials/MI_EnergyShield.MI_EnergyShield"));
	if (DomeMat.Succeeded())
	{
		ShieldDomeComponent->SetMaterial(0, DomeMat.Object);
	}
}

void AOJJ_ProtectionTower::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateDomeVisual(DeltaSeconds);
}

void AOJJ_ProtectionTower::BeginPlay()
{
	// AMachineBase::BeginPlay가 PlanetEventManager에 "머신"으로, FactoryManager에 등록한다.
	// 차폐장은 자기 반경(1200) 안에 자기 자신이 들어가므로 IsMachineShieldedFromMagneticStorm이
	// true → 자기폭풍 효율 modifier가 자신에게는 걸리지 않는다(자기 차폐). 단 머신 등록 시점엔
	// 아직 ShieldGenerator로 등록 전이므로, 아래 RegisterToEventManager가 ApplyActiveEventToMachines로
	// 전 머신을 재평가해 교정한다(폭풍 중 배치 시에도 자기 차폐가 즉시 반영).
	Super::BeginPlay();

	RegisterToEventManager();

	// [#353] 자기폭풍 이벤트 구독 + 초기 상태 동기화(이미 폭풍 중 배치돼도 돔 반영). 매니저=WorldSubsystem.
	// 공용 이벤트 델리게이트라 핸들러에서 MagneticStorm만 필터. 해제는 EndPlay 대칭(RemoveDynamic).
	if (UWorld* World = GetWorld())
	{
		if (UPlanetEventManagerSubsystem* Manager = World->GetSubsystem<UPlanetEventManagerSubsystem>())
		{
			Manager->OnPlanetEventStarted.AddDynamic(this, &AOJJ_ProtectionTower::HandleEventStarted);
			Manager->OnPlanetEventEnded.AddDynamic(this, &AOJJ_ProtectionTower::HandleEventEnded);
			bMagneticStormActive = (Manager->GetEventState().Type == EPlanetEventType::MagneticStorm);
		}
	}

	// [#353] 돔 반경 스케일 — Sphere 기본 반경 50uu이므로 GetShieldRadius()/50배 = 실제 반경(1200이면 24배 →
	// 반경 1200uu / 지름 2400uu). ShieldRadius는 데이터테이블 비의존 UPROPERTY(기본 1200)라 Super::BeginPlay 이후 유효.
	if (ShieldDomeComponent)
	{
		ShieldDomeComponent->SetRelativeScale3D(FVector(GetShieldRadius() / 50.0f));
	}

	// [#353] 표시 확정은 단일 진입점 경유(디버그 플래그 OR 자기폭풍 활성+가동). ⚠️ 직접 SetVisibility 금지.
	ShieldVisualAlpha = (bShowShieldDome || (bMagneticStormActive && IsShieldActive())) ? 1.0f : 0.0f;
	UpdateDomeVisibility();

	// PIE 디버그 반경 표시(에디터 상시 기즈모는 컴포넌트가 필요해 최소화 — 보고 참조).
	// 한계: persistent 스피어라 같은 월드 내 타워 이동/파괴 시 잔상이 남을 수 있음(dev 시각화 전용).
	if (bShowDebugRadius)
	{
		if (UWorld* World = GetWorld())
		{
			DrawDebugSphere(World, GetActorLocation(), ShieldRadius, 24, FColor::Cyan,
				/*bPersistentLines*/ true, /*LifeTime*/ -1.0f);
		}
	}
}

void AOJJ_ProtectionTower::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// [#353] 이벤트 구독 해제(BeginPlay AddDynamic 대칭) — Unregister와 같은 자리.
	if (UWorld* World = GetWorld())
	{
		if (UPlanetEventManagerSubsystem* Manager = World->GetSubsystem<UPlanetEventManagerSubsystem>())
		{
			Manager->OnPlanetEventStarted.RemoveDynamic(this, &AOJJ_ProtectionTower::HandleEventStarted);
			Manager->OnPlanetEventEnded.RemoveDynamic(this, &AOJJ_ProtectionTower::HandleEventEnded);
		}
	}

	UnregisterFromEventManager();
	Super::EndPlay(EndPlayReason);
}

bool AOJJ_ProtectionTower::AddItem(FName ItemID, int32 Count)
{
	LOG_SSR_W(TEXT("ProtectionTower has no input port and cannot receive item: %s"), *ItemID.ToString());
	return false;
}

bool AOJJ_ProtectionTower::CanReceiveConveyorItem(FName ItemID, int32 Count) const
{
	return false;
}

void AOJJ_ProtectionTower::RegisterToEventManager()
{
	if (UWorld* World = GetWorld())
	{
		if (UPlanetEventManagerSubsystem* Manager = World->GetSubsystem<UPlanetEventManagerSubsystem>())
		{
			Manager->RegisterShieldGenerator(this);
		}
	}
}

void AOJJ_ProtectionTower::UnregisterFromEventManager()
{
	if (UWorld* World = GetWorld())
	{
		if (UPlanetEventManagerSubsystem* Manager = World->GetSubsystem<UPlanetEventManagerSubsystem>())
		{
			Manager->UnregisterShieldGenerator(this);
		}
	}
}

void AOJJ_ProtectionTower::HandleEventStarted(EPlanetEventType EventType, float Severity)
{
	// [#353] 공용 이벤트 델리게이트 — 자기폭풍만 처리(SandStorm 등 무시).
	if (EventType != EPlanetEventType::MagneticStorm)
	{
		return;
	}
	bMagneticStormActive = true;
	UpdateDomeVisibility();
}

void AOJJ_ProtectionTower::HandleEventEnded(EPlanetEventType EventType)
{
	if (EventType != EPlanetEventType::MagneticStorm)
	{
		return;
	}
	bMagneticStormActive = false;
	UpdateDomeVisibility();
}

void AOJJ_ProtectionTower::UpdateDomeVisibility()
{
	// [#353] 돔 표시 단일 출처 — 디버그 강제(bShowShieldDome) OR (자기폭풍 활성 && 차폐 가동). 분기 산재 금지.
	if (!ShieldDomeComponent)
	{
		return;
	}
	const bool bShow = bShowShieldDome || (bMagneticStormActive && IsShieldActive());
	ShieldDomeComponent->SetVisibility(bShow || ShieldVisualAlpha > KINDA_SMALL_NUMBER);
}

void AOJJ_ProtectionTower::UpdateDomeVisual(float DeltaSeconds)
{
	if (!ShieldDomeComponent)
	{
		return;
	}

	const bool bTargetShow = bShowShieldDome || (bMagneticStormActive && IsShieldActive());
	const float TargetAlpha = bTargetShow ? 1.0f : 0.0f;
	const bool bBlendingIn = TargetAlpha > ShieldVisualAlpha;
	const float BlendSeconds = bBlendingIn ? ShieldBlendInSeconds : ShieldBlendOutSeconds;
	const float BlendRate = BlendSeconds > KINDA_SMALL_NUMBER ? (1.0f / BlendSeconds) : 1.0f;
	ShieldVisualAlpha = FMath::FInterpConstantTo(ShieldVisualAlpha, TargetAlpha, DeltaSeconds, BlendRate);

	const float SmoothedAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, ShieldVisualAlpha, 2.0f);
	const float DomeScale = (GetShieldRadius() / 50.0f) * FMath::Lerp(0.05f, 1.0f, SmoothedAlpha);
	ShieldDomeComponent->SetRelativeScale3D(FVector(DomeScale));
	ShieldDomeComponent->SetVisibility(bTargetShow || ShieldVisualAlpha > KINDA_SMALL_NUMBER);
}
