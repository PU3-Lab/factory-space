// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MachineBase.h"
#include "EscapePod.generated.h"

UCLASS()
class WANTED_FACTORY_API AEscapePod : public AMachineBase
{
	GENERATED_BODY()

public:
	AEscapePod();

	virtual void OnRemovedFromGrid() override;
	virtual bool OJJ_RequiresOccupancyOnlyRegistration() const override { return true; }

protected:
	virtual void BeginPlay() override;

private:
	void RegisterToNearestGrid();
};
