#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Machines/WarehousePort.h" 
#include "MachineBase.h"
#include "Machines/MachineTable.h"
#include "Machines/LiquidTank.h"    
#include "UI_WarehouseInteract.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWarehouseClosed);

UCLASS()
class WANTED_FACTORY_API UUI_WarehouseInteract : public UUserWidget
{
    GENERATED_BODY()

protected:
    UUI_WarehouseInteract(const FObjectInitializer& ObjectInitializer);

    // --- 기계 공통 및 출력(Output) 관련 위젯만 바인딩 ---
    UPROPERTY(meta = (BindWidget)) class UImage* IMG_MachinePreview;
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_MachineName;
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_MachineState;
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_DurabilityPercent;
    UPROPERTY(meta = (BindWidget)) class UProgressBar* PB_Durability;
    UPROPERTY(meta = (BindWidget)) class UButton* BTN_Close;
    UPROPERTY(meta = (BindWidget)) class UButton* BTN_Repair;

    // 출력(보관함) UI들
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_OutputName;
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_OutputCount;
    UPROPERTY(meta = (BindWidget)) class UProgressBar* PB_OutputBuffer;
    UPROPERTY(meta = (BindWidget)) class UBorder* B_OutputDropZone; // 마우스 센서 패드
    UPROPERTY(meta = (BindWidget)) class UImage* IMG_OutputIcon;
    UPROPERTY(meta = (BindWidget)) class UProgressBar* PB_CraftingProgress;
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_ProgressPercent;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data") class UDataTable* MachineDataTable;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data") class UDataTable* ResourceDataTable;

    class AMachineBase* TargetMachine;
    FName ManualDroppedOutputItemID = NAME_None; // 유저가 수동 지정한 아이템 기억 주머니

    FName LastOutputVisualItemID = NAME_None;

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // UpdateOutputUI
    void UpdateOutputUI(FName ItemName, int32 CurrentAmount, int32 MaxAmount);
    void UpdateMachineName(FString MachineName);
    void UpdateMachineState(FString StateText, FLinearColor StateColor);
    void UpdateDurabilityUI(float CurrentDurability, float MaxDurability);
    void UpdateCraftingProgress(float Percent);
    
    UFUNCTION() void OnCloseClicked();
    UFUNCTION() void OnRepairClicked();

    // 드래그 앤 드롭 수용
    virtual bool NativeOnDrop(const FGeometry& MyGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    // 출력 칸에서 아이템을 집어 가방으로 던지기 위한 마우스 인풋 오버라이드
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;

    // 아이템을 뺏을 때 기계 가동을 취소하고 품목을 초기화하는 함수
    void CancelMachineProcess();
public:
    void SetTargetMachine(AMachineBase* InMachine);
    
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnWarehouseClosed OnClosed;
};
