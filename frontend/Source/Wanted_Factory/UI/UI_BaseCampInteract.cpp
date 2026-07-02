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
#include "FactoryAgentClientSubsystem.h"
#include "FactorySaveSubsystem.h"
#include "MachineBase.h"
#include "FactoryManagerSubsystem.h"
#include "UI_FactoryStatusRow.h"
#include "Resource/ResourceData.h"
#include "ItemDragDropOperation.h"
#include "UI/UIInteractDisplayHelpers.h"
#include "Blueprint/DragDropOperation.h" 
#include "Blueprint/SlateBlueprintLibrary.h"
#include "InputCoreTypes.h"

using namespace UIInteractHelpers;

static bool IsInputIconDragCandidate(UImage* Icon, FName ItemID, const TMap<FName, int32>& InputInventory, FVector2D ScreenPosition)
{
    return Icon
        && !ItemID.IsNone()
        && InputInventory.FindRef(ItemID) > 0
        && USlateBlueprintLibrary::IsUnderLocation(Icon->GetCachedGeometry(), ScreenPosition);
}

void UUI_BaseCampInteract::SetTargetMachine(AMachineBase* InMachine)
{
    TargetBaseCamp = InMachine;
    ManualDroppedOutputItemID = NAME_None;
    LastInputVisualItemID_1 = NAME_None;
    LastInputVisualItemID_2 = NAME_None;
    LastInputVisualItemID_3 = NAME_None;
    LastOutputVisualItemID = NAME_None;
    LatestMaterialGenerationOutputItemID = NAME_None;
    DraggingInputItemID = NAME_None;
    DraggingInputIcon = nullptr;
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

    if (BTN_RequestMaterialGeneration)
    {
        BTN_RequestMaterialGeneration->OnClicked.RemoveDynamic(this, &UUI_BaseCampInteract::OnRequestMaterialGenerationClicked);
        BTN_RequestMaterialGeneration->OnClicked.AddDynamic(this, &UUI_BaseCampInteract::OnRequestMaterialGenerationClicked);
    }

    if (BTN_RequestOptimization)
    {
        BTN_RequestOptimization->OnClicked.RemoveDynamic(this, &UUI_BaseCampInteract::OnRequestOptimizationClicked);
        BTN_RequestOptimization->OnClicked.AddDynamic(this, &UUI_BaseCampInteract::OnRequestOptimizationClicked);
    }

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UFactoryAgentClientSubsystem* AgentClient = GameInstance->GetSubsystem<UFactoryAgentClientSubsystem>())
        {
            AgentClient->OnMaterialGenerationResponseReceived.RemoveDynamic(
                this,
                &UUI_BaseCampInteract::HandleMaterialGenerationResponse);
            AgentClient->OnMaterialGenerationResponseReceived.AddDynamic(
                this,
                &UUI_BaseCampInteract::HandleMaterialGenerationResponse);
        }
    }

    // 초기 화면 상태 지정
    SwitchSubPaneMode(EBaseCampSubMode::LevelUpgrade);
    RefreshAllUpgradeNodes();
}

void UUI_BaseCampInteract::NativeDestruct()
{
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UFactoryAgentClientSubsystem* AgentClient = GameInstance->GetSubsystem<UFactoryAgentClientSubsystem>())
        {
            AgentClient->OnMaterialGenerationResponseReceived.RemoveDynamic(
                this,
                &UUI_BaseCampInteract::HandleMaterialGenerationResponse);
        }
    }

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
    if (OutputName.IsNone())
    {
        TargetBaseCamp->PeekFirstOutputItem(OutputName);
    }
    if (OutputName.IsNone() && !LatestMaterialGenerationOutputItemID.IsNone())
    {
        OutputName = LatestMaterialGenerationOutputItemID;
    }
    if (!ManualDroppedOutputItemID.IsNone()) OutputName = ManualDroppedOutputItemID;
    int32 OutputAmount = TargetBaseCamp->GetOutputBuffer().FindRef(OutputName);
    UpdateOutputUI(OutputName, OutputAmount);
}

void UUI_BaseCampInteract::UpdateInputSlotUI(int32 SlotIndex, FName ItemName, int32 CurrentAmount, int32 MaxAmount)
{
    UTextBlock* TargetTXT_Name = (SlotIndex == 1) ? TXT_InputName_1 : ((SlotIndex == 2) ? TXT_InputName_2 : TXT_InputName_3);
    UTextBlock* TargetTXT_Count = (SlotIndex == 1) ? TXT_InputCount_1 : ((SlotIndex == 2) ? TXT_InputCount_2 : TXT_InputCount_3);
    UProgressBar* TargetPB_Buffer = (SlotIndex == 1) ? PB_InputBuffer_1 : ((SlotIndex == 2) ? PB_InputBuffer_2 : PB_InputBuffer_3);
    UImage* TargetIMG_Icon = (SlotIndex == 1) ? IMG_InputIcon_1 : ((SlotIndex == 2) ? IMG_InputIcon_2 : IMG_InputIcon_3);
    FName& LastVisualID = (SlotIndex == 1) ? LastInputVisualItemID_1 : ((SlotIndex == 2) ? LastInputVisualItemID_2 : LastInputVisualItemID_3);

    const FName DisplayItemName = (CurrentAmount > 0) ? ItemName : NAME_None;
    LastVisualID = DisplayItemName;

    if (TargetTXT_Name && TargetTXT_Count && TargetPB_Buffer)
    {
        TargetTXT_Name->SetText(DisplayItemName.IsNone() ? FText::GetEmpty() : GetResourceDisplayText(this, ResourceDataTable, DisplayItemName));
        TargetTXT_Count->SetText(CurrentAmount > 0 ? FText::FromString(FString::Printf(TEXT("%d / %d"), CurrentAmount, MaxAmount)) : FText::GetEmpty());
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
        if (UTexture2D* Tex = GetResourceIconTexture(this, ResourceDataTable, DisplayItemName))
        {
            TargetIMG_Icon->SetBrushFromTexture(Tex);
            TargetIMG_Icon->SetColorAndOpacity(FLinearColor::White);
        }
    }
}

void UUI_BaseCampInteract::UpdateOutputUI(FName ItemName, int32 CurrentAmount)
{
    const FName DisplayItemName = ItemName.IsNone() ? LastOutputVisualItemID : ItemName;
    if (!ItemName.IsNone()) LastOutputVisualItemID = ItemName;

    if (TXT_OutputName)
    {
        TXT_OutputName->SetText(GetResourceDisplayText(this, ResourceDataTable, DisplayItemName));
    }

    if (IMG_OutputIcon)
    {
        if (DisplayItemName.IsNone())
        {
            IMG_OutputIcon->SetVisibility(ESlateVisibility::Hidden);
            return;
        }

        IMG_OutputIcon->SetVisibility(ESlateVisibility::Visible);
        if (UTexture2D* Tex = GetResourceIconTexture(this, ResourceDataTable, DisplayItemName))
        {
            IMG_OutputIcon->SetBrushFromTexture(Tex);
            IMG_OutputIcon->SetColorAndOpacity((CurrentAmount <= 0 && DisplayItemName != ManualDroppedOutputItemID) ? FLinearColor(1.f, 1.f, 1.f, 0.15f) : FLinearColor::White);
        }
    }
}

// 인벤토리 슬롯에서 드래그해서 신물질 기계로 떨어뜨릴 때의 핵심 처리 핸들러
bool UUI_BaseCampInteract::NativeOnDrop(const FGeometry& MyGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    UItemDragDropOperation* ItemDragOp = Cast<UItemDragDropOperation>(InOperation);
    if (!ItemDragOp || !TargetBaseCamp) return false;
    if (ItemDragOp->Payload == this) return false;

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
FReply UUI_BaseCampInteract::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    FReply Reply = Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
    if (!TargetBaseCamp || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    {
        return Reply;
    }

    DraggingInputItemID = NAME_None;
    DraggingInputIcon = nullptr;

    const FVector2D ClickPosition = InMouseEvent.GetScreenSpacePosition();
    const TMap<FName, int32>& InputInventory = TargetBaseCamp->GetInputInventory();

    if (IsInputIconDragCandidate(IMG_InputIcon_1, LastInputVisualItemID_1, InputInventory, ClickPosition))
    {
        DraggingInputItemID = LastInputVisualItemID_1;
        DraggingInputIcon = IMG_InputIcon_1;
    }
    else if (IsInputIconDragCandidate(IMG_InputIcon_2, LastInputVisualItemID_2, InputInventory, ClickPosition))
    {
        DraggingInputItemID = LastInputVisualItemID_2;
        DraggingInputIcon = IMG_InputIcon_2;
    }
    else if (IsInputIconDragCandidate(IMG_InputIcon_3, LastInputVisualItemID_3, InputInventory, ClickPosition))
    {
        DraggingInputItemID = LastInputVisualItemID_3;
        DraggingInputIcon = IMG_InputIcon_3;
    }

    if (DraggingInputItemID.IsNone())
    {
        return Reply;
    }

    return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
}

void UUI_BaseCampInteract::NativeOnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& InPointerEvent, UDragDropOperation*& OutOperation)
{
    Super::NativeOnDragDetected(MyGeometry, InPointerEvent, OutOperation);

    if (DraggingInputItemID.IsNone())
    {
        return;
    }

    UItemDragDropOperation* DragOp = NewObject<UItemDragDropOperation>(this);
    if (!DragOp)
    {
        return;
    }

    DragOp->DraggedItemID = DraggingInputItemID;
    DragOp->Payload = this;

    OutOperation = DragOp;
}

bool UUI_BaseCampInteract::TakeInputItemForInventoryDrop(FName ItemID)
{
    if (!TargetBaseCamp)
    {
        return false;
    }

    const bool bTaken = TargetBaseCamp->TakeInputItem(ItemID, 1);
    if (bTaken)
    {
        DraggingInputItemID = NAME_None;
        DraggingInputIcon = nullptr;
    }

    return bTaken;
}

void UUI_BaseCampInteract::RefreshCampInventoryAfterInventoryDrop()
{
    RefreshCampInventoryGrid();
}

void UUI_BaseCampInteract::ReturnInputItemFromFailedDrop(FName ItemID)
{
    if (!TargetBaseCamp || ItemID.IsNone())
    {
        return;
    }

    TargetBaseCamp->AddItem(ItemID, 1);
    RefreshCampInventoryGrid();
}

void UUI_BaseCampInteract::OnStatusTabClicked() { SwitchSubPaneMode(EBaseCampSubMode::FactoryStatus); }
void UUI_BaseCampInteract::OnUpgradeTabClicked() { SwitchSubPaneMode(EBaseCampSubMode::LevelUpgrade); }
void UUI_BaseCampInteract::OnMaterialTabClicked() { SwitchSubPaneMode(EBaseCampSubMode::newMaterial); }
void UUI_BaseCampInteract::OnRequestMaterialGenerationClicked() { RequestMaterialGeneration(); }
void UUI_BaseCampInteract::OnRequestOptimizationClicked() { RequestProcessOptimization(); }

void UUI_BaseCampInteract::HandleMaterialGenerationResponse(const FFactoryMaterialGenerationResponse& Response)
{
    if (!Response.MaterialId.IsEmpty())
    {
        LatestMaterialGenerationOutputItemID = FName(Response.MaterialId);
        LastOutputVisualItemID = LatestMaterialGenerationOutputItemID;
        UpdateOutputUI(LatestMaterialGenerationOutputItemID, TargetBaseCamp
            ? TargetBaseCamp->GetOutputBuffer().FindRef(LatestMaterialGenerationOutputItemID)
            : 0);
        return;
    }

    if (Response.Outputs.Num() > 0 && !Response.Outputs[0].ItemId.IsNone())
    {
        LatestMaterialGenerationOutputItemID = Response.Outputs[0].ItemId;
        LastOutputVisualItemID = LatestMaterialGenerationOutputItemID;
        UpdateOutputUI(LatestMaterialGenerationOutputItemID, TargetBaseCamp
            ? TargetBaseCamp->GetOutputBuffer().FindRef(LatestMaterialGenerationOutputItemID)
            : 0);
    }
}

bool UUI_BaseCampInteract::RequestMaterialGeneration()
{
    if (!TargetBaseCamp)
    {
        return false;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UFactoryAgentClientSubsystem* AgentClient = GameInstance
        ? GameInstance->GetSubsystem<UFactoryAgentClientSubsystem>()
        : nullptr;
    if (!AgentClient)
    {
        return false;
    }

    TArray<FFactoryMaterialRequestInput> Inputs;
    Inputs.Reserve(3);

    TArray<TPair<FName, int32>> SortedInputs = TargetBaseCamp->GetInputInventory().Array();
    SortedInputs.Sort([](const TPair<FName, int32>& Left, const TPair<FName, int32>& Right)
    {
        if (Left.Key != Right.Key)
        {
            return Left.Key.LexicalLess(Right.Key);
        }

        return Left.Value < Right.Value;
    });

    for (const TPair<FName, int32>& InputPair : SortedInputs)
    {
        if (InputPair.Key.IsNone() || InputPair.Value <= 0)
        {
            continue;
        }

        FFactoryMaterialRequestInput& Input = Inputs.AddDefaulted_GetRef();
        Input.ItemId = InputPair.Key;
        Input.Quantity = InputPair.Value;

        if (Inputs.Num() >= 3)
        {
            break;
        }
    }

    if (Inputs.Num() == 0)
    {
        return false;
    }

    if (AgentClient->GetConnectionState() == EFactoryAgentConnectionState::Disconnected)
    {
        AgentClient->ConnectToDefaultServer();
    }

    return AgentClient->SendMaterialGenerationRequest(Inputs, TEXT(""), true, TEXT("basecamp-ui-001"));
}

bool UUI_BaseCampInteract::RequestProcessOptimization()
{
    UGameInstance* GameInstance = GetGameInstance();
    UFactorySaveSubsystem* SaveSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UFactorySaveSubsystem>()
        : nullptr;
    if (!SaveSubsystem)
    {
        return false;
    }

    return SaveSubsystem->SendManualProcessOptimizerAnalyzeRequest();
}

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
    FString PowerString = FString::Printf(
        TEXT("소모량 %.1fW / 총 전력량 %.1fW"),
        PowerOverview.CurrentDemandPower,
        PowerOverview.CurrentAvailablePower);
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
        const FText SolvedItemName = GetResourceDisplayText(this, ResourceDataTable, Stat.ItemID);
        UTexture2D* SolvedIcon = GetResourceIconTexture(this, ResourceDataTable, Stat.ItemID);

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
    
    int32 MaxColumns = 4;
    int32 TotalSlots = 24;

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
