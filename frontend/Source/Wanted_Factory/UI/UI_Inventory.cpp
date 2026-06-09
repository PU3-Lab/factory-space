#include "UI/UI_Inventory.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Layout/Margin.h"               
#include "PlayerWarehouseSubsystem.h"
#include "UI/UI_InventorySlot.h"

void UUI_Inventory::RefreshInventoryWindow()
{
	if (!GDP_ItemGrid || !SlotWidgetClass) return;
	
	GDP_ItemGrid->ClearChildren(); 

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance) return;

	UPlayerWarehouseSubsystem* WarehouseSubsystem = GameInstance->GetSubsystem<UPlayerWarehouseSubsystem>();
	if (!WarehouseSubsystem) return;

	const TMap<FName, int32>& CurrentItems = WarehouseSubsystem->GetStoredItems();
	TArray<FName> ItemIDs;
	CurrentItems.GetKeys(ItemIDs);

	int32 MaxColumns = 6; 
	int32 TotalSlots = 30; 

	for (int32 i = 0; i < TotalSlots; ++i)
	{
		UUI_InventorySlot* NewSlot = CreateWidget<UUI_InventorySlot>(this, SlotWidgetClass);
		if (NewSlot)
		{
			if (i < ItemIDs.Num())
			{
				FName Key = ItemIDs[i];
				NewSlot->UpdateSlot(Key, CurrentItems[Key]);
			}
			else
			{
				NewSlot->UpdateSlot(NAME_None, 0);
			}

			int32 Row = i / MaxColumns;
			int32 Column = i % MaxColumns;

			GDP_ItemGrid->AddChildToUniformGrid(NewSlot, Row, Column);
		}
	}
}