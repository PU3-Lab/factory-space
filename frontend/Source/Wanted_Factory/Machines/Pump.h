// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MachineBase.h"
#include "Resource/ResourceBase.h"
#include "Pump.generated.h"

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
