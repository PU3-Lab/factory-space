// Fill out your copyright notice in the Description page of Project Settings.


#include "Synthesizer.h"

ASynthesizer::ASynthesizer()
{
	PrimaryActorTick.bCanEverTick = true;

	MachineType = TEXT("Synthesizer");
	GridSize = FIntPoint(3, 3);
	InputPortCount = 2;
	OutputPortCount = 2;
	bNeedPower = true;
	PowerConsumption = 10.f;
	MaxDurability = 1000.f;
	CurrentDurability = MaxDurability;
	bDisableWhenBroken = true;

	InputPorts.Reset();
	for (int32 PortIndex = 0; PortIndex < InputPortCount; ++PortIndex)
	{
		FMachinePortData InputPort;
		InputPort.PortIndex = PortIndex;
		InputPort.PortType = EPortType::Input;
		InputPorts.Add(InputPort);
	}

	OutputPorts.Reset();
	for (int32 PortIndex = 0; PortIndex < OutputPortCount; ++PortIndex)
	{
		FMachinePortData OutputPort;
		OutputPort.PortIndex = PortIndex;
		OutputPort.PortType = EPortType::Output;
		OutputPorts.Add(OutputPort);
	}
}
