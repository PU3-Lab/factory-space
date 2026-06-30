#include "TeleCommunicationTower.h"

#include "Wanted_Factory.h"

ATeleCommunicationTower::ATeleCommunicationTower()
{
	PrimaryActorTick.bCanEverTick = false;

	MachineType = TEXT("TeleCommunicationTower");
	bNeedPower = true;
	bDisableWhenBroken = true;
	bInfiniteDurability = true;
	bShowDebugBufferText = false;
}

bool ATeleCommunicationTower::AddItem(FName ItemID, int32 Count)
{
	// LOG_SSR_W(TEXT("TeleCommunicationTower has no input port and cannot receive item: %s"), *ItemID.ToString());
	return false;
}

bool ATeleCommunicationTower::CanReceiveConveyorItem(FName ItemID, int32 Count) const
{
	return false;
}
