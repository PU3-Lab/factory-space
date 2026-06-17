#include "UI/UI_WarehouseInteract.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Engine/DataTable.h"
#include "Resource/ResourceData.h"
#include "Machines/MachineTable.h"
#include "Machines/MachineSubsystem.h"
#include "Machines/WarehousePort.h"
#include "MachineBase.h"
#include "ItemDragDropOperation.h"
#include "OJJ_Player.h"
#include "UI/UI_Inventory.h"
#include "PlayerWarehouseSubsystem.h"
#include "QuestManagerSubsystem.h"
#include "Blueprint/SlateBlueprintLibrary.h"



void UUI_WarehouseInteract::SetTargetMachine(AMachineBase* InMachine)
{
    TargetMachine = InMachine;
    ManualDroppedOutputItemID = NAME_None;
    
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
        UpdateMachineName(MachineTypeName.ToString());
        
        if (MachineDataTable && IMG_MachinePreview)
        {
            FMachineTableRow MachineData;
            UMachineSubsystem* MachineSubsystem = GetGameInstance()
                ? GetGameInstance()->GetSubsystem<UMachineSubsystem>()
                : nullptr;
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
    if (BTN_Close) BTN_Close->OnClicked.AddDynamic(this, &UUI_WarehouseInteract::OnCloseClicked);
    if (BTN_Repair) BTN_Repair->OnClicked.AddDynamic(this, &UUI_WarehouseInteract::OnRepairClicked);
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

    // 상태 텍스트 분기 처리
    EMachineState State = TargetMachine->GetMachineState();
    switch (State)
    {
    case EMachineState::Working: UpdateMachineState(TEXT("출력 중"), FLinearColor::Green); break;
    case EMachineState::Idle:    UpdateMachineState(TEXT("보관 중"), FLinearColor::Gray); break;
    case EMachineState::Blocked: UpdateMachineState(TEXT("창고 가득 참"), FLinearColor::Red); break;
    case EMachineState::NoPower: UpdateMachineState(TEXT("전력 없음"), FLinearColor::Red); break;
    default:                     UpdateMachineState(TEXT("대기 중"), FLinearColor::White); break;
    }

    // 중앙 프로그래스바
    float MaxTime = TargetMachine->GetProcessTime();
    if (State == EMachineState::Working && MaxTime > 0.0f)
    {
        float RemainTime = GetWorld()->GetTimerManager().GetTimerRemaining(TargetMachine->GetProcessTimer());
        float Progress = 1.0f - (RemainTime / MaxTime);
        UpdateCraftingProgress(Progress);
    }
    else
    {
        UpdateCraftingProgress(0.0f);
    }

    UpdateDurabilityUI(TargetMachine->GetCurrentDurability(), TargetMachine->GetMaxDurability());
}

void UUI_WarehouseInteract::UpdateOutputUI(FName ItemName, int32 CurrentAmount, int32 MaxAmount)
{
    if (TXT_OutputName && TXT_OutputCount && PB_OutputBuffer)
    {
        TXT_OutputName->SetText(FText::FromName(ItemName));
        FString CountStr = FString::Printf(TEXT("%d / %d"), CurrentAmount, MaxAmount);
        TXT_OutputCount->SetText(FText::FromString(CountStr));
        
        float FillPercent = (MaxAmount > 0) ? (float)CurrentAmount / MaxAmount : 0.0f;
        PB_OutputBuffer->SetPercent(FillPercent);
    }

    if (ItemName.IsNone())
    {
        if (IMG_OutputIcon) IMG_OutputIcon->SetVisibility(ESlateVisibility::Hidden);
        return;
    }

    if (ResourceDataTable && IMG_OutputIcon)
    {
        IMG_OutputIcon->SetVisibility(ESlateVisibility::Visible);
        if (FResourceData* RowData = ResourceDataTable->FindRow<FResourceData>(ItemName, TEXT("FindOutputContext")))
        {
            if (RowData->ImgAsset.IsValid()) IMG_OutputIcon->SetBrushFromTexture(RowData->ImgAsset.Get());
            else
            {
                UTexture2D* LoadedTexture = RowData->ImgAsset.LoadSynchronous();
                if (LoadedTexture) IMG_OutputIcon->SetBrushFromTexture(LoadedTexture);
            }

            if (CurrentAmount <= 0 && ItemName != ManualDroppedOutputItemID)
            {
                IMG_OutputIcon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.15f));
            }
            else
            {
                IMG_OutputIcon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
            }
        }
    }
}

void UUI_WarehouseInteract::UpdateCraftingProgress(float Percent)
{
    if (PB_CraftingProgress && TXT_ProgressPercent)
    {
        PB_CraftingProgress->SetPercent(Percent);
        int32 PercentInt = FMath::RoundToInt(Percent * 100.0f);
        FString ProgressStr = FString::Printf(TEXT("진행도: %d%%"), PercentInt);
        TXT_ProgressPercent->SetText(FText::FromString(ProgressStr));
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

    // 1. 대상 기계가 '창고포트' 일 때
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

    // 2. 대상 기계가 '유체탱크' 일 때
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

    // 3. 일반 생산 기계 계열일 때
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

    // 마우스 좌클릭이고, 현재 기계에 지정되거나 쌓인 아이템이 존재할 때만 드래그를 발동합니다.
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
    
    if (IMG_OutputIcon)
    {
        UImage* DragVisualImage = NewObject<UImage>(GetOwningPlayer(), UImage::StaticClass());
        if (DragVisualImage)
        {
            DragVisualImage->SetBrush(IMG_OutputIcon->GetBrush());
            DragVisualImage->SetDesiredSizeOverride(FVector2D(64.f, 64.f));
            DragOp->DefaultDragVisual = DragVisualImage;
        }
    }
    
    CancelMachineProcess();

    OutOperation = DragOp;
}

void UUI_WarehouseInteract::CancelMachineProcess()
{
    if (!TargetMachine || ManualDroppedOutputItemID.IsNone()) return;

    // 1. 돌아가던 기계의 공정 타이머를 중지시킵니다.
    TargetMachine->StopProcess();

    // 현재 기계 버퍼에 들어있는 정확한 수량을 확인합니다.
    int32 CurrentStoredAmount = TargetMachine->GetOutputBuffer().FindRef(ManualDroppedOutputItemID);

    // 2. 창고 포트일 때 내부 지정 품목 해제 및 버퍼 완벽 차감
    if (AWarehousePort* WarehousePort = Cast<AWarehousePort>(TargetMachine))
    {
        WarehousePort->SetSelectedOutputItem(NAME_None);
        if (CurrentStoredAmount > 0)
        {
            TargetMachine->TakeOutputItem(ManualDroppedOutputItemID, CurrentStoredAmount);
        }
    }
    // 3. 유체 탱크일 때 저장 유체 종류 해제 및 차감
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
        // 일반 생산 기계일 경우
        if (CurrentStoredAmount > 0)
        {
            TargetMachine->TakeOutputItem(ManualDroppedOutputItemID, CurrentStoredAmount);
        }
    }

    // 4. UI 변수 초기화 및 기계 상태 새로고침
    ManualDroppedOutputItemID = NAME_None;
    TargetMachine->RefreshMachineState();

    // 5. 슬롯 아이템 데이터 상태 새로고침 촉발
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

void UUI_WarehouseInteract::UpdateMachineName(FString MachineName) { if (TXT_MachineName) TXT_MachineName->SetText(FText::FromString(MachineName)); }
void UUI_WarehouseInteract::UpdateMachineState(FString StateText, FLinearColor StateColor) { if (TXT_MachineState) { TXT_MachineState->SetText(FText::FromString(StateText)); TXT_MachineState->SetColorAndOpacity(FSlateColor(StateColor)); } }
void UUI_WarehouseInteract::OnCloseClicked() { RemoveFromParent(); }
void UUI_WarehouseInteract::OnRepairClicked() { if (TargetMachine) TargetMachine->RepairUsingWarehouse(); }
void UUI_WarehouseInteract::UpdateDurabilityUI(float CurrentDur, float MaxDur) { if (TXT_DurabilityPercent && PB_Durability) { float SafeMax = (MaxDur > 0.f) ? MaxDur : 100.f; float Percent = FMath::Clamp(CurrentDur / SafeMax, 0.0f, 1.0f); PB_Durability->SetPercent(Percent); FString DurabilityStr = FString::Printf(TEXT("내구도: %d / %d"), FMath::RoundToInt(CurrentDur), FMath::RoundToInt(SafeMax)); TXT_DurabilityPercent->SetText(FText::FromString(DurabilityStr)); } }
