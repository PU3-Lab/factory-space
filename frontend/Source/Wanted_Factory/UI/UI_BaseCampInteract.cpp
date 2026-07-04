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
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "UI_UpgradeNode.h"
#include "UI/UI_InventorySlot.h"
#include "PlayerWarehouseSubsystem.h"
#include "FactoryAgentClientSubsystem.h"
#include "FactorySaveSubsystem.h"
#include "MachineBase.h"
#include "FactoryManagerSubsystem.h"
#include "UI/UI_DialogueBalloon.h"
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

    // 珥덇린 ?붾㈃ ?곹깭 吏??
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

// ?뙚 [?좎꽕] ?ㅼ떆媛??⑹꽦湲??곗씠??誘몃윭留????곕룞
void UUI_BaseCampInteract::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!TargetBaseCamp || !WS_SubPaneSwitcher) return;

    // 理쒖쟻?? ?좎?媛 3踰덉㎏ '?좊Ъ吏??⑹꽦(Index 2)' ??쓣 蹂닿퀬 ?덉쓣 ?뚮쭔 ?ㅼ떆媛?UI瑜??곗궛?⑸땲??
    if (WS_SubPaneSwitcher->GetActiveWidgetIndex() != 2) return;

    // ?ㅻ━吏???⑹꽦湲???濡쒖쭅??洹몃?濡?嫄곗젏 湲곌퀎 而댄룷?뚰듃??留ㅽ븨?⑸땲??
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
        // ?뙚 ??議곌굔臾??뺣텇???꾩씠?쒖씠 ?놁쑝硫??섏? ?ㅻえ媛 ?⑥? ?딄퀬 ?꾨꼍?섍쾶 ?щ챸(Hidden)?댁쭛?덈떎!
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

// ?몃깽?좊━ ?щ’?먯꽌 ?쒕옒洹명빐???좊Ъ吏?湲곌퀎濡??⑥뼱?⑤┫ ?뚯쓽 ?듭떖 泥섎━ ?몃뱾??
bool UUI_BaseCampInteract::NativeOnDrop(const FGeometry& MyGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    UItemDragDropOperation* ItemDragOp = Cast<UItemDragDropOperation>(InOperation);
    if (!ItemDragOp || !TargetBaseCamp) return false;
    if (ItemDragOp->Payload == this) return false;

    FName DroppedItemID = ItemDragOp->DraggedItemID;
    if (DroppedItemID.IsNone()) return false;

    // 1. 湲곌퀎???대떦 ?꾩씠???낅젰 ?щ’ ?섎웾???대? 苑?李쇰뒗吏 寃??
    int32 CurrentInputAmount = TargetBaseCamp->GetInputInventory().FindRef(DroppedItemID);
    if (CurrentInputAmount >= TargetBaseCamp->GetMaxInput()) return false;
    
    UGameInstance* GI = GetGameInstance();
    UPlayerWarehouseSubsystem* WarehouseSubsystem = GI ? GI->GetSubsystem<UPlayerWarehouseSubsystem>() : nullptr;
    
    // 2. 李쎄퀬?먯꽌 1媛?爰쇰궡湲??쒕룄
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

// (?섎㉧吏 OnStatusTabClicked, OnUpgradeTabClicked, OnMaterialTabClicked, SwitchSubPaneMode, RefreshFactoryStatus, RefreshAllUpgradeNodes, RefreshCampInventoryGrid 肄붾뱶??湲곗〈 援ы쁽怨??꾩쟾???숈씪?섎?濡??좎?)
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

    const bool bRequestSent = SaveSubsystem->SendManualProcessOptimizerAnalyzeRequest();
    if (!bRequestSent)
    {
        return false;
    }

    TArray<UUserWidget*> FoundWidgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
        this,
        FoundWidgets,
        UUI_DialogueBalloon::StaticClass(),
        false);

    if (FoundWidgets.Num() > 0)
    {
        for (UUserWidget* FoundWidget : FoundWidgets)
        {
            if (UUI_DialogueBalloon* DialogueBalloon = Cast<UUI_DialogueBalloon>(FoundWidget))
            {
                DialogueBalloon->BeginProcessOptimizerRequest();
            }
        }
    }

    return true;
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
    // PB_PowerStatus媛 ?좏슚?쒖? 寃?ы븯??諛⑹뼱??援ъ텞
    if (!TXT_PowerStatus || !PB_PowerStatus || !SB_ResourceList) return;

    SB_ResourceList->ClearChildren();

    UGameInstance* GI = GetGameInstance();
    UFactoryManagerSubsystem* FactoryManager = GI ? GI->GetSubsystem<UFactoryManagerSubsystem>() : nullptr;
    if (!FactoryManager) return;

    // ?꾨젰 ?곗씠??諛섏쁺
    FFactoryPowerOverview PowerOverview = FactoryManager->GetFactoryPowerOverview();
    FString PowerString = FString::Printf(
        TEXT("소모 전력 %.1fW / 총 전력 %.1fW"),
        PowerOverview.CurrentDemandPower,
        PowerOverview.CurrentAvailablePower);
    TXT_PowerStatus->SetText(FText::FromString(PowerString));

    // ?꾨줈洹몃젅??諛??쇱꽱??怨꾩궛 (0.0f ~ 1.0f)
    float PowerPercent = 0.0f;
    
    // 理쒕? ?꾨젰??0?????섎늻湲??ㅻ쪟(Division by Zero)媛 ?섎뒗 寃껋쓣 諛⑹??⑸땲??
    if (PowerOverview.CurrentAvailablePower > 0.0f)
    {
        PowerPercent = PowerOverview.CurrentDemandPower / PowerOverview.CurrentAvailablePower;
    }

    // ?꾨젰 ?뚮え媛 怨듦툒??珥덇낵?덉쓣 ??寃뚯씠吏媛 ?곗졇 ?섍?吏 ?딅룄濡?0.0 ~ 1.0 ?ъ씠濡??덉쟾?섍쾶 ?쎌쓣 寃곷땲??
    PowerPercent = FMath::Clamp(PowerPercent, 0.0f, 1.0f);

    // 怨꾩궛??鍮꾩쑉???꾨줈洹몃젅??諛붿뿉 ?μ갑!
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
    int32 TotalSlots = 20;

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

