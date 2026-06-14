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
	
	int32 MaxColumns = 5;  
	int32 TotalSlots = 30; // 총 슬롯은 30개로 유지 (5칸 * 6줄 = 30)

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
void UUI_Inventory::UpdateSlotQuantitiesOnly()
{
	if (!GDP_ItemGrid) return;

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance) return;

	UPlayerWarehouseSubsystem* WarehouseSubsystem = GameInstance->GetSubsystem<UPlayerWarehouseSubsystem>();
	if (!WarehouseSubsystem) return;

	// 현재 서브시스템에 담긴 최신 물품 내역 가져오기
	const TMap<FName, int32>& CurrentItems = WarehouseSubsystem->GetStoredItems();
	TArray<FName> ItemIDs;
	CurrentItems.GetKeys(ItemIDs);

	// 격자판에 생성되어 있는 자식 슬롯 개수를 파악합니다
	int32 ChildCount = GDP_ItemGrid->GetChildrenCount();

	for (int32 i = 0; i < ChildCount; ++i)
	{
		// 런타임에 메모리에 살아있는 진짜 슬롯 위젯을 콕 집어 꺼냅니다.
		UUI_InventorySlot* SlotWidget = Cast<UUI_InventorySlot>(GDP_ItemGrid->GetChildAt(i));
		if (SlotWidget)
		{
			// 슬롯 위젯을 부수지 않고, 내부의 UpdateSlot 함수만 다시 호출하여 이미지와 숫자만 싹 갈아 끼웁니다
			if (i < ItemIDs.Num())
			{
				FName Key = ItemIDs[i];
				SlotWidget->UpdateSlot(Key, CurrentItems[Key]);
			}
			else
			{
				// 데이터가 없는 남은 빈 칸들 깔끔하게 초기화
				SlotWidget->UpdateSlot(NAME_None, 0);
			}
		}
	}
}