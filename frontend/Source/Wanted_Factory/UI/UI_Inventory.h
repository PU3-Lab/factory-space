#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI_Inventory.generated.h"

UCLASS()
class WANTED_FACTORY_API UUI_Inventory : public UUserWidget
{
	GENERATED_BODY()

protected:
	// 그리드 패널
	UPROPERTY(meta = (BindWidget))
	class UUniformGridPanel* GDP_ItemGrid;
	
	UPROPERTY(EditAnywhere, Category = "Inventory | UI")
	TSubclassOf<UUserWidget> SlotWidgetClass;

public:
	// 인벤토리가 열릴 때마다 창고 서브시스템을 읽어와서 화면을 갱신
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RefreshInventoryWindow();
	void UpdateSlotQuantitiesOnly();
};