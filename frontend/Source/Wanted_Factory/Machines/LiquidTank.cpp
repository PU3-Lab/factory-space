#include "Machines/LiquidTank.h"

#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "PlayerWarehouseSubsystem.h"
#include "Resource/ResourceData.h"
#include "UObject/ConstructorHelpers.h"
#include "Wanted_Factory.h"

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
	SyncDisplayedBuffer();
	AMachineBase::UpdateDebugBufferText();
}

void ALiquidTank::SetSelectedOutputLiquid(FName ItemID)
{
	if (!ItemID.IsNone() && !IsLiquidItem(ItemID))
	{
		LOG_SSR_W(TEXT("Liquid tank output rejected non-liquid item: %s"), *ItemID.ToString());
		return;
	}

	SelectedOutputLiquidID = ItemID;
	SyncDisplayedBuffer();
	UpdateDebugBufferText();
}

FName ALiquidTank::GetStoredLiquidID() const
{
	const UPlayerWarehouseSubsystem* Warehouse = GetWarehouse();
	if (!Warehouse || SelectedOutputLiquidID.IsNone())
	{
		return NAME_None;
	}

	if (Warehouse->GetItemCount(SelectedOutputLiquidID) > 0)
	{
		return SelectedOutputLiquidID;
	}

	return NAME_None;
}

int32 ALiquidTank::GetStoredLiquidAmount() const
{
	const UPlayerWarehouseSubsystem* Warehouse = GetWarehouse();
	const FName StoredLiquidID = GetStoredLiquidID();
	return (!Warehouse || StoredLiquidID.IsNone()) ? 0 : Warehouse->GetItemCount(StoredLiquidID);
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

	UPlayerWarehouseSubsystem* Warehouse = GetWarehouse();
	if (!Warehouse || !Warehouse->TakeItem(ItemID, Count))
	{
		return false;
	}

	SyncDisplayedBuffer();
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

UPlayerWarehouseSubsystem* ALiquidTank::GetWarehouse() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UPlayerWarehouseSubsystem>() : nullptr;
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
	if (!IsLiquidItem(ItemID) || Count <= 0 || !GetWarehouse())
	{
		return false;
	}

	return true;
}

bool ALiquidTank::StoreLiquid(FName ItemID, int32 Count)
{
	if (!CanStoreLiquid(ItemID, Count))
	{
		return false;
	}

	UPlayerWarehouseSubsystem* Warehouse = GetWarehouse();
	if (!Warehouse || !Warehouse->AddItem(ItemID, Count))
	{
		return false;
	}

	SyncDisplayedBuffer();
	AMachineBase::UpdateDebugBufferText();
	return true;
}

void ALiquidTank::SyncDisplayedBuffer()
{
	OutputBuffer.Reset();

	const FName DisplayLiquidID = GetStoredLiquidID();
	const int32 DisplayAmount = GetStoredLiquidAmount();
	if (!DisplayLiquidID.IsNone() && DisplayAmount > 0)
	{
		OutputBuffer.Add(DisplayLiquidID, DisplayAmount);
	}
}
