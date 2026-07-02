#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI_BaseCampInteract.generated.h"

class AMachineBase;
class UUniformGridPanel;
class UDragDropOperation;
class UBorder;
class UButton;
class UImage;
class UTextBlock;
class UProgressBar;
class UFactoryAgentClientSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBaseCampInteractClosedSignature);

UENUM(BlueprintType)
enum class EBaseCampSubMode : uint8
{
    FactoryStatus,
    LevelUpgrade,
    newMaterial
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

    UFUNCTION(BlueprintCallable, Category = "BaseCamp UI")
    bool RequestMaterialGeneration();

    UFUNCTION(BlueprintCallable, Category = "BaseCamp UI")
    bool RequestProcessOptimization();

    void RefreshAllUpgradeNodes();
    bool TakeInputItemForInventoryDrop(FName ItemID);
    void RefreshCampInventoryAfterInventoryDrop();
    void ReturnInputItemFromFailedDrop(FName ItemID);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual bool NativeOnDrop(const FGeometry& MyGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& InPointerEvent, UDragDropOperation*& OutOperation) override;

    // --- 기본 상단 탭 및 스위처 ---
    UPROPERTY(meta = (BindWidget)) class UButton* BTN_Tab_FactoryStatus;
    UPROPERTY(meta = (BindWidget)) class UButton* BTN_Tab_LevelUpgrade;
    UPROPERTY(meta = (BindWidget)) class UButton* BTN_Tab_newMaterial;
    UPROPERTY(meta = (BindWidgetOptional)) class UButton* BTN_RequestMaterialGeneration;
    UPROPERTY(meta = (BindWidgetOptional)) class UButton* BTN_RequestOptimization;
    UPROPERTY(meta = (BindWidget)) class UWidgetSwitcher* WS_SubPaneSwitcher;

    // --- 공장 상태창 탭 관련 ---
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_PowerStatus;
    UPROPERTY(meta = (BindWidget)) class UProgressBar* PB_PowerStatus;
    UPROPERTY(meta = (BindWidget)) class UScrollBox* SB_ResourceList;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseCamp UI|Config")
    TSubclassOf<class UUI_FactoryStatusRow> FactoryStatusRowClass;

    // --- 신물질 합성 탭 관련 (드롭존 & 비주얼 컴포넌트 변수 통합) ---
    UPROPERTY(meta = (BindWidget)) UBorder* B_InputDropZone;
    UPROPERTY(meta = (BindWidget)) UBorder* B_InputDropZone_1;
    UPROPERTY(meta = (BindWidget)) UBorder* B_InputDropZone_2;
    UPROPERTY(meta = (BindWidget)) UBorder* B_OutputDropZone;

    UPROPERTY(meta = (BindWidget)) UImage* IMG_InputIcon_1;
    UPROPERTY(meta = (BindWidget)) UImage* IMG_InputIcon_2;
    UPROPERTY(meta = (BindWidget)) UImage* IMG_InputIcon_3;

    UPROPERTY(meta = (BindWidget)) UTextBlock* TXT_InputName_1;
    UPROPERTY(meta = (BindWidget)) UTextBlock* TXT_InputName_2;
    UPROPERTY(meta = (BindWidget)) UTextBlock* TXT_InputName_3;

    UPROPERTY(meta = (BindWidget)) UProgressBar* PB_InputBuffer_1;
    UPROPERTY(meta = (BindWidget)) UProgressBar* PB_InputBuffer_2;
    UPROPERTY(meta = (BindWidget)) UProgressBar* PB_InputBuffer_3;

    UPROPERTY(meta = (BindWidget)) UTextBlock* TXT_InputCount_1;
    UPROPERTY(meta = (BindWidget)) UTextBlock* TXT_InputCount_2;
    UPROPERTY(meta = (BindWidget)) UTextBlock* TXT_InputCount_3;

    UPROPERTY(meta = (BindWidget)) UImage* IMG_OutputIcon;
    UPROPERTY(meta = (BindWidget)) UTextBlock* TXT_OutputName;

    // --- 우측하단 가방 그리드 관련 ---
    UPROPERTY(meta = (BindWidget)) UUniformGridPanel* GDP_CampInventoryGrid;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseCamp UI|Config")
    TSubclassOf<class UUI_InventorySlot> InventorySlotClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseCamp UI|Config")
    class UDataTable* ResourceDataTable;

    // --- 내부 헬퍼 및 비주얼 잔상 주머니 ---
    FName ManualDroppedOutputItemID = NAME_None;
    FName LastInputVisualItemID_1 = NAME_None;
    FName LastInputVisualItemID_2 = NAME_None;
    FName LastInputVisualItemID_3 = NAME_None;
    FName LastOutputVisualItemID = NAME_None;
    FName DraggingInputItemID = NAME_None;

    UPROPERTY()
    UImage* DraggingInputIcon = nullptr;

    void RefreshCampInventoryGrid();
    void UpdateInputSlotUI(int32 SlotIndex, FName ItemName, int32 CurrentAmount, int32 MaxAmount);
    void UpdateOutputUI(FName ItemName, int32 CurrentAmount);
    
    UFUNCTION() void OnStatusTabClicked();
    UFUNCTION() void OnUpgradeTabClicked();
    UFUNCTION() void OnMaterialTabClicked();
    UFUNCTION() void OnRequestMaterialGenerationClicked();
    UFUNCTION() void OnRequestOptimizationClicked();
    UFUNCTION() void HandleMaterialGenerationResponse(const FFactoryMaterialGenerationResponse& Response);
    void SwitchSubPaneMode(EBaseCampSubMode NewMode);

private:
    UPROPERTY()
    AMachineBase* TargetBaseCamp;

    FName LatestMaterialGenerationOutputItemID = NAME_None;
};
