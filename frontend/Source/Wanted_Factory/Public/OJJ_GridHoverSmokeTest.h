// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OJJ_GridHoverSmokeTest.generated.h"

class AOJJ_Grid;
class AMachineBase;

/**
 * 호버 미리보기 정적 검증용 임시 액터.
 * 레벨에 드롭 → TargetGrid/MachineClass 지정 → PIE로 색 분기 확인 → 검증 후 제거.
 */
UCLASS()
class WANTED_FACTORY_API AOJJ_GridHoverSmokeTest : public AActor
{
	GENERATED_BODY()

public:
	AOJJ_GridHoverSmokeTest();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditInstanceOnly, Category = "Smoke Test")
	TObjectPtr<AOJJ_Grid> TargetGrid;

	UPROPERTY(EditAnywhere, Category = "Smoke Test")
	TSubclassOf<AMachineBase> MachineClass;

	// 머신 A 실제 배치 위치 (점유될 셀)
	UPROPERTY(EditAnywhere, Category = "Smoke Test")
	FIntPoint OccupiedOrigin;

	// 머신 B 호버 표시 위치 (빈 셀, 녹색 기대)
	UPROPERTY(EditAnywhere, Category = "Smoke Test")
	FIntPoint EmptyHoverOrigin;

	// 빈→점유 호버 전환까지의 딜레이
	UPROPERTY(EditAnywhere, Category = "Smoke Test", meta = (ClampMin = "0.1"))
	float SwitchDelaySeconds;

private:
	UPROPERTY(Transient)
	TObjectPtr<AMachineBase> HoverMachine;

	FTimerHandle SwitchTimerHandle;

	void HoverOnOccupiedCell();
};
