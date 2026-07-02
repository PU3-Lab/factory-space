#include "UI/UI_WarehouseInteract.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Engine/DataTable.h"
#include "Resource/ResourceData.h"
#include "Machines/MachineTable.h"
#include "Components/ProgressBar.h"
#include "Machines/MachineSubsystem.h"
#include "Machines/WarehousePort.h"
#include "MachineBase.h"
#include "ItemDragDropOperation.h"
#include "OJJ_Player.h"
#include "UI/UI_Inventory.h"
#include "PlayerWarehouseSubsystem.h"
#include "QuestManagerSubsystem.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "UObject/ConstructorHelpers.h"
#include "UI/UIInteractDisplayHelpers.h"

using namespace UIInteractHelpers;

UUI_WarehouseInteract::UUI_WarehouseInteract(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    static ConstructorHelpers::FObjectFinder<UDataTable> ResourceTableFinder(
        TEXT("/Game/DataTable/DT_ResourceData.DT_ResourceData"));
    if (ResourceTableFinder.Succeeded())
    {
        ResourceDataTable = ResourceTableFinder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> MachineTableFinder(
        TEXT("/Game/DataTable/DT_MachineData.DT_MachineData"));
    if (MachineTableFinder.Succeeded())
    {
        MachineDataTable = MachineTableFinder.Object;
    }
}

void UUI_WarehouseInteract::SetTargetMachine(AMachineBase* InMachine)
{
    TargetMachine = InMachine;
    ManualDroppedOutputItemID = NAME_None;
    LastOutputVisualItemID = NAME_None;
    
    if (TargetMachine)
    {
        if (AWarehousePort* WarehousePort = Cast<AWarehousePort>(TargetMachine))
        {
            ManualDroppedOutputItemID = WarehousePort->GetSelectedOutputItem();
        }
        else if (ALiquidTank* LiquidTank = Cast<ALiquidTank>(TargetMachine))
        {
            ManualDroppedOutputItemID = LiquidTank->GetSelectedOutputLiquid();
        }

        FName MachineTypeName = TargetMachine->GetMachineType();
        UMachineSubsystem* MachineSubsystem = GetGameInstance()
            ? GetGameInstance()->GetSubsystem<UMachineSubsystem>()
            : nullptr;
        UpdateMachineName(GetMachineDisplayText(MachineSubsystem, MachineTypeName));
        
        if (MachineDataTable && IMG_MachinePreview)
        {
            FMachineTableRow MachineData;
            const bool bFoundMachineData = MachineSubsystem &&
                MachineSubsystem->FindMachineData(MachineTypeName, MachineData);

            if (bFoundMachineData)
            {
                IMG_MachinePreview->SetVisibility(ESlateVisibility::Visible);
                UTexture2D* LoadedTexture = MachineData.ImgAsset.LoadSynchronous();
                if (LoadedTexture) IMG_MachinePreview->SetBrushFromTexture(LoadedTexture);
            }
        }
    }
}

void UUI_WarehouseInteract::NativeConstruct()
{
    Super::NativeConstruct();
}

void UUI_WarehouseInteract::NativeDestruct()
{
    OnClosed.Broadcast();
    Super::NativeDestruct();
}

void UUI_WarehouseInteract::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!TargetMachine) return;
    
    // 기계 타입별 정밀 실시간 수량 데이터 가져오기
    FName OutputName = NAME_None;
    int32 OutputAmount = 0;
    int32 MaxOutputAmount = TargetMachine->GetMaxOutput();

    if (AWarehousePort* WarehousePort = Cast<AWarehousePort>(TargetMachine))
    {
        OutputName = WarehousePort->GetSelectedOutputItem();
        OutputAmount = WarehousePort->GetSelectedOutputItemCount();
        MaxOutputAmount = FMath::Max(OutputAmount, 1);
    }
    else if (ALiquidTank* LiquidTank = Cast<ALiquidTank>(TargetMachine))
    {
        OutputName = LiquidTank->GetSelectedOutputLiquid();
        OutputAmount = LiquidTank->GetStoredLiquidAmount();
        MaxOutputAmount = LiquidTank->GetCapacity();
    }
    else
    {
        FRecipeTable Recipe = TargetMachine->GetCurrentRecipe();
        OutputName = Recipe.OutputItem1;
        if (!ManualDroppedOutputItemID.IsNone())
        {
            OutputName = ManualDroppedOutputItemID;
        }
        OutputAmount = TargetMachine->GetOutputBuffer().FindRef(OutputName);
    }
    
    // 우측 보관함 슬롯 노출 갱신
    UpdateOutputUI(OutputName, OutputAmount, MaxOutputAmount);
}

void UUI_WarehouseInteract::UpdateOutputUI(FName ItemName, int32 CurrentAmount, int32 MaxAmount)
{
    const FName DisplayItemName = ItemName.IsNone() ? LastOutputVisualItemID : ItemName;
    if (!ItemName.IsNone())
    {
        LastOutputVisualItemID = ItemName;
    }

    if (TXT_OutputName && TXT_OutputCount && PB_OutputBuffer)
    {
        TXT_OutputName->SetText(GetResourceDisplayText(this, ResourceDataTable, DisplayItemName));
        FString CountStr = FString::Printf(TEXT("%d / %d"), CurrentAmount, MaxAmount);
        TXT_OutputCount->SetText(FText::FromString(CountStr));
        
        float FillPercent = (MaxAmount > 0) ? (float)CurrentAmount / MaxAmount : 0.0f;
        PB_OutputBuffer->SetPercent(FillPercent);
    }

    if (DisplayItemName.IsNone())
    {
        if (IMG_OutputIcon) IMG_OutputIcon->SetVisibility(ESlateVisibility::Hidden);
        return;
    }

    if (IMG_OutputIcon)
    {
        if (UTexture2D* IconTexture = GetResourceIconTexture(this, ResourceDataTable, DisplayItemName))
        {
            IMG_OutputIcon->SetVisibility(ESlateVisibility::Visible);
            IMG_OutputIcon->SetBrushFromTexture(IconTexture);
            if (CurrentAmount <= 0 && DisplayItemName != ManualDroppedOutputItemID)
            {
                IMG_OutputIcon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.15f));
            }
            else
            {
                IMG_OutputIcon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
            }
        }
        else
        {
            IMG_OutputIcon->SetBrush(FSlateBrush());
            IMG_OutputIcon->SetVisibility(ESlateVisibility::Hidden);
        }
    }
}

bool UUI_WarehouseInteract::NativeOnDrop(const FGeometry& MyGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    UItemDragDropOperation* ItemDragOp = Cast<UItemDragDropOperation>(InOperation);
    if (!ItemDragOp || !TargetMachine || !B_OutputDropZone) return false;
    
    FName TargetItemID = ItemDragOp->DraggedItemID; 
    if (TargetItemID.IsNone()) return false;

    FVector2D DropScreenPos = InDragDropEvent.GetScreenSpacePosition();
    if (!USlateBlueprintLibrary::IsUnderLocation(B_OutputDropZone->GetCachedGeometry(), DropScreenPos)) return false;

    UGameInstance* GI = GetGameInstance();
    UPlayerWarehouseSubsystem* WarehouseSubsystem = GI ? GI->GetSubsystem<UPlayerWarehouseSubsystem>() : nullptr;

    if (AWarehousePort* WarehousePort = Cast<AWarehousePort>(TargetMachine))
    {
        if (WarehouseSubsystem && WarehouseSubsystem->TakeItem(TargetItemID, 1))
        {
            const FName PreviousOutputItem = WarehousePort->GetSelectedOutputItem();
            WarehousePort->SetSelectedOutputItem(TargetItemID);
            if (WarehousePort->GetSelectedOutputItem() != TargetItemID && PreviousOutputItem != TargetItemID) return false;

            ManualDroppedOutputItemID = TargetItemID;

            if (UQuestManagerSubsystem* QuestManager = GI->GetSubsystem<UQuestManagerSubsystem>())
            {
                QuestManager->NotifyTutorialEvent(TEXT("WarehouseOutputItemSet"), TargetItemID);
            }
            TargetMachine->TryStartProcess(); 
            return true;
        }
        return false;
    }

    if (ALiquidTank* LiquidTank = Cast<ALiquidTank>(TargetMachine))
    {
        if (WarehouseSubsystem && WarehouseSubsystem->TakeItem(TargetItemID, 1))
        {
            const FName PreviousOutputLiquid = LiquidTank->GetSelectedOutputLiquid();
            LiquidTank->SetSelectedOutputLiquid(TargetItemID);
            if (LiquidTank->GetSelectedOutputLiquid() != TargetItemID && PreviousOutputLiquid != TargetItemID) return false;

            ManualDroppedOutputItemID = TargetItemID;
            TargetMachine->TryStartProcess();
            return true;
        }
        return false;
    }

    int32 CurrentInputAmount = TargetMachine->GetInputInventory().FindRef(TargetItemID);
    if (CurrentInputAmount >= TargetMachine->GetMaxInput()) return false;

    if (WarehouseSubsystem && WarehouseSubsystem->TakeItem(TargetItemID, 1)) 
    {
        TargetMachine->AddItem(TargetItemID, 1);
        TargetMachine->TryStartProcess(); 
        ManualDroppedOutputItemID = TargetItemID;
        return true; 
    }
    
    return false;
}

FReply UUI_WarehouseInteract::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    FReply Reply = Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);

    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && !ManualDroppedOutputItemID.IsNone())
    {
        FVector2D ClickPos = InMouseEvent.GetScreenSpacePosition();
        if (B_OutputDropZone && USlateBlueprintLibrary::IsUnderLocation(B_OutputDropZone->GetCachedGeometry(), ClickPos))
        {
            return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
        }
    }
    return Reply;
}

void UUI_WarehouseInteract::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

    if (ManualDroppedOutputItemID.IsNone()) return;

    UItemDragDropOperation* DragOp = NewObject<UItemDragDropOperation>(this);
    if (!DragOp) return;

    DragOp->DraggedItemID = ManualDroppedOutputItemID;
    
    CancelMachineProcess();

    OutOperation = DragOp;
}

void UUI_WarehouseInteract::CancelMachineProcess()
{
    if (!TargetMachine || ManualDroppedOutputItemID.IsNone()) return;

    TargetMachine->StopProcess();

    int32 CurrentStoredAmount = TargetMachine->GetOutputBuffer().FindRef(ManualDroppedOutputItemID);

    if (AWarehousePort* WarehousePort = Cast<AWarehousePort>(TargetMachine))
    {
        WarehousePort->SetSelectedOutputItem(NAME_None);
        if (CurrentStoredAmount > 0)
        {
            TargetMachine->TakeOutputItem(ManualDroppedOutputItemID, CurrentStoredAmount);
        }
    }
    else if (ALiquidTank* LiquidTank = Cast<ALiquidTank>(TargetMachine))
    {
        LiquidTank->SetSelectedOutputLiquid(NAME_None);
        if (CurrentStoredAmount > 0)
        {
            TargetMachine->TakeOutputItem(ManualDroppedOutputItemID, CurrentStoredAmount);
        }
    }
    else
    {
        if (CurrentStoredAmount > 0)
        {
            TargetMachine->TakeOutputItem(ManualDroppedOutputItemID, CurrentStoredAmount);
        }
    }

    ManualDroppedOutputItemID = NAME_None;
    TargetMachine->RefreshMachineState();

    APlayerController* PC = GetOwningPlayer();
    if (PC)
    {
        AOJJ_Player* OJJPlayer = Cast<AOJJ_Player>(PC->GetPawn());
        if (OJJPlayer && OJJPlayer->GetInventoryWidgetInstance())
        {
            OJJPlayer->GetInventoryWidgetInstance()->UpdateSlotQuantitiesOnly();
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[창고 홀드] 아이템이 배달부 등짐으로 옮겨져 임시 차감 연산을 완료했습니다."));
}

void UUI_WarehouseInteract::UpdateMachineName(const FText& MachineName) { if (TXT_MachineName) TXT_MachineName->SetText(MachineName); }
