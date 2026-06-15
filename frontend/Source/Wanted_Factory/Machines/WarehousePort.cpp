#include "Machines/WarehousePort.h"

#include "Engine/DataTable.h"
#include "PlayerWarehouseSubsystem.h"
#include "Resource/ResourceData.h"
#include "UObject/ConstructorHelpers.h"
#include "Wanted_Factory.h"

AWarehousePort::AWarehousePort()
{
	PrimaryActorTick.bCanEverTick = true;

	MachineType = TEXT("WarehousePort");
	bNeedPower = false;
	bDisableWhenBroken = true;

	static ConstructorHelpers::FObjectFinder<UDataTable> ResourceTableFinder(
		TEXT("/Game/DataTable/DT_ResourceData.DT_ResourceData"));
	if (ResourceTableFinder.Succeeded())
	{
		ResourceTable = ResourceTableFinder.Object;
	}
}

void AWarehousePort::SetSelectedOutputItem(FName ItemID)
{
	if (!ItemID.IsNone() && !IsSolidItem(ItemID))
	{
		LOG_SSR_W(TEXT("Warehouse output rejected non-solid item: %s"), *ItemID.ToString());
		return;
	}

	SelectedOutputItemID = ItemID;
	UpdateDebugBufferText();
}

int32 AWarehousePort::GetSelectedOutputItemCount() const
{
	if (!IsSolidItem(SelectedOutputItemID))
	{
		return 0;
	}

	const UPlayerWarehouseSubsystem* Warehouse = GetWarehouse();
	return Warehouse ? Warehouse->GetItemCount(SelectedOutputItemID) : 0;
}

bool AWarehousePort::AddItem(FName ItemID, int32 Count)
{
	if (isBroken() && bDisableWhenBroken)
	{
		return false;
	}

	return StoreInputItem(ItemID, Count);
}

bool AWarehousePort::CanReceiveConveyorItem(FName ItemID, int32 Count) const
{
	return !(isBroken() && bDisableWhenBroken)
		&& CanStoreSolid(ItemID, Count);
}

bool AWarehousePort::ReceiveConveyorItem(FName ItemID, int32 Count)
{
	if (!CanReceiveConveyorItem(ItemID, Count))
	{
		LOG_SSR_W(TEXT("Warehouse rejected conveyor item: %s x%d"),
			*ItemID.ToString(),
			Count);
		return false;
	}

	const bool bStored = StoreInputItem(ItemID, Count);
	if (bStored)
	{
		LOG_SSR_W(TEXT("Warehouse received from conveyor: %s x%d"),
			*ItemID.ToString(),
			Count);
	}

	return bStored;
}

bool AWarehousePort::StoreInputItem(FName ItemID, int32 Count)
{
	if (!CanStoreSolid(ItemID, Count))
	{
		return false;
	}

	UPlayerWarehouseSubsystem* Warehouse = GetWarehouse();
	if (!Warehouse || !Warehouse->AddItem(ItemID, Count))
	{
		return false;
	}

	LOG_SSR_W(TEXT("Warehouse stored: %s x%d, total %d"),
		*ItemID.ToString(),
		Count,
		Warehouse->GetItemCount(ItemID));

	UpdateDebugBufferText();
	return true;
}

bool AWarehousePort::PeekFirstOutputItem(FName& OutItemID) const
{
	OutItemID = NAME_None;

	const UPlayerWarehouseSubsystem* Warehouse = GetWarehouse();
	if (!Warehouse || SelectedOutputItemID.IsNone() || !IsSolidItem(SelectedOutputItemID))
	{
		return false;
	}

	if (!Warehouse->CanTakeItem(SelectedOutputItemID, 1))
	{
		return false;
	}

	OutItemID = SelectedOutputItemID;
	return true;
}

bool AWarehousePort::TryTakeFirstOutputItem(FName& OutItemID)
{
	if (!PeekFirstOutputItem(OutItemID))
	{
		return false;
	}

	UPlayerWarehouseSubsystem* Warehouse = GetWarehouse();
	if (!Warehouse || !Warehouse->TakeItem(OutItemID, 1))
	{
		OutItemID = NAME_None;
		return false;
	}

	LOG_SSR_W(TEXT("Warehouse released: %s x1, remaining %d"),
		*OutItemID.ToString(),
		Warehouse->GetItemCount(OutItemID));

	UpdateDebugBufferText();
	return true;
}

UPlayerWarehouseSubsystem* AWarehousePort::GetWarehouse() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UPlayerWarehouseSubsystem>() : nullptr;
}

bool AWarehousePort::IsSolidItem(FName ItemID) const
{
	if (!ResourceTable || ItemID.IsNone())
	{
		return false;
	}

	const FResourceData* Resource = ResourceTable->FindRow<FResourceData>(ItemID, TEXT("WarehousePort.IsSolidItem"));
	return Resource && Resource->form == FName(TEXT("solid"));
}

bool AWarehousePort::CanStoreSolid(FName ItemID, int32 Count) const
{
	return IsSolidItem(ItemID) && Count > 0 && GetWarehouse();
}
