// Fill out your copyright notice in the Description page of Project Settings.

#include "OJJ_ProtectionTower.h"

#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Wanted_Factory.h"

AOJJ_ProtectionTower::AOJJ_ProtectionTower()
{
	PrimaryActorTick.bCanEverTick = false;

	// 루트 = StaticMesh(비주얼). 메시 에셋은 BP에서 지정.
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(MeshComponent);
}

void AOJJ_ProtectionTower::BeginPlay()
{
	Super::BeginPlay();

	RegisterToEventManager();

	// PIE 디버그 반경 표시(에디터 상시 기즈모는 컴포넌트가 필요해 최소화 — 보고 참조).
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
	UnregisterFromEventManager();
	Super::EndPlay(EndPlayReason);
}

void AOJJ_ProtectionTower::RegisterToEventManager()
{
	// TODO(이찬 API 연동): PlanetEventManagerSubsystem에 차폐장 등록.
	// 이찬이 RegisterShieldGenerator(AOJJ_ProtectionTower*) 구현 시 아래 한 줄로 교체:
	//   if (UWorld* W = GetWorld()) if (auto* S = W->GetSubsystem<UPlanetEventManagerSubsystem>()) S->RegisterShieldGenerator(this);
	// (그때 #include "PlanetEventManagerSubsystem.h" 추가)
	LOG_OJJ(TEXT("ShieldGenerator BeginPlay (radius=%.0f, active=%d) — 이벤트 매니저 등록 대기(이찬 API 미구현)"),
		ShieldRadius, bIsShieldActive ? 1 : 0);
}

void AOJJ_ProtectionTower::UnregisterFromEventManager()
{
	// TODO(이찬 API 연동): PlanetEventManagerSubsystem에서 차폐장 등록 해제.
	// 이찬이 UnregisterShieldGenerator(AOJJ_ProtectionTower*) 구현 시 아래 한 줄로 교체:
	//   if (UWorld* W = GetWorld()) if (auto* S = W->GetSubsystem<UPlanetEventManagerSubsystem>()) S->UnregisterShieldGenerator(this);
	LOG_OJJ(TEXT("ShieldGenerator EndPlay — 이벤트 매니저 등록 해제 대기(이찬 API 미구현)"));
}
