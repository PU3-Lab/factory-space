// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MachineBase.h"
#include "TeleCommunicationTower.generated.h"

UCLASS()
class WANTED_FACTORY_API ATeleCommunicationTower : public AMachineBase
{
	GENERATED_BODY()

public:
	ATeleCommunicationTower();

	virtual bool AddItem(FName ItemID, int32 Count) override;
	virtual bool CanReceiveConveyorItem(FName ItemID, int32 Count = 1) const override;
	virtual bool CanPlayerInteract() const override { return false; }
};
