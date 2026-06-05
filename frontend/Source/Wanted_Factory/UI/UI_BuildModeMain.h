#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI_BuildModeMain.generated.h"

UCLASS()
class WANTED_FACTORY_API UUI_BuildModeMain : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

protected:
	UPROPERTY(meta = (BindWidget)) class UButton* BTN_Slot_Machine;     // 1번: 기계
	UPROPERTY(meta = (BindWidget)) class UButton* BTN_Slot_Conveyor;    // 0번: 컨베이어
	UPROPERTY(meta = (BindWidget)) class UButton* BTN_Slot_PowerNode;   // 9번: 전력 노드
	UPROPERTY(meta = (BindWidget)) class UButton* BTN_Slot_Shield;      // 8번: 쉴드
	UPROPERTY(meta = (BindWidget)) class UButton* BTN_Slot_PowerPlant;  // 7번: 발전소
	UPROPERTY(meta = (BindWidget)) class UButton* BTN_Slot_PowerLine;   // -번: 전선

private:
	UFUNCTION() void OnMachineClicked();
	UFUNCTION() void OnConveyorClicked();
	UFUNCTION() void OnPowerNodeClicked();
	UFUNCTION() void OnShieldClicked();
	UFUNCTION() void OnPowerPlantClicked();
	UFUNCTION() void OnPowerLineClicked();
	void ExecutePlacementMode(int32 SlotIndex);
};