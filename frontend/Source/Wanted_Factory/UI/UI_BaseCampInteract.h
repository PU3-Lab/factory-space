#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI_BaseCampInteract.generated.h"

class AMachineBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBaseCampInteractClosedSignature);

UENUM(BlueprintType)
enum class EBaseCampSubMode : uint8
{
	FactoryStatus,
	LevelUpgrade
};

UCLASS()
class WANTED_FACTORY_API UUI_BaseCampInteract : public UUserWidget
{
	GENERATED_BODY()

public:
	
	UPROPERTY(BlueprintAssignable, Category = "BaseCamp UI")
	FBaseCampInteractClosedSignature OnClosed;

	UFUNCTION(BlueprintCallable, Category = "BaseCamp UI")
	void SetTargetMachine(AMachineBase* InMachine);

	UFUNCTION(BlueprintCallable, Category = "BaseCamp UI")
	void RefreshFactoryStatus();

	void RefreshAllUpgradeNodes();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget)) class UButton* BTN_Tab_FactoryStatus;
	UPROPERTY(meta = (BindWidget)) class UButton* BTN_Tab_LevelUpgrade;
	UPROPERTY(meta = (BindWidget)) class UWidgetSwitcher* WS_SubPaneSwitcher;

	UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_PowerStatus;
	UPROPERTY(meta = (BindWidget)) class UScrollBox* SB_ResourceList;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseCamp UI|Config")
	TSubclassOf<class UUI_FactoryStatusRow> FactoryStatusRowClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseCamp UI|Config")
	class UDataTable* ResourceDataTable;

	UFUNCTION() void OnStatusTabClicked();
	UFUNCTION() void OnUpgradeTabClicked();
	void SwitchSubPaneMode(EBaseCampSubMode NewMode);

private:
	UPROPERTY()
	AMachineBase* TargetBaseCamp;
};