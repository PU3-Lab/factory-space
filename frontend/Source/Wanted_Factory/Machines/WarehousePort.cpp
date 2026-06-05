#include "Machines/WarehousePort.h"

#include "PlayerWarehouseSubsystem.h"
#include "Wanted_Factory.h"

AWarehousePort::AWarehousePort()
{
	PrimaryActorTick.bCanEverTick = false;

	MachineType = TEXT("WarehousePort");
	GridSize = FIntPoint(2, 2);
	InputPortCount = 1;
	OutputPortCount = 1;
	bNeedPower = false;
	MaxDurability = 1000.f;
	CurrentDurability = MaxDurability;
	bDisableWhenBroken = true;

	InputPorts.Reset();
	FMachinePortData InputPort;
	InputPort.PortIndex = 0;
	InputPort.PortType = EPortType::Input;
	InputPorts.Add(InputPort);

	OutputPorts.Reset();
	FMachinePortData OutputPort;
	OutputPort.PortIndex = 0;
	OutputPort.PortType = EPortType::Output;
	OutputPorts.Add(OutputPort);
}

void AWarehousePort::SetSelectedOutputItem(FName ItemID)
{
	SelectedOutputItemID = ItemID;
	UpdateDebugBufferText();
}

int32 AWarehousePort::GetSelectedOutputItemCount() const
{
	const UPlayerWarehouseSubsystem* Warehouse = GetWarehouse();
	return Warehouse ? Warehouse->GetItemCount(SelectedOutputItemID) : 0;
}

bool AWarehousePort::AddItem(FName ItemID, int32 Count)
{
	if (isBroken() && bDisableWhenBroken)
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

bool AWarehousePort::CanReceiveConveyorItem(FName ItemID, int32 Count) const
{
	return !(isBroken() && bDisableWhenBroken)
		&& !ItemID.IsNone()
		&& Count > 0
		&& GetWarehouse();
}

bool AWarehousePort::ReceiveConveyorItem(FName ItemID, int32 Count)
{
	return AddItem(ItemID, Count);
}

bool AWarehousePort::PeekFirstOutputItem(FName& OutItemID) const
{
	OutItemID = NAME_None;

	const UPlayerWarehouseSubsystem* Warehouse = GetWarehouse();
	if (!Warehouse || SelectedOutputItemID.IsNone())
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
