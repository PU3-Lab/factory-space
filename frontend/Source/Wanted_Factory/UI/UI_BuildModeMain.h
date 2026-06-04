#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/DataTable.h"
#include "UI_BuildModeMain.generated.h"

UCLASS()
class UUI_BuildModeMain : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	
	UFUNCTION(BlueprintCallable, Category = "Quickslot")
	void HandleQuickslotClicked(int32 SlotIndex);

protected:
	UPROPERTY(meta = (BindWidget))
	class UHorizontalBox* HBox_QuickslotBar; 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FactoryData")
	class UDataTable* FactoryDataTable;

private:
	void OnKey1Pressed();
	void OnKey2Pressed();
	void OnKey3Pressed();
	void OnKey4Pressed();
	void OnKey5Pressed();
	void OnKey6Pressed();
	void OnKey7Pressed();
	void OnKey8Pressed();
	void OnKey9Pressed();
	void OnKey0Pressed();
};