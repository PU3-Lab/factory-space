#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FactorySpaceTypes.h" 
#include "UI_Quickslot.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuickslotClicked, int32, SlotIndex);

UCLASS()
class UUI_Quickslot : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	void InitSlot(int32 InSlotIndex, FText KeyText);
	
	void SetBuildingData(const FFactoryData& InData);
    
	void ClearSlot();

	UPROPERTY(BlueprintAssignable)
	FOnQuickslotClicked OnSlotClickedDelegate;
	
	const FFactoryData& GetAssignedData() const { return AssignedData; }
	bool IsEmpty() const { return bIsEmpty; }

protected:
	UPROPERTY(meta = (BindWidget))
	class UButton* Btn_Slot;

	UPROPERTY(meta = (BindWidget))
	class UImage* Img_Icon;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* Txt_KeyNumber;

private:
	UFUNCTION()
	void HandleSlotClicked();

	int32 SlotIndex;
	bool bIsEmpty = true;
	
	FFactoryData AssignedData; 
};