#include "UI_Quickslot.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UUI_Quickslot::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Slot)
	{
		Btn_Slot->OnClicked.AddDynamic(this, &UUI_Quickslot::HandleSlotClicked);
	}
}

void UUI_Quickslot::InitSlot(int32 InSlotIndex, FText KeyText)
{
	SlotIndex = InSlotIndex;
	if (Txt_KeyNumber)
	{
		Txt_KeyNumber->SetText(KeyText);
	}
	ClearSlot();
}

void UUI_Quickslot::SetBuildingData(const FFactoryData& InData)
{
	AssignedData = InData;
	bIsEmpty = false;

	if (Img_Icon && AssignedData.FactoryIcon)
	{
		Img_Icon->SetBrushFromTexture(AssignedData.FactoryIcon);
		Img_Icon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void UUI_Quickslot::ClearSlot()
{
	bIsEmpty = true;
	if (Img_Icon)
	{
		Img_Icon->SetBrushFromTexture(nullptr); 
		Img_Icon->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UUI_Quickslot::HandleSlotClicked()
{
	if (!bIsEmpty)
	{
		OnSlotClickedDelegate.Broadcast(SlotIndex);
	}
}