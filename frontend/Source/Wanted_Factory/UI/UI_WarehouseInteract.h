#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI_WarehouseInteract.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWarehouseClosed);

UCLASS()
class WANTED_FACTORY_API UUI_WarehouseInteract : public UUserWidget
{
    GENERATED_BODY()

protected:
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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data") class UDataTable* MachineDataTable;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data") class UDataTable* ResourceDataTable;

    class AMachineBase* TargetMachine;
    FName ManualDroppedOutputItemID = NAME_None; // 유저가 수동 지정한 아이템 기억 주머니

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // UpdateOutputUI
    void UpdateOutputUI(FName ItemName, int32 CurrentAmount, int32 MaxAmount);
    void UpdateMachineName(FString MachineName);
    void UpdateMachineState(FString StateText, FLinearColor StateColor);
    void UpdateDurabilityUI(float CurrentDurability, float MaxDurability);

    UFUNCTION() void OnCloseClicked();
    UFUNCTION() void OnRepairClicked();

    // 드래그 앤 드롭 수용
    virtual bool NativeOnDrop(const FGeometry& MyGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

public:
    void SetTargetMachine(AMachineBase* InMachine);
    
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnWarehouseClosed OnClosed;
};