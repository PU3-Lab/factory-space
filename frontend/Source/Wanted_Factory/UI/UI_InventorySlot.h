#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI_InventorySlot.generated.h"

UCLASS()
class WANTED_FACTORY_API UUI_InventorySlot : public UUserWidget
{
	GENERATED_BODY()

protected:
	// 아이콘 이미지
	UPROPERTY(meta = (BindWidget))
	class UImage* IMG_ItemIcon;

	// 아이템 개수 텍스트
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* TXT_ItemCount;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	class UDataTable* ResourceDataTable;
	
	FName CurrentSlotItemID = NAME_None;
	int32 CurrentSlotItemCount = 0;

	// 드래그가 감지되었을 때 엔진이 호출해 주는 가상 함수 오버라이드
	virtual void NativeOnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& InPointerEvent,
									  UDragDropOperation*& OutOperation) override;

public:
	// 서브시스템 데이터를 받아 슬롯의 시각적 정보 갱신
	void UpdateSlot(FName ItemID, int32 Count);
};