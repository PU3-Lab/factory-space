// Fill out your copyright notice in the Description page of Project Settings.


#include "Synthesizer.h"

ASynthesizer::ASynthesizer()
{
	PrimaryActorTick.bCanEverTick = true;

	MachineType = TEXT("Synthesizer");
	bNeedPower = true;
	bDisableWhenBroken = true;
}

void ASynthesizer::ApplyMachineData(const FMachineTableRow& MachineData)
{
	FMachineTableRow SynthesizerData = MachineData;
	SynthesizerData.InputPortCnt = 3;
	SynthesizerData.OutputPortCnt = 1;
	Super::ApplyMachineData(SynthesizerData);
}
