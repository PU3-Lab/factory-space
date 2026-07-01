#include "BaseCamp.h"

#include "Wanted_Factory.h"

ABaseCamp::ABaseCamp()
{
	PrimaryActorTick.bCanEverTick = false;

	MachineType = TEXT("BaseCamp");
	bNeedPower = false;
	bDisableWhenBroken = false;
	bInfiniteDurability = true;
	bShowDebugBufferText = false;
}

bool ABaseCamp::AddItem(FName ItemID, int32 Count)
{
	// LOG_SSR_W(TEXT("BaseCamp has no input port and cannot receive item: %s"), *ItemID.ToString());
	return Super::AddItem(ItemID, Count);
}

bool ABaseCamp::CanReceiveConveyorItem(FName ItemID, int32 Count) const
{
	return false;
}
