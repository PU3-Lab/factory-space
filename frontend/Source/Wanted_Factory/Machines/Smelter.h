// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MachineBase.h"
#include "Smelter.generated.h"

UCLASS()
class WANTED_FACTORY_API ASmelter : public AMachineBase
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ASmelter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual bool AddItem(FName ItemID, int32 Count) override;
	virtual void AddOutputItem(FName ItemID, int32 Count) override;
	virtual bool CanAddToOutputBuffer(const FRecipeTable& Recipe) const override;
	virtual bool CanReceiveConveyorItem(FName ItemID, int32 Count = 1) const override;
};
