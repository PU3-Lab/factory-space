#include "UI/UI_BaseCampInteract.h"
#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/Border.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Blueprint/WidgetTree.h"
#include "UI_UpgradeNode.h"
#include "UI/UI_InventorySlot.h"
#include "PlayerWarehouseSubsystem.h"
#include "MachineBase.h"
#include "FactoryManagerSubsystem.h"
#include "UI_FactoryStatusRow.h"
#include "Resource/ResourceData.h"
#include "ItemDragDropOperation.h"
#include "UI/UIInteractDisplayHelpers.h" // 🌟 기존 오리지널 텍스트 변환 헬퍼 인클루드
#include "Blueprint/DragDropOperation.h" 

using namespace UIInteractHelpers;

void UUI_BaseCampInteract::SetTargetMachine(AMachineBase* InMachine)
{
    TargetBaseCamp = InMachine;
    ManualDroppedOutputItemID = NAME_None;
    LastInputVisualItemID_1 = NAME_None;
    LastInputVisualItemID_2 = NAME_None;
    LastInputVisualItemID_3 = NAME_None;
    LastOutputVisualItemID = NAME_None;
}

void UUI_BaseCampInteract::NativeConstruct()
{
    Super::NativeConstruct();

    if (BTN_Tab_FactoryStatus)
    {
       BTN_Tab_FactoryStatus->OnClicked.RemoveDynamic(this, &UUI_BaseCampInteract::OnStatusTabClicked);
       BTN_Tab_FactoryStatus->OnClicked.AddDynamic(this, &UUI_BaseCampInteract::OnStatusTabClicked);
    }

    if (BTN_Tab_LevelUpgrade)
    {
       BTN_Tab_LevelUpgrade->OnClicked.RemoveDynamic(this, &UUI_BaseCampInteract::OnUpgradeTabClicked);
       BTN_Tab_LevelUpgrade->OnClicked.AddDynamic(this, &UUI_BaseCampInteract::OnUpgradeTabClicked);
    }
    
    if (BTN_Tab_newMaterial)
    {
        BTN_Tab_newMaterial->OnClicked.RemoveDynamic(this, &UUI_BaseCampInteract::OnMaterialTabClicked);
        BTN_Tab_newMaterial->OnClicked.AddDynamic(this, &UUI_BaseCampInteract::OnMaterialTabClicked);
    }

    // 초기 화면 상태 지정
    SwitchSubPaneMode(EBaseCampSubMode::LevelUpgrade);
    RefreshAllUpgradeNodes();
}

void UUI_BaseCampInteract::NativeDestruct()
{
    OnClosed.Broadcast();
    Super::NativeDestruct();
}

// 🌟 [신설] 실시간 합성기 데이터 미러링 틱 연동
void UUI_BaseCampInteract::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!TargetBaseCamp || !WS_SubPaneSwitcher) return;

    // 최적화: 유저가 3번째 '신물질 합성(Index 2)' 탭을 보고 있을 때만 실시간 UI를 연산합니다.
    if (WS_SubPaneSwitcher->GetActiveWidgetIndex() != 2) return;

    // 오리지널 합성기 틱 로직을 그대로 거점 기계 컴포넌트에 매핑합니다.
    FRecipeTable Recipe = TargetBaseCamp->GetCurrentRecipe();
    TArray<FName> ExpectedInputs = { Recipe.InputItem1, Recipe.InputItem2, Recipe.InputItem3 };
    const TMap<FName, int32>& InputInv = TargetBaseCamp->GetInputInventory();

    if (Recipe.MachineType.IsNone())
    {
        int32 SlotIdx = 0;
        for (const auto& Pair : InputInv)
        {
            if (SlotIdx >= 3) break;
            if (Pair.Value > 0)
            {
                ExpectedInputs[SlotIdx] = Pair.Key;
                SlotIdx++;
            }
        }
    }

    int32 MaxInputAmount = TargetBaseCamp->GetMaxInput();
    UpdateInputSlotUI(1, ExpectedInputs[0], InputInv.FindRef(ExpectedInputs[0]), MaxInputAmount);
    UpdateInputSlotUI(2, ExpectedInputs[1], InputInv.FindRef(ExpectedInputs[1]), MaxInputAmount);
    UpdateInputSlotUI(3, ExpectedInputs[2], InputInv.FindRef(ExpectedInputs[2]), MaxInputAmount);

    FName OutputName = Recipe.OutputItem1;
    if (!ManualDroppedOutputItemID.IsNone()) OutputName = ManualDroppedOutputItemID;
    int32 OutputAmount = TargetBaseCamp->GetOutputBuffer().FindRef(OutputName);
    UpdateOutputUI(OutputName, OutputAmount, TargetBaseCamp->GetMaxOutput());
}

void UUI_BaseCampInteract::UpdateInputSlotUI(int32 SlotIndex, FName ItemName, int32 CurrentAmount, int32 MaxAmount)
{
    UTextBlock* TargetTXT_Name = (SlotIndex == 1) ? TXT_InputName_1 : ((SlotIndex == 2) ? TXT_InputName_2 : TXT_InputName_3);
    UTextBlock* TargetTXT_Count = (SlotIndex == 1) ? TXT_InputCount_1 : ((SlotIndex == 2) ? TXT_InputCount_2 : TXT_InputCount_3);
    UProgressBar* TargetPB_Buffer = (SlotIndex == 1) ? PB_InputBuffer_1 : ((SlotIndex == 2) ? PB_InputBuffer_2 : PB_InputBuffer_3);
    UImage* TargetIMG_Icon = (SlotIndex == 1) ? IMG_InputIcon_1 : ((SlotIndex == 2) ? IMG_InputIcon_2 : IMG_InputIcon_3);
    FName& LastVisualID = (SlotIndex == 1) ? LastInputVisualItemID_1 : ((SlotIndex == 2) ? LastInputVisualItemID_2 : LastInputVisualItemID_3);

    const FName DisplayItemName = ItemName.IsNone() ? LastVisualID : ItemName;
    if (!ItemName.IsNone()) LastVisualID = ItemName;

    if (TargetTXT_Name && TargetTXT_Count && TargetPB_Buffer)
    {
        TargetTXT_Name->SetText(GetResourceDisplayText(ResourceDataTable, DisplayItemName));
        TargetTXT_Count->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), CurrentAmount, MaxAmount)));
        TargetPB_Buffer->SetPercent((MaxAmount > 0) ? (float)CurrentAmount / MaxAmount : 0.0f);
    }

    if (TargetIMG_Icon)
    {
        // 🌟 이 조건문 덕분에 아이템이 없으면 하얀 네모가 뜨지 않고 완벽하게 투명(Hidden)해집니다!
        if (DisplayItemName.IsNone())
        {
            TargetIMG_Icon->SetVisibility(ESlateVisibility::Hidden);
            return;
        }

        TargetIMG_Icon->SetVisibility(ESlateVisibility::Visible);
        if (ResourceDataTable)
        {
            FResourceData* RowData = ResourceDataTable->FindRow<FResourceData>(DisplayItemName, TEXT("FindInputSlotIcon"));
            if (RowData)
            {
                UTexture2D* Tex = RowData->ImgAsset.IsValid() ? RowData->ImgAsset.Get() : RowData->ImgAsset.LoadSynchronous();
                if (Tex) TargetIMG_Icon->SetBrushFromTexture(Tex);
                TargetIMG_Icon->SetColorAndOpacity(CurrentAmount <= 0 ? FLinearColor(1.f, 1.f, 1.f, 0.15f) : FLinearColor::White);
            }
        }
    }
}

void UUI_BaseCampInteract::UpdateOutputUI(FName ItemName, int32 CurrentAmount, int32 MaxAmount)
{
    const FName DisplayItemName = ItemName.IsNone() ? LastOutputVisualItemID : ItemName;
    if (!ItemName.IsNone()) LastOutputVisualItemID = ItemName;

    if (TXT_OutputName && TXT_OutputCount && PB_OutputBuffer)
    {
        TXT_OutputName->SetText(GetResourceDisplayText(ResourceDataTable, DisplayItemName));
        TXT_OutputCount->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), CurrentAmount, MaxAmount)));
        PB_OutputBuffer->SetPercent((MaxAmount > 0) ? (float)CurrentAmount / MaxAmount : 0.0f);
    }

    if (IMG_OutputIcon)
    {
        if (DisplayItemName.IsNone())
        {
            IMG_OutputIcon->SetVisibility(ESlateVisibility::Hidden);
            return;
        }

        IMG_OutputIcon->SetVisibility(ESlateVisibility::Visible);
        if (ResourceDataTable)
        {
            FResourceData* RowData = ResourceDataTable->FindRow<FResourceData>(DisplayItemName, TEXT("FindOutputIcon"));
            if (RowData)
            {
                UTexture2D* Tex = RowData->ImgAsset.IsValid() ? RowData->ImgAsset.Get() : RowData->ImgAsset.LoadSynchronous();
                if (Tex) IMG_OutputIcon->SetBrushFromTexture(Tex);
                IMG_OutputIcon->SetColorAndOpacity((CurrentAmount <= 0 && DisplayItemName != ManualDroppedOutputItemID) ? FLinearColor(1.f, 1.f, 1.f, 0.15f) : FLinearColor::White);
            }
        }
    }
}

// 인벤토리 슬롯에서 드래그해서 신물질 기계로 떨어뜨릴 때의 핵심 처리 핸들러
bool UUI_BaseCampInteract::NativeOnDrop(const FGeometry& MyGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    UItemDragDropOperation* ItemDragOp = Cast<UItemDragDropOperation>(InOperation);
    if (!ItemDragOp || !TargetBaseCamp) return false;

    FName DroppedItemID = ItemDragOp->DraggedItemID;
    if (DroppedItemID.IsNone()) return false;

    // 1. 기계의 해당 아이템 입력 슬롯 수량이 이미 꽉 찼는지 검사
    int32 CurrentInputAmount = TargetBaseCamp->GetInputInventory().FindRef(DroppedItemID);
    if (CurrentInputAmount >= TargetBaseCamp->GetMaxInput()) return false;
    
    UGameInstance* GI = GetGameInstance();
    UPlayerWarehouseSubsystem* WarehouseSubsystem = GI ? GI->GetSubsystem<UPlayerWarehouseSubsystem>() : nullptr;
    
    // 2. 창고에서 1개 꺼내기 시도
    if (WarehouseSubsystem && WarehouseSubsystem->TakeItem(DroppedItemID, 1)) 
    {
        if (!TargetBaseCamp->AddItem(DroppedItemID, 1))
        {
            WarehouseSubsystem->AddItem(DroppedItemID, 1);
            return false;
        }

        TargetBaseCamp->TryStartProcess();
        RefreshCampInventoryGrid();
        return true; 
    }

    return Super::NativeOnDrop(MyGeometry, InDragDropEvent, InOperation);
}

// (나머지 OnStatusTabClicked, OnUpgradeTabClicked, OnMaterialTabClicked, SwitchSubPaneMode, RefreshFactoryStatus, RefreshAllUpgradeNodes, RefreshCampInventoryGrid 코드는 기존 구현과 완전히 동일하므로 유지)
void UUI_BaseCampInteract::OnStatusTabClicked() { SwitchSubPaneMode(EBaseCampSubMode::FactoryStatus); }
void UUI_BaseCampInteract::OnUpgradeTabClicked() { SwitchSubPaneMode(EBaseCampSubMode::LevelUpgrade); }
void UUI_BaseCampInteract::OnMaterialTabClicked() { SwitchSubPaneMode(EBaseCampSubMode::newMaterial); }

void UUI_BaseCampInteract::SwitchSubPaneMode(EBaseCampSubMode NewMode)
{
    if (!WS_SubPaneSwitcher) return;
    int32 TargetIndex = 0;
    switch (NewMode)
    {
    case EBaseCampSubMode::FactoryStatus: TargetIndex = 0; break;
    case EBaseCampSubMode::LevelUpgrade:  TargetIndex = 1; break;
    case EBaseCampSubMode::newMaterial:   TargetIndex = 2; break;
    }
    WS_SubPaneSwitcher->SetActiveWidgetIndex(TargetIndex);
    
    if (NewMode == EBaseCampSubMode::FactoryStatus) RefreshFactoryStatus();
    else if (NewMode == EBaseCampSubMode::newMaterial) RefreshCampInventoryGrid();
}

void UUI_BaseCampInteract::RefreshFactoryStatus()
{
    // PB_PowerStatus가 유효한지 검사하는 방어선 구축
    if (!TXT_PowerStatus || !PB_PowerStatus || !SB_ResourceList) return;

    SB_ResourceList->ClearChildren();

    UGameInstance* GI = GetGameInstance();
    UFactoryManagerSubsystem* FactoryManager = GI ? GI->GetSubsystem<UFactoryManagerSubsystem>() : nullptr;
    if (!FactoryManager) return;

    // 전력 데이터 반영
    FFactoryPowerOverview PowerOverview = FactoryManager->GetFactoryPowerOverview();
    FString PowerString = FString::Printf(TEXT("%.1fW / %.1fW"), PowerOverview.CurrentDemandPower, PowerOverview.CurrentAvailablePower);
    TXT_PowerStatus->SetText(FText::FromString(PowerString));

    // 프로그레스 바 퍼센트 계산 (0.0f ~ 1.0f)
    float PowerPercent = 0.0f;
    
    // 최대 전력이 0일 때 나누기 오류(Division by Zero)가 나는 것을 방지합니다.
    if (PowerOverview.CurrentAvailablePower > 0.0f)
    {
        PowerPercent = PowerOverview.CurrentDemandPower / PowerOverview.CurrentAvailablePower;
    }

    // 전력 소모가 공급을 초과했을 때 게이지가 터져 나가지 않도록 0.0 ~ 1.0 사이로 안전하게 락을 겁니다.
    PowerPercent = FMath::Clamp(PowerPercent, 0.0f, 1.0f);

    // 계산된 비율을 프로그레스 바에 장착!
    PB_PowerStatus->SetPercent(PowerPercent);

    TArray<FFactoryItemProductionStat> ProductionStats = FactoryManager->GetItemProductionStats();
    for (const FFactoryItemProductionStat& Stat : ProductionStats)
    {
        if (Stat.ItemID.IsNone()) continue;
        FText SolvedItemName = FText::FromName(Stat.ItemID);
        UTexture2D* SolvedIcon = nullptr;

        if (ResourceDataTable)
        {
            FResourceData* RowData = ResourceDataTable->FindRow<FResourceData>(Stat.ItemID, TEXT("BaseCamp_Status_Lookup"));
            if (RowData)
            {
                if (!RowData->DisplayName.IsEmpty()) SolvedItemName = FText::FromString(RowData->DisplayName);
                SolvedIcon = RowData->ImgAsset.IsValid() ? RowData->ImgAsset.Get() : RowData->ImgAsset.LoadSynchronous();
            }
        }

        if (FactoryStatusRowClass)
        {
            UUI_FactoryStatusRow* NewRow = CreateWidget<UUI_FactoryStatusRow>(this, FactoryStatusRowClass);
            if (NewRow)
            {
                NewRow->SetRowData(SolvedIcon, SolvedItemName, Stat.ActualProductionPerSecond, Stat.TheoreticalProductionPerSecond);
                SB_ResourceList->AddChild(NewRow);
            }
        }
    }
}

void UUI_BaseCampInteract::RefreshAllUpgradeNodes()
{
    if (!WidgetTree) return;
    TArray<UWidget*> AllWidgets;
    WidgetTree->GetAllWidgets(AllWidgets);
    for (UWidget* Widget : AllWidgets)
    {
       if (UUI_UpgradeNode* UpgradeNode = Cast<UUI_UpgradeNode>(Widget)) UpgradeNode->RefreshNodeState();
    }
}

void UUI_BaseCampInteract::RefreshCampInventoryGrid()
{
    if (!GDP_CampInventoryGrid || !InventorySlotClass) return;
    GDP_CampInventoryGrid->ClearChildren();

    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance) return;

    UPlayerWarehouseSubsystem* WarehouseSubsystem = GameInstance->GetSubsystem<UPlayerWarehouseSubsystem>();
    if (!WarehouseSubsystem) return;

    const TMap<FName, int32>& CurrentItems = WarehouseSubsystem->GetStoredItems();
    TArray<FName> ItemIDs;
    for (const TPair<FName, int32>& Item : CurrentItems)
    {
        if (Item.Value > 0) ItemIDs.Add(Item.Key);
    }
    
    int32 MaxColumns = 5;  
    int32 TotalSlots = 30; 

    for (int32 i = 0; i < TotalSlots; ++i)
    {
        UUserWidget* CreatedWidget = CreateWidget<UUserWidget>(this, InventorySlotClass);
        if (!CreatedWidget) continue;

        UUI_InventorySlot* NewSlot = Cast<UUI_InventorySlot>(CreatedWidget);
        if (NewSlot)
        {
            if (i < ItemIDs.Num())
            {
                FName Key = ItemIDs[i];
                NewSlot->UpdateSlot(Key, CurrentItems[Key]);
            }
            else NewSlot->UpdateSlot(NAME_None, 0);
            
            int32 Row = i / MaxColumns;
            int32 Column = i % MaxColumns;

            UUniformGridSlot* GridSlot = GDP_CampInventoryGrid->AddChildToUniformGrid(NewSlot, Row, Column);
            if (GridSlot)
            {
                GridSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
                GridSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Fill);
            }
        }
    }
}