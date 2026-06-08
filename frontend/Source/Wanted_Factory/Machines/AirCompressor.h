// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MachineBase.h"
#include "Resource/ResourceBase.h"
#include "AirCompressor.generated.h"

class AOJJ_Grid;

UCLASS()
class WANTED_FACTORY_API AAirCompressor : public AMachineBase
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AAirCompressor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual bool CanPlaceAdditional(const AOJJ_Grid* Grid, FIntPoint Origin, int32 RotationSteps) const override;
	virtual void OnPlacedOnGrid(AOJJ_Grid* Grid, FIntPoint Origin, int32 RotationSteps) override;
	virtual void OnRemovedFromGrid() override;

	AResourceBase* FindAdjacentGas(const AOJJ_Grid* Grid, FIntPoint Origin, int32 RotationSteps) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Air Compressor")
	AResourceBase* LinkedResource;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air Compressor", meta = (ClampMin = "1"))
	int32 CompressAmount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air Compressor", meta = (ClampMin = "0.01"))
	float CompressInterval = 2.0f;

	FTimerHandle CompressTimerHandle;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Air Compressor")
	void SetLinkedResource(AResourceBase* NewResource);

	UFUNCTION(BlueprintCallable, Category = "Air Compressor")
	bool CanCompress() const;

	UFUNCTION(BlueprintCallable, Category = "Air Compressor")
	void StartCompressing();

	UFUNCTION(BlueprintCallable, Category = "Air Compressor")
	void StopCompressing();

	UFUNCTION(BlueprintCallable, Category = "Air Compressor")
	void CompressResource();
};
