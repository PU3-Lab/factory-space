// Fill out your copyright notice in the Description page of Project Settings.

#include "OJJ_PortraitCaptureSubsystem.h"

#include "OJJ_PortraitCapture.h"
#include "OJJ_PortraitSettings.h"
#include "Engine/World.h"
#include "EngineUtils.h" // TActorIterator

void UOJJ_PortraitCaptureSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// 에디터 프리뷰 등 비게임 월드 제외.
	if (!InWorld.IsGameWorld())
	{
		return;
	}

	// 데디케이티드 서버는 렌더가 없어 포트레이트 캡처가 무의미 — 스폰하지 않는다(메시/RT 로드·캡처 비용 낭비 회피).
	if (InWorld.GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const UOJJ_PortraitSettings* Settings = GetDefault<UOJJ_PortraitSettings>();
	if (!Settings)
	{
		return;
	}

	// 현재 persistent 맵명(PIE 접두사 제거) 화이트리스트 체크 — 인게임 레벨에서만 스폰.
	// AutoSpawnLevels는 persistent 레벨명 전용 — 스트리밍 서브레벨/World Partition 셀명과는 매칭되지 않는다.
	FString MapName = InWorld.GetMapName();
	MapName.RemoveFromStart(InWorld.StreamingLevelsPrefix);
	if (!Settings->AutoSpawnLevels.Contains(FName(*MapName)))
	{
		return;
	}

	// 중복 가드 — 레벨에 이미 (수동 배치/타 경로로) 존재하면 스폰하지 않는다.
	// 이 경우 PortraitCaptureInstance에 저장하지 않아 Deinitialize에서 외부 소유분을 파괴하지 않는다.
	for (TActorIterator<AOJJ_PortraitCapture> It(&InWorld); It; ++It)
	{
		UE_LOG(LogTemp, Log, TEXT("[PortraitSubsystem] '%s'에 기존 PortraitCapture 존재 — 자동 스폰 생략."), *MapName);
		return;
	}

	TSubclassOf<AOJJ_PortraitCapture> SpawnClass = Settings->PortraitCaptureClass.LoadSynchronous();
	if (!SpawnClass)
	{
		SpawnClass = AOJJ_PortraitCapture::StaticClass();
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector SpawnLoc(0.f, 0.f, Settings->SpawnZ);

	PortraitCaptureInstance = InWorld.SpawnActor<AOJJ_PortraitCapture>(
		SpawnClass, SpawnLoc, FRotator::ZeroRotator, Params);

	if (PortraitCaptureInstance)
	{
		UE_LOG(LogTemp, Log, TEXT("[PortraitSubsystem] '%s'에 PortraitCapture 자동 스폰 @ Z=%.0f."), *MapName, Settings->SpawnZ);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[PortraitSubsystem] '%s' PortraitCapture 스폰 실패."), *MapName);
	}
}

void UOJJ_PortraitCaptureSubsystem::Deinitialize()
{
	// 자체 스폰분만 정리(레벨 수동 배치분은 건드리지 않음).
	if (PortraitCaptureInstance)
	{
		PortraitCaptureInstance->Destroy();
		PortraitCaptureInstance = nullptr;
	}

	Super::Deinitialize();
}
