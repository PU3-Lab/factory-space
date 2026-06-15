// Fill out your copyright notice in the Description page of Project Settings.

#include "OJJ_ProtectionTower.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "PlanetEventManagerSubsystem.h"
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

}

void AOJJ_ProtectionTower::BeginPlay()
{
	// AMachineBase::BeginPlay가 PlanetEventManager에 "머신"으로, FactoryManager에 등록한다.
	// 차폐장은 자기 반경(700) 안에 자기 자신이 들어가므로 IsMachineShieldedFromMagneticStorm이
	// true → 자기폭풍 효율 modifier가 자신에게는 걸리지 않는다(자기 차폐). 단 머신 등록 시점엔
	// 아직 ShieldGenerator로 등록 전이므로, 아래 RegisterToEventManager가 ApplyActiveEventToMachines로
	// 전 머신을 재평가해 교정한다(폭풍 중 배치 시에도 자기 차폐가 즉시 반영).
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
