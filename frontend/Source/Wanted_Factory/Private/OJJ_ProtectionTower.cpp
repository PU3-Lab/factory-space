// Fill out your copyright notice in the Description page of Project Settings.

#include "OJJ_ProtectionTower.h"

#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "PlanetEventManagerSubsystem.h"
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
	UnregisterFromEventManager();
	Super::EndPlay(EndPlayReason);
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
