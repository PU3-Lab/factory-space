// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OJJ_PortraitSettings.generated.h"

class AOJJ_PortraitCapture;

/**
 * 로봇 포트레이트 자동 스폰 설정 (Project Settings ▸ Game ▸ "OJJ Portrait Capture").
 *
 * config=Game → DefaultGame.ini에 저장. 레벨이 추가될 때 코드 수정 없이 에디터에서 AutoSpawnLevels에
 * 맵명을 추가하면, UOJJ_PortraitCaptureSubsystem이 해당 레벨에서 AOJJ_PortraitCapture를 자동 스폰한다.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "OJJ Portrait Capture"))
class WANTED_FACTORY_API UOJJ_PortraitSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UOJJ_PortraitSettings();

	/** 포트레이트 캡처 액터를 자동 스폰할 레벨(접두사 없는 맵명, 예: L_Planet). 코드 수정 없이 여기서 추가. */
	UPROPERTY(EditAnywhere, config, Category = "AutoSpawn")
	TArray<FName> AutoSpawnLevels;

	/** 스폰할 액터 클래스(미지정 시 AOJJ_PortraitCapture 기본). BP 변형을 쓰려면 여기 지정. */
	UPROPERTY(EditAnywhere, config, Category = "AutoSpawn")
	TSoftClassPtr<AOJJ_PortraitCapture> PortraitCaptureClass;

	/** 메인뷰 격리용 스폰 높이(지하). */
	UPROPERTY(EditAnywhere, config, Category = "AutoSpawn")
	float SpawnZ = -5000.f;

	/** Project Settings 분류 — "Game" 섹션에 표시. */
	virtual FName GetCategoryName() const override { return FName("Game"); }
};
