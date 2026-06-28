// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OJJ_PortraitCaptureSubsystem.generated.h"

class AOJJ_PortraitCapture;

/**
 * 로봇 포트레이트 캡처 액터 자동 스폰 (OJJ 소유, UWorldSubsystem).
 *
 * 엔진이 월드(레벨)마다 1개 생성 → 단일 인스턴스 전제를 자연히 충족하고, 멀티플레이 시 각 클라가
 * 로컬로 스폰한다(RT 복제 불필요). OnWorldBeginPlay에서 UOJJ_PortraitSettings.AutoSpawnLevels
 * 화이트리스트에 든 인게임 레벨에서만 AOJJ_PortraitCapture를 스폰하고, 레벨 언로드 시 정리한다.
 *
 * 파이썬 수동 스폰을 대체한다 — 어느 (허용) 레벨에서든, 머지/레벨 재오픈과 무관하게 자동 동작.
 */
UCLASS()
class WANTED_FACTORY_API UOJJ_PortraitCaptureSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

private:
	/** 이 서브시스템이 스폰한 인스턴스(자체 스폰분만 저장 — 레벨 수동 배치분은 Destroy 책임 없음). */
	UPROPERTY()
	TObjectPtr<AOJJ_PortraitCapture> PortraitCaptureInstance = nullptr;
};
