#include "PlayerWarehouseSubsystem.h"

bool UPlayerWarehouseSubsystem::AddItem(FName ItemID, int32 Count)
{
	if (ItemID.IsNone() || Count <= 0)
	{
		return false;
	}

	StoredItems.FindOrAdd(ItemID) += Count;
	return true;
}

bool UPlayerWarehouseSubsystem::CanTakeItem(FName ItemID, int32 Count) const
{
	if (ItemID.IsNone() || Count <= 0)
	{
		return false;
	}

	return StoredItems.FindRef(ItemID) >= Count;
}

bool UPlayerWarehouseSubsystem::TakeItem(FName ItemID, int32 Count)
{
	if (!CanTakeItem(ItemID, Count))
	{
		return false;
	}

	int32& StoredCount = StoredItems.FindOrAdd(ItemID);
	StoredCount -= Count;
	if (StoredCount <= 0)
	{
		StoredItems.Remove(ItemID);
	}

	return true;
}

int32 UPlayerWarehouseSubsystem::GetItemCount(FName ItemID) const
{
	return ItemID.IsNone() ? 0 : StoredItems.FindRef(ItemID);
}

void UPlayerWarehouseSubsystem::ClearWarehouse()
{
	StoredItems.Reset();
}
