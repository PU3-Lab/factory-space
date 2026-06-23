#include "SignalAmplifier.h"

#include "Wanted_Factory.h"

ASignalAmplifier::ASignalAmplifier()
{
	PrimaryActorTick.bCanEverTick = false;

	MachineType = TEXT("Signal_Amplifier");
	bNeedPower = true;
	bDisableWhenBroken = true;
	bShowDebugBufferText = false;
}

bool ASignalAmplifier::AddItem(FName ItemID, int32 Count)
{
	LOG_SSR_W(TEXT("Signal_Amplifier has no input port and cannot receive item: %s"), *ItemID.ToString());
	return false;
}

bool ASignalAmplifier::CanReceiveConveyorItem(FName ItemID, int32 Count) const
{
	return false;
}
