#pragma once

#include "CoreMinimal.h"
#include "MachineBase.h"
#include "Blueprint/UserWidget.h"
#include "UI_MachineInteract.generated.h"

class UTextBlock;
class UProgressBar;
class UButton;

// 위젯이 닫힐 때 1회 통지(파라미터 없음). 구독자(예: OJJ_Player)가 입력모드/커서를 즉시 복원하는 용도.
// LDJ 협의 완료 — 추가만(기존 닫기 로직 OnCloseClicked/RemoveFromParent 무수정).
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInteractClosed);

UCLASS()
class WANTED_FACTORY_API UUI_MachineInteract : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 모든 닫힘 경로(BTN_Close·외부 RemoveFromParent)를 NativeDestruct 한 곳에서 커버해 OnClosed를 1회 Broadcast.
	virtual void NativeDestruct() override;

	// 위젯 파괴 직전 1회 브로드캐스트되는 닫힘 통지. BlueprintAssignable이라 BP에서도 바인딩 가능.
	UPROPERTY(BlueprintAssignable, Category = "Machine Interact")
	FOnInteractClosed OnClosed;

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
	
	UFUNCTION(BlueprintCallable, Category = "Factory UI")
	void UpdateDurabilityUI(float CurrentDurability, float MaxDurability);

protected:
	// --- 입력(Input) 위젯 ---
	UPROPERTY(meta = (BindWidget)) class UImage* IMG_InputIcon;
	UPROPERTY(meta = (BindWidget)) UTextBlock* TXT_InputName;
	UPROPERTY(meta = (BindWidget)) UTextBlock* TXT_InputCount;
	UPROPERTY(meta = (BindWidget)) UProgressBar* PB_InputBuffer;
	
	// --- 출력(Output) 위젯 ---
	UPROPERTY(meta = (BindWidget)) class UImage* IMG_OutputIcon;
	UPROPERTY(meta = (BindWidget)) UTextBlock* TXT_OutputName;
	UPROPERTY(meta = (BindWidget)) UTextBlock* TXT_OutputCount;
	UPROPERTY(meta = (BindWidget)) UProgressBar* PB_OutputBuffer;
	
	//--- 중앙(Progress & State) 위젯 ---
	UPROPERTY(meta = (BindWidget)) UProgressBar* PB_CraftingProgress;
	UPROPERTY(meta = (BindWidget)) UTextBlock* TXT_ProgressPercent;
	UPROPERTY(meta = (BindWidget)) UTextBlock* TXT_MachineState;
	UPROPERTY(meta = (BindWidget)) UTextBlock* TXT_MachineName;
	UPROPERTY(meta = (BindWidget))
	class UImage* IMG_MachinePreview;
	
	UPROPERTY(meta = (BindWidget))
	class UButton* BTN_Close;
	UPROPERTY(meta = (BindWidgetOptional))
	class UButton* BTN_Repair;
	
	// --- 내구도 관련 위젯 ---
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* TXT_DurabilityPercent;

	UPROPERTY(meta = (BindWidget))
	class UProgressBar* PB_Durability;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	class UDataTable* ResourceDataTable;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	class UDataTable* MachineDataTable;

private:
	// 테스트하기 위한 임시 변수
	float DummyProgress = 0.0f; 
	
	UPROPERTY()
	TObjectPtr<AMachineBase> TargetMachine;
	
	UFUNCTION()
	void OnCloseClicked();

	UFUNCTION()
	void OnRepairClicked();
};
