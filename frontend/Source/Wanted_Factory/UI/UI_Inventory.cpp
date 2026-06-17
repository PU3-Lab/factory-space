#include "UI/UI_Inventory.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Engine/DataTable.h"
#include "Layout/Margin.h"               
#include "PlayerWarehouseSubsystem.h"
#include "Resource/ResourceData.h"
#include "UI/UI_InventorySlot.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/CanvasPanelSlot.h"

UUI_Inventory::UUI_Inventory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UDataTable> ResourceTableFinder(
		TEXT("/Game/DataTable/DT_ResourceData.DT_ResourceData"));
	if (ResourceTableFinder.Succeeded())
	{
		ResourceDataTable = ResourceTableFinder.Object;
	}
}

void UUI_Inventory::SetItemFormFilter(FName FormFilter)
{
	ItemFormFilter = FormFilter;
}

bool UUI_Inventory::IsAllowedItem(FName ItemID) const
{
	if (ItemID.IsNone() || ItemFormFilter.IsNone())
	{
		return !ItemID.IsNone();
	}

	if (!ResourceDataTable)
	{
		return false;
	}

	const FResourceData* Resource = ResourceDataTable->FindRow<FResourceData>(ItemID, TEXT("Inventory.IsAllowedItem"));
	return Resource && Resource->form == ItemFormFilter;
}

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
	for (const TPair<FName, int32>& Item : CurrentItems)
	{
		if (Item.Value > 0 && IsAllowedItem(Item.Key))
		{
			ItemIDs.Add(Item.Key);
		}
	}
	
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
	for (const TPair<FName, int32>& Item : CurrentItems)
	{
		if (Item.Value > 0 && IsAllowedItem(Item.Key))
		{
			ItemIDs.Add(Item.Key);
		}
	}

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
void UUI_Inventory::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
	{
		DefaultPosition = CanvasSlot->GetPosition();
	}
}

void UUI_Inventory::AdjustInventoryLayout(bool bIsWarehouseOpen)
{
	if (bIsWarehouseOpen) return;
	
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;

	// 현재 플레이어 모니터의 실시간 해상도 크기 수집
	int32 ScreenWidth, ScreenHeight;
	PC->GetViewportSize(ScreenWidth, ScreenHeight);

	// 화면 완벽한 데드 센터(Dead Center) 정중앙 배치 좌표 계산
	float CenteredX = ScreenWidth * 0.5f; 
	float CenteredY = ScreenHeight * 0.5f;

	// 뷰포트 위치 및 피벗 정중앙 고정
	SetPositionInViewport(FVector2D(CenteredX, CenteredY), true);
	SetAlignmentInViewport(FVector2D(0.5f, 0.5f)); 
    
	UE_LOG(LogTemp, Log, TEXT("인벤토리 정중앙 배치 "));
}