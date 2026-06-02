#pragma once

#include "CoreMinimal.h"
#include "MachineBase.h"
#include "Blueprint/UserWidget.h"
#include "UI_MachineInteract.generated.h"

class UTextBlock;
class UProgressBar;

UCLASS()
class WANTED_FACTORY_API UUI_MachineInteract : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	
	UFUNCTION(BlueprintCallable, Category = "Factory UI")
	void UpdateInputUI(FName ItemName, int32 CurrentAmount, int32 MaxAmount);

	UFUNCTION(BlueprintCallable, Category = "Factory UI")
	void UpdateOutputUI(FName ItemName, int32 CurrentAmount, int32 MaxAmount);

	UFUNCTION(BlueprintCallable, Category = "Factory UI")
	void UpdateMachineState(FString StateText, FLinearColor StateColor);

	UFUNCTION(BlueprintCallable, Category = "Factory UI")
	void UpdateCraftingProgress(float Percent);
	
	UFUNCTION(BlueprintCallable, Category = "Factory UI")
	void SetTargetMachine(AMachineBase* InMachine);
	
	UFUNCTION(BlueprintCallable, Category = "Factory UI")
	void UpdateMachineName(FString MachineName);

protected:
	// --- 입력(Input) 위젯 ---
	UPROPERTY(meta = (BindWidget)) UTextBlock* TXT_InputName;
	UPROPERTY(meta = (BindWidget)) UTextBlock* TXT_InputCount;
	UPROPERTY(meta = (BindWidget)) UProgressBar* PB_InputBuffer;
	
	// --- 출력(Output) 위젯 ---
	UPROPERTY(meta = (BindWidget)) UTextBlock* TXT_OutputName;
	UPROPERTY(meta = (BindWidget)) UTextBlock* TXT_OutputCount;
	UPROPERTY(meta = (BindWidget)) UProgressBar* PB_OutputBuffer;
	
	//--- 중앙(Progress & State) 위젯 ---
	UPROPERTY(meta = (BindWidget)) UProgressBar* PB_CraftingProgress;
	UPROPERTY(meta = (BindWidget)) UTextBlock* TXT_ProgressPercent;
	UPROPERTY(meta = (BindWidget)) UTextBlock* TXT_MachineState;
	
	UPROPERTY(meta = (BindWidget)) UTextBlock* TXT_MachineName;
	
	UPROPERTY(meta = (BindWidget))
	class UButton* BTN_Close;

private:
	// 테스트하기 위한 임시 변수
	float DummyProgress = 0.0f; 
	
	UPROPERTY()
	TObjectPtr<AMachineBase> TargetMachine;
	
	UFUNCTION()
	void OnCloseClicked();
};