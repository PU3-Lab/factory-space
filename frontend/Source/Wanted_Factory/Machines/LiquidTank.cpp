#include "Machines/LiquidTank.h"

#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "Resource/ResourceData.h"
#include "UObject/ConstructorHelpers.h"

ALiquidTank::ALiquidTank()
{
	PrimaryActorTick.bCanEverTick = true;

	MachineType = TEXT("LiquidTank");
	bNeedPower = false;
	bDisableWhenBroken = true;
	InputPortCount = 1;
	OutputPortCount = 1;
	InputBufferCount = 0;
	OutputBufferCount = 1;
	MaxBufferPerItem = Capacity;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded() && MeshComponent)
	{
		MeshComponent->SetStaticMesh(CylinderMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UDataTable> ResourceTableFinder(
		TEXT("/Game/DataTable/DT_ResourceData.DT_ResourceData"));
	if (ResourceTableFinder.Succeeded())
	{
		ResourceTable = ResourceTableFinder.Object;
	}
}

void ALiquidTank::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	MaxBufferPerItem = Capacity;
	AMachineBase::UpdateDebugBufferText();
}

FName ALiquidTank::GetStoredLiquidID() const
{
	for (const TPair<FName, int32>& Pair : OutputBuffer)
	{
		if (!Pair.Key.IsNone() && Pair.Value > 0)
		{
			return Pair.Key;
		}
	}

	return NAME_None;
}

int32 ALiquidTank::GetStoredLiquidAmount() const
{
	const FName StoredLiquidID = GetStoredLiquidID();
	return StoredLiquidID.IsNone() ? 0 : OutputBuffer.FindRef(StoredLiquidID);
}

bool ALiquidTank::AddItem(FName ItemID, int32 Count)
{
	if (isBroken() && bDisableWhenBroken)
	{
		return false;
	}

	return StoreLiquid(ItemID, Count);
}

bool ALiquidTank::TakeOutputItem(FName ItemID, int32 Count)
{
	if (ItemID.IsNone() || Count <= 0)
	{
		return false;
	}

	int32* StoredAmount = OutputBuffer.Find(ItemID);
	if (!StoredAmount || *StoredAmount < Count)
	{
		return false;
	}

	*StoredAmount -= Count;
	if (*StoredAmount <= 0)
	{
		OutputBuffer.Remove(ItemID);
	}

	UpdateDebugBufferText();
	return true;
}

bool ALiquidTank::CanReceiveConveyorItem(FName ItemID, int32 Count) const
{
	return !(isBroken() && bDisableWhenBroken) && CanStoreLiquid(ItemID, Count);
}

bool ALiquidTank::ReceiveConveyorItem(FName ItemID, int32 Count)
{
	return StoreLiquid(ItemID, Count);
}

bool ALiquidTank::PeekFirstOutputItem(FName& OutItemID) const
{
	OutItemID = GetStoredLiquidID();
	return !OutItemID.IsNone() && GetStoredLiquidAmount() > 0;
}

bool ALiquidTank::TryTakeFirstOutputItem(FName& OutItemID)
{
	if (!PeekFirstOutputItem(OutItemID))
	{
		return false;
	}

	if (!TakeOutputItem(OutItemID, 1))
	{
		OutItemID = NAME_None;
		return false;
	}

	return true;
}

bool ALiquidTank::IsLiquidItem(FName ItemID) const
{
	if (!ResourceTable || ItemID.IsNone())
	{
		return false;
	}

	const FResourceData* Resource = ResourceTable->FindRow<FResourceData>(ItemID, TEXT("LiquidTank.IsLiquidItem"));
	return Resource && Resource->form == FName(TEXT("liquid"));
}

bool ALiquidTank::CanStoreLiquid(FName ItemID, int32 Count) const
{
	if (!IsLiquidItem(ItemID) || Count <= 0)
	{
		return false;
	}

	const FName StoredLiquidID = GetStoredLiquidID();
	if (!StoredLiquidID.IsNone() && StoredLiquidID != ItemID)
	{
		return false;
	}

	return GetStoredLiquidAmount() + Count <= Capacity;
}

bool ALiquidTank::StoreLiquid(FName ItemID, int32 Count)
{
	if (!CanStoreLiquid(ItemID, Count))
	{
		return false;
	}

	MaxBufferPerItem = Capacity;
	OutputBuffer.FindOrAdd(ItemID) += Count;
	AMachineBase::UpdateDebugBufferText();
	return true;
}
