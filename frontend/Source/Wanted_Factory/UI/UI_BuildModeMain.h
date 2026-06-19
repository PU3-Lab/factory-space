#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OJJ_BuildController.h"
#include "UI_BuildModeMain.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UWidgetAnimation;

UENUM(BlueprintType)
enum class EBuildSubMode : uint8
{
    Machine,
    Power,
    Structure
};

USTRUCT(BlueprintType)
struct FHotbarSlotData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EOJJ_BuildPlacementMode PlacementMode = EOJJ_BuildPlacementMode::Machine;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName SubsystemKey = NAME_None;
    
    // 모드가 바뀔 때 슬롯에 띄워줄 진짜 한글 이름표
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FText DisplayName = FText::GetEmpty();
};

UCLASS()
class WANTED_FACTORY_API UUI_BuildModeMain : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UFUNCTION(BlueprintCallable, Category = "Build Mode")
    void SwitchBuildSubMode(EBuildSubMode NewMode);

    // 현재 카테고리(CurrentSubMode)의 SlotIndex(1~10)번 슬롯을 실행 — 슬롯 버튼 클릭(OnSlotNClicked)과 동일 경로.
    // 키보드 숫자키 연동을 위해 노출(OJJ_Player가 호출). 로직은 내부 CurrentSubMode/레지스트리 기준.
    UFUNCTION(BlueprintCallable, Category = "Build Mode")
    void ExecutePlacementMode(int32 SlotIndex);

protected:
    UPROPERTY(meta = (BindWidget)) UButton* BTN_SubMode_Machine;
    UPROPERTY(meta = (BindWidget)) UButton* BTN_SubMode_Power;
    UPROPERTY(meta = (BindWidget)) UButton* BTN_SubMode_Structure;

    UPROPERTY(meta = (BindWidget)) class UWidget* HB_HotbarContainer;

    UPROPERTY(meta = (BindWidgetAnim), Transient) UWidgetAnimation* Anim_HotbarOut;
    UPROPERTY(meta = (BindWidgetAnim), Transient) UWidgetAnimation* Anim_HotbarIn;

    UPROPERTY(meta = (BindWidget))
    UImage* IMG_SelectedPreview;
    UPROPERTY(meta = (BindWidget))
    UTextBlock* TXT_SelectedName;
private:
    EBuildSubMode CurrentSubMode = EBuildSubMode::Machine;
    EBuildSubMode PendingSubMode = EBuildSubMode::Machine;

    // 가비지 컬렉션(GC) 보호막이 완벽히 씌워진 핫바 3대 컴포넌트 배열
    UPROPERTY() TArray<UButton*> HotbarButtons;
    UPROPERTY() TArray<UImage*> HotbarIcons;
    UPROPERTY() TArray<UTextBlock*> HotbarTexts;

    TMap<EBuildSubMode, TArray<FHotbarSlotData>> SubModeHotbarRegistry;

    UFUNCTION() void OnMachineModeClicked();
    UFUNCTION() void OnPowerModeClicked();
    UFUNCTION() void OnStructureModeClicked();

    UFUNCTION() void OnSlot1Clicked();
    UFUNCTION() void OnSlot2Clicked();
    UFUNCTION() void OnSlot3Clicked();
    UFUNCTION() void OnSlot4Clicked();
    UFUNCTION() void OnSlot5Clicked();
    UFUNCTION() void OnSlot6Clicked();
    UFUNCTION() void OnSlot7Clicked();
    UFUNCTION() void OnSlot8Clicked();
    UFUNCTION() void OnSlot9Clicked();
    UFUNCTION() void OnSlot10Clicked();

    UFUNCTION() void OnHotbarOutFinished();

    void InitializeHotbarRegistry();
    void RefreshHotbarSlotsVisual();
    void UpdateSelectedPreview();
    TMap<FName, int32> CachedMachineLevels;
};