// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MachineBase.h"
#include "Synthesizer.generated.h"

UCLASS()
class WANTED_FACTORY_API ASynthesizer : public AMachineBase
{
	GENERATED_BODY()

public:
	ASynthesizer();

	virtual void ApplyMachineData(const FMachineTableRow& MachineData) override;
	virtual bool ShouldUseConveyorOccluder() const override { return false; }
};
