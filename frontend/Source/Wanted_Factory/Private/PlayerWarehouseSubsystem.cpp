#include "PlayerWarehouseSubsystem.h"

void UPlayerWarehouseSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	AddItem(TEXT("iron_ore"), 30);
	AddItem(TEXT("copper_ore"), 30);
	AddItem(TEXT("iron_ingot"), 20);
}

bool UPlayerWarehouseSubsystem::AddItem(FName ItemID, int32 Count)
{
	if (ItemID.IsNone() || Count <= 0)
	{
		return false;
	}

	int32& StoredCount = StoredItems.FindOrAdd(ItemID);
	StoredCount += Count;
	OnItemAdded.Broadcast(ItemID, Count, StoredCount);
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
