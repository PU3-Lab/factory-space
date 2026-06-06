// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MachineBase.h"
#include "Resource/ResourceBase.h"
#include "MinerMachine.generated.h"

class AOJJ_Grid;

UCLASS()
class WANTED_FACTORY_API AMinerMachine : public AMachineBase
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AMinerMachine();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 배치 제약: footprint 4방향 인접 셀에 미선점 Ore 광맥이 하나라도 있어야 배치 가능.
	// CDO-safe — 인자 Grid/Origin/RotationSteps만 사용(MachineBase.h 주석 참조).
	virtual bool CanPlaceAdditional(const AOJJ_Grid* Grid, FIntPoint Origin, int32 RotationSteps) const override;

	// 배치 확정: 인접 미선점 Ore 광맥을 선점(Claim) + LinkedResource 연결.
	virtual void OnPlacedOnGrid(AOJJ_Grid* Grid, FIntPoint Origin, int32 RotationSteps) override;

	// 제거: 선점한 광맥 해제.
	virtual void OnRemovedFromGrid() override;

	// Origin/회전 풋프린트의 4방향 인접 셀에서 첫 번째 미선점 Ore 광맥을 반환(없으면 nullptr).
	// const + Grid 인자만 사용 → CanPlaceAdditional(CDO 호출)에서도 안전. OnPlacedOnGrid도 재사용.
	AResourceBase* FindAdjacentUnclaimedOre(const AOJJ_Grid* Grid, FIntPoint Origin, int32 RotationSteps) const;

	// 채굴 대상 자원
   UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining")
	AResourceBase* LinkedResource;

	// 한 번 채굴할 때 얻는 수량
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining")
	int32 MineAmount = 1;

	// 채굴 주기
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining")
	float MineInterval = 2.0f;

	FTimerHandle MineTimerHandle;

public:
	// Grid/Placement 쪽에서 설치 성공 후 호출
	UFUNCTION(BlueprintCallable, Category="Mining")
	void SetLinkedResource(AResourceBase* NewResource);

	// 채굴 가능한 상태인지 확인
	UFUNCTION(BlueprintCallable, Category="Mining")
	bool CanMine() const;

	// 실제 채굴 실행
	UFUNCTION(BlueprintCallable, Category="Mining")
	void MineResource();

	// 채굴 시작
	UFUNCTION(BlueprintCallable, Category="Mining")
	void StartMining();

	// 채굴 정지
	UFUNCTION(BlueprintCallable, Category="Mining")
	void StopMining();
};
