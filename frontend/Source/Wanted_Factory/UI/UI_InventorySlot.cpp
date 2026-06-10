#include "UI/UI_InventorySlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UUI_InventorySlot::UpdateSlot(FName ItemID, int32 Count)
{
	if (ItemID.IsNone() || Count <= 0)
	{
		// 아이템이 없으면 슬롯을 비우거나 숨김 처리
		TXT_ItemCount->SetText(FText::GetEmpty());
		IMG_ItemIcon->SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	IMG_ItemIcon->SetVisibility(ESlateVisibility::Visible);
	TXT_ItemCount->SetText(FText::FromString(FString::Printf(TEXT("%d"), Count)));

	// 아이템 ID별 텍스처 매핑 로직을 여기에 구현하여 이미지를 세팅
}