#include "DummyMachineBase.h"

#include "Components/TextRenderComponent.h"
#include "Wanted_Factory.h"

namespace
{
void AddRecipeItemQuantity(TMap<FName, int32>& ItemQuantities, FName ItemID, int32 Count)
{
	if (ItemID.IsNone() || Count <= 0)
	{
		return;
	}

	ItemQuantities.FindOrAdd(ItemID) += Count;
}

TMap<FName, int32> BuildInputQuantities(const FRecipeTable& Recipe)
{
	TMap<FName, int32> InputQuantities;
	AddRecipeItemQuantity(InputQuantities, Recipe.InputItem1, Recipe.InputQty1);
	AddRecipeItemQuantity(InputQuantities, Recipe.InputItem2, Recipe.InputQty2);
	AddRecipeItemQuantity(InputQuantities, Recipe.InputItem3, Recipe.InputQty3);
	return InputQuantities;
}

TMap<FName, int32> BuildOutputQuantities(const FRecipeTable& Recipe)
{
	TMap<FName, int32> OutputQuantities;
	AddRecipeItemQuantity(OutputQuantities, Recipe.OutputItem1, Recipe.OutputQty1);
	AddRecipeItemQuantity(OutputQuantities, Recipe.OutputItem2, Recipe.OutputQty2);
	return OutputQuantities;
}

FString FormatItemMap(const TMap<FName, int32>& Items)
{
	if (Items.Num() == 0)
	{
		return TEXT("None");
	}

	FString Result;
	for (const TPair<FName, int32>& Item : Items)
	{
		if (Item.Key.IsNone() || Item.Value <= 0)
		{
			continue;
		}

		if (!Result.IsEmpty())
		{
			Result += TEXT("\n");
		}
		Result += FString::Printf(TEXT("%s x%d"), *Item.Key.ToString(), Item.Value);
	}

	return Result.IsEmpty() ? FString(TEXT("None")) : Result;
}
}

bool ADummyMachineBase::CanAddInputItem(FName ItemID, int32 Count) const
{
	if (bIsBroken || ItemID.IsNone() || Count <= 0)
	{
		return false;
	}

	return InputInventory.FindRef(ItemID) + Count <= MaxInputPerItem;
}

bool ADummyMachineBase::HasEnoughIngredients(const FRecipeTable& Recipe) const
{
	const TMap<FName, int32> InputQuantities = BuildInputQuantities(Recipe);
	for (const TPair<FName, int32>& InputQuantity : InputQuantities)
	{
		const int32* FoundCount = InputInventory.Find(InputQuantity.Key);
		if (!FoundCount || *FoundCount < InputQuantity.Value)
		{
			return false;
		}
	}

	return true;
}

void ADummyMachineBase::ConsumeIngredients(const FRecipeTable& Recipe)
{
	const TMap<FName, int32> InputQuantities = BuildInputQuantities(Recipe);
	for (const TPair<FName, int32>& InputQuantity : InputQuantities)
	{
		int32* FoundCount = InputInventory.Find(InputQuantity.Key);
		if (!FoundCount)
		{
			continue;
		}

		*FoundCount -= InputQuantity.Value;
		if (*FoundCount <= 0)
		{
			InputInventory.Remove(InputQuantity.Key);
		}
	}

	UpdateDebugBufferText();
}

void ADummyMachineBase::AddOutputItem(FName ItemID, int32 Count)
{
	if (ItemID.IsNone() || Count <= 0)
	{
		return;
	}

	const int32 CurrentCount = OutputBuffer.FindRef(ItemID);
	if (CurrentCount + Count > MaxBufferPerItem)
	{
		MachineState = EDummyMachineState::Blocked;
		LOG_SSR_W(TEXT("Dummy output buffer full: %s %d / %d"), *ItemID.ToString(), CurrentCount, MaxBufferPerItem);
		return;
	}

	int32& BufferCount = OutputBuffer.FindOrAdd(ItemID);
	BufferCount += Count;
	UpdateDebugBufferText();
}

bool ADummyMachineBase::CanAddToOutputBuffer(const FRecipeTable& Recipe) const
{
	const TMap<FName, int32> OutputQuantities = BuildOutputQuantities(Recipe);
	for (const TPair<FName, int32>& OutputQuantity : OutputQuantities)
	{
		if (OutputBuffer.FindRef(OutputQuantity.Key) + OutputQuantity.Value > MaxBufferPerItem)
		{
			return false;
		}
	}

	return true;
}

bool ADummyMachineBase::TakeOutputItem(FName ItemID, int32 Count)
{
	if (ItemID.IsNone() || Count <= 0)
	{
		return false;
	}

	int32* FoundCount = OutputBuffer.Find(ItemID);
	if (!FoundCount || *FoundCount < Count)
	{
		LOG_SSR_W(TEXT("Dummy TakeOutputItem failed: %s"), *ItemID.ToString());
		return false;
	}

	*FoundCount -= Count;
	if (*FoundCount <= 0)
	{
		OutputBuffer.Remove(ItemID);
	}

	if (MachineState == EDummyMachineState::Blocked)
	{
		MachineState = EDummyMachineState::Idle;
		TryStartProcess();
	}

	UpdateDebugBufferText();
	return true;
}

bool ADummyMachineBase::PeekFirstOutputItem(FName& OutItemID) const
{
	OutItemID = NAME_None;
	for (const TPair<FName, int32>& Output : OutputBuffer)
	{
		if (!Output.Key.IsNone() && Output.Value > 0)
		{
			OutItemID = Output.Key;
			return true;
		}
	}

	return false;
}

bool ADummyMachineBase::TryTakeFirstOutputItem(FName& OutItemID)
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

bool ADummyMachineBase::CanReceiveConveyorItem(FName ItemID, int32 Count) const
{
	return CanAddInputItem(ItemID, Count);
}

bool ADummyMachineBase::ReceiveConveyorItem(FName ItemID, int32 Count)
{
	const bool bAdded = AddItem(ItemID, Count);
	UpdateDebugBufferText();
	return bAdded;
}

void ADummyMachineBase::UpdateDebugBufferText()
{
	if (!DebugBufferText)
	{
		return;
	}

	DebugBufferText->SetVisibility(bShowDebugBufferText);
	DebugBufferText->SetWorldSize(DebugTextWorldSize);
	DebugBufferText->SetRelativeLocation(DebugTextOffset);
	if (!bShowDebugBufferText)
	{
		return;
	}

	const FString DebugText = FString::Printf(
		TEXT("Input\n%s\n\nOutput\n%s\n\nDurability\n%.0f / %.0f\nEfficiency\n%.0f%%"),
		*FormatItemMap(InputInventory),
		*FormatItemMap(OutputBuffer),
		Durability,
		MaxDurability,
		PlanetProductionEfficiency * 100.0f);
	DebugBufferText->SetText(FText::FromString(DebugText));
}

bool ADummyMachineBase::TransferOutputToMachine(ADummyMachineBase* TargetMachine, FName ItemID, int32 Count)
{
	if (!TargetMachine || TargetMachine == this || ItemID.IsNone() || Count <= 0)
	{
		return false;
	}

	int32* FoundCount = OutputBuffer.Find(ItemID);
	if (!FoundCount || *FoundCount < Count || !TargetMachine->CanAddInputItem(ItemID, Count))
	{
		return false;
	}

	if (!TakeOutputItem(ItemID, Count))
	{
		return false;
	}

	return TargetMachine->AddItem(ItemID, Count);
}

bool ADummyMachineBase::ConnectOutputToMachine(
	int32 OutputPortIndex,
	ADummyMachineBase* TargetMachine,
	int32 TargetInputPortIndex)
{
	if (!TargetMachine || TargetMachine == this)
	{
		return false;
	}

	if (!OutputPorts.IsValidIndex(OutputPortIndex) || !TargetMachine->InputPorts.IsValidIndex(TargetInputPortIndex))
	{
		return false;
	}

	FDummyMachinePortConnection NewConnection;
	NewConnection.FromOutputPortIndex = OutputPortIndex;
	NewConnection.TargetMachine = TargetMachine;
	NewConnection.TargetInputPortIndex = TargetInputPortIndex;

	OutputConnections.RemoveAll(
		[OutputPortIndex](const FDummyMachinePortConnection& Connection)
		{
			return Connection.FromOutputPortIndex == OutputPortIndex;
		});

	OutputConnections.Add(NewConnection);
	OutputPorts[OutputPortIndex].bIsConnected = true;
	TargetMachine->InputPorts[TargetInputPortIndex].bIsConnected = true;
	return true;
}

bool ADummyMachineBase::TransferOutputByPort(int32 OutputPortIndex, FName ItemID, int32 Count)
{
	if (!OutputPorts.IsValidIndex(OutputPortIndex))
	{
		return false;
	}

	const FDummyMachinePortConnection* FoundConnection = OutputConnections.FindByPredicate(
		[OutputPortIndex](const FDummyMachinePortConnection& Connection)
		{
			return Connection.FromOutputPortIndex == OutputPortIndex;
		});

	if (!FoundConnection || !FoundConnection->TargetMachine)
	{
		return false;
	}

	return TransferOutputToMachine(FoundConnection->TargetMachine, ItemID, Count);
}

void ADummyMachineBase::DebugInventory()
{
	LOG_SSR_W(TEXT("========== Dummy Input Inventory =========="));
	for (const TPair<FName, int32>& Input : InputInventory)
	{
		LOG_SSR_W(TEXT("%s x %d"), *Input.Key.ToString(), Input.Value);
	}

	LOG_SSR_W(TEXT("========== Dummy Output Buffer =========="));
	for (const TPair<FName, int32>& Buffer : OutputBuffer)
	{
		LOG_SSR_W(TEXT("%s x %d / %d"), *Buffer.Key.ToString(), Buffer.Value, MaxBufferPerItem);
	}

	LOG_SSR_W(TEXT("Dummy durability: %.2f / %.2f"), Durability, MaxDurability);
	LOG_SSR_W(TEXT("Dummy production efficiency: %.2f"), PlanetProductionEfficiency);
}
