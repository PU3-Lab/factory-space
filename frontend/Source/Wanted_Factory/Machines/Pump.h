// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MachineBase.h"
#include "Resource/ResourceBase.h"
#include "Pump.generated.h"

class AOJJ_Grid;

UCLASS()
class WANTED_FACTORY_API APump : public AMachineBase
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	APump();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// 배치 제약: footprint 4방향 인접 셀에 form=="liquid" 수원이 하나라도 있어야 배치 가능.
	// 채굴기(AMinerMachine) Ore 인접 패턴 미러링하되 Claim(선점) 없음 — 무제한·무한(여러 펌프가 공유).
	// CDO-safe — 인자 Grid/Origin/RotationSteps와 CDO-safe getter(GetMachineSize)만 사용.
	virtual bool CanPlaceAdditional(const AOJJ_Grid* Grid, FIntPoint Origin, int32 RotationSteps) const override;

	// #182: 펌프는 물 위(WaterArea∩분류 water) 직접 배치 허용 — 게이트 A/B 면제 + 수면 Z 안착(그리드 측).
	virtual bool CanStandOnWater() const override { return true; }

	// 배치 확정: 인접 수원(form=liquid)을 LinkedResource로 연결. Claim 호출 안 함(무제한 공유).
	virtual void OnPlacedOnGrid(AOJJ_Grid* Grid, FIntPoint Origin, int32 RotationSteps) override;

	// Origin/회전 풋프린트의 4방향 인접 셀에서 첫 form=liquid 자원을 반환(없으면 nullptr).
	// 채굴기 FindAdjacentUnclaimedOre 미러링하되 IsClaimed 판정 없음. const + Grid 인자만 사용 →
	// CanPlaceAdditional(CDO 호출)에서도 안전. OnPlacedOnGrid도 재사용.
	AResourceBase* FindAdjacentWater(const AOJJ_Grid* Grid, FIntPoint Origin, int32 RotationSteps) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pump")
	AResourceBase* LinkedResource;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pump", meta = (ClampMin = "1"))
	int32 PumpAmount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pump", meta = (ClampMin = "0.01"))
	float PumpInterval = 2.0f;

	FTimerHandle PumpTimerHandle;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Pump")
	void SetLinkedResource(AResourceBase* NewResource);

	UFUNCTION(BlueprintCallable, Category = "Pump")
	bool CanPump() const;

	UFUNCTION(BlueprintCallable, Category = "Pump")
	void StartPumping();

	UFUNCTION(BlueprintCallable, Category = "Pump")
	void StopPumping();

	UFUNCTION(BlueprintCallable, Category = "Pump")
	void PumpResource();
};
