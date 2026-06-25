#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Recipe/RecipeTable.h"
#include "MachineBase.h"
#include "UI_SynthesizerInteract.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSynthesizerClosed);

UCLASS()
class WANTED_FACTORY_API UUI_SynthesizerInteract : public UUserWidget
{
    GENERATED_BODY()

protected:
    UUI_SynthesizerInteract(const FObjectInitializer& ObjectInitializer);

    // --- 기본 상단 정보 바인딩 ---
    UPROPERTY(meta = (BindWidget)) class UImage* IMG_MachinePreview;
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_MachineName;
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_MachineState;

    // --- [합성기 핵심] 3개의 입력(Ingredient) 슬롯 위젯들 바인딩 ---
    // 1번 입력 슬롯
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_InputName_1;
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_InputCount_1;
    UPROPERTY(meta = (BindWidget)) class UProgressBar* PB_InputBuffer_1;
    UPROPERTY(meta = (BindWidget)) class UImage* IMG_InputIcon_1;

    // 2번 입력 슬롯
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_InputName_2;
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_InputCount_2;
    UPROPERTY(meta = (BindWidget)) class UProgressBar* PB_InputBuffer_2;
    UPROPERTY(meta = (BindWidget)) class UImage* IMG_InputIcon_2;

    // 3번 입력 슬롯
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_InputName_3;
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_InputCount_3;
    UPROPERTY(meta = (BindWidget)) class UProgressBar* PB_InputBuffer_3;
    UPROPERTY(meta = (BindWidget)) class UImage* IMG_InputIcon_3;

    // --- 중앙 진행도 및 하단 내구도 바인딩 ---
    UPROPERTY(meta = (BindWidget)) class UProgressBar* PB_CraftingProgress;
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_ProgressPercent;
    UPROPERTY(meta = (BindWidget)) class UProgressBar* PB_Durability;
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_DurabilityPercent;
    UPROPERTY(meta = (BindWidget)) class UButton* BTN_Repair;

    // --- 우측 출력(Output) 슬롯 바인딩 ---
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_OutputName;
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_OutputCount;
    UPROPERTY(meta = (BindWidget)) class UProgressBar* PB_OutputBuffer;
    UPROPERTY(meta = (BindWidget)) class UBorder* B_OutputDropZone;
    UPROPERTY(meta = (BindWidget)) class UImage* IMG_OutputIcon;

    // --- 데이터 테이블 ---
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data") class UDataTable* MachineDataTable;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data") class UDataTable* ResourceDataTable;

    // --- 내부 참조 변수 및 비주얼 잔상 메모리 주머니 ---
    class AMachineBase* TargetMachine;
    FName ManualDroppedOutputItemID = NAME_None;
    
    FName LastInputVisualItemID_1 = NAME_None;
    FName LastInputVisualItemID_2 = NAME_None;
    FName LastInputVisualItemID_3 = NAME_None;
    FName LastOutputVisualItemID = NAME_None;

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // 내부 렌더링 헬퍼 래퍼 함수들
    void UpdateInputSlotUI(int32 SlotIndex, FName ItemName, int32 CurrentAmount, int32 MaxAmount);
    void UpdateOutputUI(FName ItemName, int32 CurrentAmount, int32 MaxAmount);
    void UpdateMachineState(FString StateText, FLinearColor StateColor);
    void UpdateCraftingProgress(float Percent);
    void UpdateDurabilityUI(float CurrentDurability, float MaxDurability);
    void UpdateMachineName(const FText& MachineName);

    UFUNCTION() void OnRepairClicked();

    // 드래그 앤 드롭 투입 처리
    virtual bool NativeOnDrop(const FGeometry& MyGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

public:
    void SetTargetMachine(AMachineBase* InMachine);

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnSynthesizerClosed OnClosed;
};