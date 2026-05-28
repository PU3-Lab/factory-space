// Fill out your copyright notice in the Description page of Project Settings.


#include "OJJ_GridHoverSmokeTest.h"

#include "Engine/World.h"
#include "MachineBase.h"
#include "OJJ_Grid.h"
#include "TimerManager.h"

AOJJ_GridHoverSmokeTest::AOJJ_GridHoverSmokeTest()
{
	PrimaryActorTick.bCanEverTick = false;
	OccupiedOrigin = FIntPoint(3, 5);
	EmptyHoverOrigin = FIntPoint(5, 5);
	SwitchDelaySeconds = 2.0f;
}

void AOJJ_GridHoverSmokeTest::BeginPlay()
{
	Super::BeginPlay();

	if (!TargetGrid || !MachineClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GridHoverSmokeTest] TargetGrid 또는 MachineClass 미설정"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TargetGrid->SetVisualizationVisible(true);

	// 머신 A: 점유 셀에 실제 배치 (TryPlaceMachine이 위치까지 이동시킴)
	if (AMachineBase* MachineA = World->SpawnActor<AMachineBase>(MachineClass, FVector::ZeroVector, FRotator::ZeroRotator))
	{
		FString Reason;
		const bool bPlaced = TargetGrid->TryPlaceMachine(MachineA, OccupiedOrigin, Reason);
		if (!bPlaced)
		{
			UE_LOG(LogTemp, Warning, TEXT("[GridHoverSmokeTest] TryPlaceMachine 실패: %s"), *Reason);
		}
		else
		{
			// 빨강 호버 시각 검증을 위해 머신을 Z+100으로 띄움 (그리드 점유는 유지)
			FVector LiftedLoc = MachineA->GetActorLocation();
			LiftedLoc.Z += 100.0f;
			MachineA->SetActorLocation(LiftedLoc);
		}
	}

	// 머신 B: 호버 미리보기 전용 (그리드 등록 X). 시각 혼동 방지를 위해 메시 숨김.
	HoverMachine = World->SpawnActor<AMachineBase>(MachineClass, FVector::ZeroVector, FRotator::ZeroRotator);
	if (!HoverMachine)
	{
		return;
	}
	HoverMachine->SetActorHiddenInGame(true);

	TargetGrid->UpdateHoverPreview(HoverMachine, EmptyHoverOrigin);
	UE_LOG(LogTemp, Display, TEXT("[GridHoverSmokeTest] 호버 (빈 셀) → 녹색 기대 @%s"), *EmptyHoverOrigin.ToString());

	World->GetTimerManager().SetTimer(
		SwitchTimerHandle,
		this,
		&AOJJ_GridHoverSmokeTest::HoverOnOccupiedCell,
		SwitchDelaySeconds,
		false);
}

void AOJJ_GridHoverSmokeTest::HoverOnOccupiedCell()
{
	if (!TargetGrid || !HoverMachine)
	{
		return;
	}

	TargetGrid->UpdateHoverPreview(HoverMachine, OccupiedOrigin);
	UE_LOG(LogTemp, Display, TEXT("[GridHoverSmokeTest] 호버 (점유 셀) → 빨강 기대 @%s"), *OccupiedOrigin.ToString());
}
