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
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	// 6번 제외
	UPROPERTY(meta = (BindWidget)) class UButton* BTN_Slot_1_Storage;
	UPROPERTY(meta = (BindWidget)) class UButton* BTN_Slot_2_Conveyor;
	UPROPERTY(meta = (BindWidget)) class UButton* BTN_Slot_3_Smelter;
	UPROPERTY(meta = (BindWidget)) class UButton* BTN_Slot_4_Grinder;
	UPROPERTY(meta = (BindWidget)) class UButton* BTN_Slot_5_Miner;
    
	UPROPERTY(meta = (BindWidget)) class UButton* BTN_Slot_7_PowerPlant;
	UPROPERTY(meta = (BindWidget)) class UButton* BTN_Slot_8_PowerGridNode;
	UPROPERTY(meta = (BindWidget)) class UButton* BTN_Slot_9_PowerLine;
	UPROPERTY(meta = (BindWidget)) class UButton* BTN_Slot_0_MagneticShield;

private:
	// 각 버튼의 클릭 이벤트에 매핑될 내부 함수들
	UFUNCTION() void OnStorageClicked();
	UFUNCTION() void OnConveyorClicked();
	UFUNCTION() void OnSmelterClicked();
	UFUNCTION() void OnGrinderClicked();
	UFUNCTION() void OnMinerClicked();
    
	UFUNCTION() void OnPowerPlantClicked();
	UFUNCTION() void OnPowerGridNodeClicked();
	UFUNCTION() void OnPowerLineClicked();
	UFUNCTION() void OnMagneticShieldClicked();

	// 단축키 번호(SlotIndex)를 넘겨받아 처리할 공통 함수
	void ExecutePlacementMode(int32 SlotIndex);
	void RefreshSlotIcons();

	int32 CachedConveyorLevel = INDEX_NONE;
	int32 CachedSmelterLevel = INDEX_NONE;
	int32 CachedGrinderLevel = INDEX_NONE;
	int32 CachedMinerLevel = INDEX_NONE;
};
