#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI_MoldingMachineInteract.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMoldingMachineClosedSignature);

class AMoldingMachine;
class UImage;
class UTextBlock;
class UProgressBar;
class UButton;
class UComboBoxString;

UCLASS()
class WANTED_FACTORY_API UUI_MoldingMachineInteract : public UUserWidget
{
    GENERATED_BODY()

public:
    // 타겟 성형기를 주입하는 함수
    void SetTargetMachine(class AMachineBase* InMachine);

    UPROPERTY(BlueprintAssignable, Category = "MoldingMachine | UI")
    FMoldingMachineClosedSignature OnClosed;
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    
    UPROPERTY(meta = (BindWidget)) UImage* IMG_MachinePreview;
    UPROPERTY(meta = (BindWidget)) UTextBlock* TXT_MachineName;
    UPROPERTY(meta = (BindWidget)) UTextBlock* TXT_MachineState;

    UPROPERTY(meta = (BindWidget)) UProgressBar* PB_CraftingProgress;
    UPROPERTY(meta = (BindWidget)) UTextBlock* TXT_ProgressPercent;
    UPROPERTY(meta = (BindWidget)) UProgressBar* PB_Durability;
    UPROPERTY(meta = (BindWidget)) UTextBlock* TXT_DurabilityPercent;
    UPROPERTY(meta = (BindWidget)) UButton* BTN_Repair;

    // 1인풋 1아웃풋용 재료/결과물 슬롯
    UPROPERTY(meta = (BindWidget)) UTextBlock* TXT_InputName;
    UPROPERTY(meta = (BindWidget)) UTextBlock* TXT_InputCount;
    UPROPERTY(meta = (BindWidget)) UProgressBar* PB_InputBuffer;
    UPROPERTY(meta = (BindWidget)) UImage* IMG_InputIcon;

    UPROPERTY(meta = (BindWidget)) UTextBlock* TXT_OutputName;
    UPROPERTY(meta = (BindWidget)) UTextBlock* TXT_OutputCount;
    UPROPERTY(meta = (BindWidget)) UProgressBar* PB_OutputBuffer;
    UPROPERTY(meta = (BindWidget)) UImage* IMG_OutputIcon;

    // 드롭다운 메뉴 위젯
    UPROPERTY(meta = (BindWidget))
    UComboBoxString* CBS_MoldingShape;

private:
    UPROPERTY()
    AMoldingMachine* TargetMoldingMachine;

    // 드롭다운 메뉴 값이 바뀌었을 때 실행될 델리게이트 함수
    UFUNCTION()
    void HandleOnShapeChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
};
