#include "UI/UI_WarehouseInteract.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Engine/DataTable.h"
#include "Resource/ResourceData.h"
#include "Machines/MachineTable.h"
#include "ItemDragDropOperation.h"
#include "OJJ_Player.h"
#include "UI/UI_Inventory.h"
#include "PlayerWarehouseSubsystem.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "MachineBase.h"
#include "Machines/MachineTable.h"
#include "UI_WarehouseInteract.generated.h"


void UUI_WarehouseInteract::SetTargetMachine(AMachineBase* InMachine)
{
    TargetMachine = InMachine;
    if (TargetMachine && MachineDataTable && IMG_MachinePreview)
    {
        FName MachineTypeName = TargetMachine->GetMachineType();
        UpdateMachineName(MachineTypeName.ToString());
        
        // 🌟 [보완한 레벨 우회 알고리즘 반영]
        FMachineTableRow* RowData = MachineDataTable->FindRow<FMachineTableRow>(MachineTypeName, TEXT("FindFallbackContext"));
        int32 MachineLevel = (RowData) ? RowData->Level : 1;

        FString LevelRowString = FString::Printf(TEXT("%s_LV%d"), *MachineTypeName.ToString(), MachineLevel);
        FMachineTableRow* FinalRowData = MachineDataTable->FindRow<FMachineTableRow>(FName(*LevelRowString), TEXT("FindWarehousePreviewContext"));
        if (!FinalRowData) FinalRowData = RowData;

        if (FinalRowData)
        {
            IMG_MachinePreview->SetVisibility(ESlateVisibility::Visible);
            UTexture2D* LoadedTexture = FinalRowData->ImgAsset.LoadSynchronous();
            if (LoadedTexture) IMG_MachinePreview->SetBrushFromTexture(LoadedTexture);
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
    
    // 🌟 [입력 UI 갱신 로직 통째로 삭제 완료!]

    // 우측 출력 버퍼 실시간 스캔 및 노출
    FRecipeTable Recipe = TargetMachine->GetCurrentRecipe();
    FName OutputName = Recipe.OutputItem1;
    if (!ManualDroppedOutputItemID.IsNone())
    {
        OutputName = ManualDroppedOutputItemID;
    }
    int32 OutputAmount = TargetMachine->GetOutputBuffer().FindRef(OutputName);
    
    UpdateOutputUI(OutputName, OutputAmount, TargetMachine->GetMaxOutput());

    // 상태 텍스트 및 내구도 동기화 (기존 유지)
    EMachineState State = TargetMachine->GetMachineState();
    switch (State)
    {
    case EMachineState::Working: UpdateMachineState(TEXT("출력 중"), FLinearColor::Green); break;
    case EMachineState::Idle:    UpdateMachineState(TEXT("보관 중"), FLinearColor::Gray); break;
    case EMachineState::Blocked: UpdateMachineState(TEXT("창고 가득 참"), FLinearColor::Red); break;
    default:                     UpdateMachineState(TEXT("대기 중"), FLinearColor::White); break;
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

            // 🌟 0개여도 수동 지정 품목이면 선명하게 박제
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

bool UUI_WarehouseInteract::NativeOnDrop(const FGeometry& MyGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    UItemDragDropOperation* ItemDragOp = Cast<UItemDragDropOperation>(InOperation);
    if (!ItemDragOp || !TargetMachine || !B_OutputDropZone) return false;
    
    // 센서 패드 범위 검사
    FVector2D DropScreenPos = InDragDropEvent.GetScreenSpacePosition();
    if (!USlateBlueprintLibrary::IsUnderLocation(B_OutputDropZone->GetCachedGeometry(), DropScreenPos)) return false;

    FName DroppedItemID = ItemDragOp->DraggedItemID;

    UGameInstance* GI = GetGameInstance();
    if (GI)
    {
        UPlayerWarehouseSubsystem* WarehouseSubsystem = GI->GetSubsystem<UPlayerWarehouseSubsystem>();
        if (WarehouseSubsystem && WarehouseSubsystem->TakeItem(DroppedItemID, 1)) 
        {
            // 🌟 [정정] 창고이므로 입력칸이 아닌 출력 버퍼(OutputBuffer)에 아이템을 누적 가산합니다!
            //TargetMachine->GetOutputBuffer().FindOrAdd(DroppedItemID) += 1;
            ManualDroppedOutputItemID = DroppedItemID;
            
            // 가방 UI 새로고침 촉발
            APlayerController* PC = GetOwningPlayer();
            if (PC)
            {
                AOJJ_Player* OJJPlayer = Cast<AOJJ_Player>(PC->GetPawn());
                if (OJJPlayer && OJJPlayer->GetInventoryWidgetInstance()) 
                {
                    OJJPlayer->GetInventoryWidgetInstance()->UpdateSlotQuantitiesOnly();
                }
            }
            return true; 
        }
    }
    return false;
}

void UUI_WarehouseInteract::UpdateMachineName(FString MachineName) { if (TXT_MachineName) TXT_MachineName->SetText(FText::FromString(MachineName)); }
void UUI_WarehouseInteract::UpdateMachineState(FString StateText, FLinearColor StateColor) { if (TXT_MachineState) { TXT_MachineState->SetText(FText::FromString(StateText)); TXT_MachineState->SetColorAndOpacity(FSlateColor(StateColor)); } }
void UUI_WarehouseInteract::OnCloseClicked() { RemoveFromParent(); }
void UUI_WarehouseInteract::OnRepairClicked() { if (TargetMachine) TargetMachine->RepairUsingWarehouse(); }
void UUI_WarehouseInteract::UpdateDurabilityUI(float CurrentDur, float MaxDur) { if (TXT_DurabilityPercent && PB_Durability) { float SafeMax = (MaxDur > 0.f) ? MaxDur : 100.f; float Percent = FMath::Clamp(CurrentDur / SafeMax, 0.0f, 1.0f); PB_Durability->SetPercent(Percent); FString DurabilityStr = FString::Printf(TEXT("내구도: %d / %d"), FMath::RoundToInt(CurrentDur), FMath::RoundToInt(SafeMax)); TXT_DurabilityPercent->SetText(FText::FromString(DurabilityStr)); } }