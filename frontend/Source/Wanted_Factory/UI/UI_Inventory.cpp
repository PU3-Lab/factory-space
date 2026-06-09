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

	// 서브시스템의 아이템들을 순서대로 꺼내기 위해 임시 배열로 변환
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
			// 30칸 중 현재 순번에 데이터가 있으면 넣고, 없으면 빈 슬롯(None, 0)으로 업데이트
			if (i < ItemIDs.Num())
			{
				FName Key = ItemIDs[i];
				NewSlot->UpdateSlot(Key, CurrentItems[Key]);
			}
			else
			{
				NewSlot->UpdateSlot(NAME_None, 0); // 빈 슬롯 배경만 그리게 됨
			}

			int32 Row = i / MaxColumns;
			int32 Column = i % MaxColumns;

			GDP_ItemGrid->AddChildToUniformGrid(NewSlot, Row, Column);
		}
	}
}