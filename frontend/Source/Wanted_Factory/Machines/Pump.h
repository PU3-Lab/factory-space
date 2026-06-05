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

	// 배치 제약(수원 인접) 뼈대 — 현재는 임시 true(무제약).
	// 채굴기(AMinerMachine)의 Ore 인접 패턴과 대칭으로, 수원은 form=="liquid"(Pump.cpp LiquidFormName,
	// CanPump 판정과 동일)인 미선점 자원을 4방향 인접에서 찾는 구조로 채울 예정.
	// CDO-safe 요구는 MachineBase.h 주석 참조.
	virtual bool CanPlaceAdditional(const AOJJ_Grid* Grid, FIntPoint Origin, int32 RotationSteps) const override;

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
