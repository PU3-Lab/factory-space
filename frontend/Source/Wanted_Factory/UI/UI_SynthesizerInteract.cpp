#include "UI/UI_SynthesizerInteract.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Engine/DataTable.h"
#include "Resource/ResourceData.h"
#include "Machines/MachineTable.h"
#include "Machines/MachineSubsystem.h"
#include "ItemDragDropOperation.h"
#include "OJJ_Player.h"
#include "UI/UI_Inventory.h"
#include "PlayerWarehouseSubsystem.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "UObject/ConstructorHelpers.h"
#include "UI/UIInteractDisplayHelpers.h"

using namespace UIInteractHelpers;

UUI_SynthesizerInteract::UUI_SynthesizerInteract(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    static ConstructorHelpers::FObjectFinder<UDataTable> ResourceTableFinder(
        TEXT("/Game/DataTable/DT_ResourceData.DT_ResourceData"));
    if (ResourceTableFinder.Succeeded()) ResourceDataTable = ResourceTableFinder.Object;

    static ConstructorHelpers::FObjectFinder<UDataTable> MachineTableFinder(
        TEXT("/Game/DataTable/DT_MachineData.DT_MachineData"));
    if (MachineTableFinder.Succeeded()) MachineDataTable = MachineTableFinder.Object;
}

void UUI_SynthesizerInteract::SetTargetMachine(AMachineBase* InMachine)
{
    TargetMachine = InMachine;
    ManualDroppedOutputItemID = NAME_None;
    LastInputVisualItemID_1 = NAME_None;
    LastInputVisualItemID_2 = NAME_None;
    LastInputVisualItemID_3 = NAME_None;
    LastOutputVisualItemID = NAME_None;
    
    if (TargetMachine)
    {
        // 내구도 무한 플래그 연동 (무적 기계 시 가시성 감춤 히든 처리)
        const ESlateVisibility FeatureVisibility = TargetMachine->IsInfiniteDurability() ? ESlateVisibility::Hidden : ESlateVisibility::Visible;
        if (PB_Durability)          PB_Durability->SetVisibility(FeatureVisibility);
        if (TXT_DurabilityPercent)  TXT_DurabilityPercent->SetVisibility(FeatureVisibility);
        if (BTN_Repair)             BTN_Repair->SetVisibility(FeatureVisibility);

        FName MachineTypeName = TargetMachine->GetMachineType();
        UMachineSubsystem* MachineSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMachineSubsystem>() : nullptr;
        UpdateMachineName(GetMachineDisplayText(MachineSubsystem, MachineTypeName));
        
        if (MachineDataTable && IMG_MachinePreview)
        {
            FMachineTableRow MachineData;
            if (MachineSubsystem && MachineSubsystem->FindMachineData(MachineTypeName, MachineData))
            {
                IMG_MachinePreview->SetVisibility(ESlateVisibility::Visible);
                UTexture2D* LoadedTexture = MachineData.ImgAsset.LoadSynchronous();
                if (LoadedTexture) IMG_MachinePreview->SetBrushFromTexture(LoadedTexture);
            }
        }
    }
}

void UUI_SynthesizerInteract::NativeConstruct()
{
    Super::NativeConstruct();
    if (BTN_Repair) BTN_Repair->OnClicked.AddDynamic(this, &UUI_SynthesizerInteract::OnRepairClicked);
}

void UUI_SynthesizerInteract::NativeDestruct()
{
    OnClosed.Broadcast();
    Super::NativeDestruct();
}

void UUI_SynthesizerInteract::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!TargetMachine) return;

    // 현재 기계의 합성 레시피에 적힌 3가지 필요 아이템 ID를 긁어옵니다.
    FRecipeTable Recipe = TargetMachine->GetCurrentRecipe();
    
    TArray<FName> ExpectedInputs = { Recipe.InputItem1, Recipe.InputItem2, Recipe.InputItem3 };
    const TMap<FName, int32>& InputInv = TargetMachine->GetInputInventory();

    // 만약 현재 공정이 Idle이라 레시피가 비어있다면, 인벤토리 주머니에 남아있는 실시간 자원 종류들을 채워넣습니다.
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

    // 3개의 슬롯 UI 실시간 밀어넣기 갱신
    int32 MaxInputAmount = TargetMachine->GetMaxInput();
    UpdateInputSlotUI(1, ExpectedInputs[0], InputInv.FindRef(ExpectedInputs[0]), MaxInputAmount);
    UpdateInputSlotUI(2, ExpectedInputs[1], InputInv.FindRef(ExpectedInputs[1]), MaxInputAmount);
    UpdateInputSlotUI(3, ExpectedInputs[2], InputInv.FindRef(ExpectedInputs[2]), MaxInputAmount);

    // 우측 출력(Output) 세팅
    FName OutputName = Recipe.OutputItem1;
    if (!ManualDroppedOutputItemID.IsNone()) OutputName = ManualDroppedOutputItemID;
    int32 OutputAmount = TargetMachine->GetOutputBuffer().FindRef(OutputName);
    UpdateOutputUI(OutputName, OutputAmount, TargetMachine->GetMaxOutput());

    // 상태 텍스트 분기
    EMachineState State = TargetMachine->GetMachineState();
    switch (State)
    {
    case EMachineState::Working: UpdateMachineState(TEXT("합성 중"), FLinearColor::Green); break;
    case EMachineState::Idle:    UpdateMachineState(TEXT("대기 중"), FLinearColor::Gray); break;
    case EMachineState::Blocked: UpdateMachineState(TEXT("출력 꽉 참"), FLinearColor::Red); break;
    case EMachineState::NoPower: UpdateMachineState(TEXT("전력 없음"), FLinearColor::Red); break;
    default:                     UpdateMachineState(TEXT("정지됨"), FLinearColor::White); break;
    }

    // 진행도 바 연산
    const float MaxTime = TargetMachine->GetEffectiveProcessTime(TargetMachine->GetProcessTime());
    if (State == EMachineState::Working && MaxTime > 0.0f)
    {
        const float RemainTime = GetWorld()->GetTimerManager().GetTimerRemaining(TargetMachine->GetProcessTimer());
        UpdateCraftingProgress(FMath::Clamp(1.0f - (RemainTime / MaxTime), 0.0f, 1.0f));
    }
    else
    {
        UpdateCraftingProgress(0.0f);
    }

    UpdateDurabilityUI(TargetMachine->GetCurrentDurability(), TargetMachine->GetMaxDurability());
}

void UUI_SynthesizerInteract::UpdateInputSlotUI(int32 SlotIndex, FName ItemName, int32 CurrentAmount, int32 MaxAmount)
{
    // 각 슬롯별 타겟 컴포넌트 포인터 매핑 배정
    UTextBlock* TargetTXT_Name = (SlotIndex == 1) ? TXT_InputName_1 : ((SlotIndex == 2) ? TXT_InputName_2 : TXT_InputName_3);
    UTextBlock* TargetTXT_Count = (SlotIndex == 1) ? TXT_InputCount_1 : ((SlotIndex == 2) ? TXT_InputCount_2 : TXT_InputCount_3);
    UProgressBar* TargetPB_Buffer = (SlotIndex == 1) ? PB_InputBuffer_1 : ((SlotIndex == 2) ? PB_InputBuffer_2 : PB_InputBuffer_3);
    UImage* TargetIMG_Icon = (SlotIndex == 1) ? IMG_InputIcon_1 : ((SlotIndex == 2) ? IMG_InputIcon_2 : IMG_InputIcon_3);
    FName& LastVisualID = (SlotIndex == 1) ? LastInputVisualItemID_1 : ((SlotIndex == 2) ? LastInputVisualItemID_2 : LastInputVisualItemID_3);

    const FName DisplayItemName = ItemName.IsNone() ? LastVisualID : ItemName;
    if (!ItemName.IsNone()) LastVisualID = ItemName;

    if (TargetTXT_Name && TargetTXT_Count && TargetPB_Buffer)
    {
        TargetTXT_Name->SetText(GetResourceDisplayText(this, ResourceDataTable, DisplayItemName));
        TargetTXT_Count->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), CurrentAmount, MaxAmount)));
        TargetPB_Buffer->SetPercent((MaxAmount > 0) ? (float)CurrentAmount / MaxAmount : 0.0f);
    }

    if (TargetIMG_Icon)
    {
        if (DisplayItemName.IsNone())
        {
            TargetIMG_Icon->SetVisibility(ESlateVisibility::Hidden);
            return;
        }

        TargetIMG_Icon->SetVisibility(ESlateVisibility::Visible);
        if (UTexture2D* Tex = GetResourceIconTexture(this, ResourceDataTable, DisplayItemName))
        {
            TargetIMG_Icon->SetBrushFromTexture(Tex);
            TargetIMG_Icon->SetColorAndOpacity(CurrentAmount <= 0 ? FLinearColor(1.f, 1.f, 1.f, 0.3f) : FLinearColor::White);
        }
    }
}

void UUI_SynthesizerInteract::UpdateOutputUI(FName ItemName, int32 CurrentAmount, int32 MaxAmount)
{
    const FName DisplayItemName = ItemName.IsNone() ? LastOutputVisualItemID : ItemName;
    if (!ItemName.IsNone()) LastOutputVisualItemID = ItemName;

    if (TXT_OutputName && TXT_OutputCount && PB_OutputBuffer)
    {
        TXT_OutputName->SetText(GetResourceDisplayText(this, ResourceDataTable, DisplayItemName));
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
        if (UTexture2D* Tex = GetResourceIconTexture(this, ResourceDataTable, DisplayItemName))
        {
            IMG_OutputIcon->SetBrushFromTexture(Tex);
            IMG_OutputIcon->SetColorAndOpacity((CurrentAmount <= 0 && DisplayItemName != ManualDroppedOutputItemID) ? FLinearColor(1.f, 1.f, 1.f, 0.15f) : FLinearColor::White);
        }
    }
}

bool UUI_SynthesizerInteract::NativeOnDrop(const FGeometry& MyGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    UItemDragDropOperation* ItemDragOp = Cast<UItemDragDropOperation>(InOperation);
    if (!ItemDragOp || !TargetMachine) return false;

    FName DroppedItemID = ItemDragOp->DraggedItemID;
    if (DroppedItemID.IsNone()) return false;

    // 가방 수량 풀 체크 가드
    int32 CurrentInputAmount = TargetMachine->GetInputInventory().FindRef(DroppedItemID);
    if (CurrentInputAmount >= TargetMachine->GetMaxInput()) return false;

    UGameInstance* GI = GetGameInstance();
    UPlayerWarehouseSubsystem* WarehouseSubsystem = GI ? GI->GetSubsystem<UPlayerWarehouseSubsystem>() : nullptr;
    
    if (WarehouseSubsystem && WarehouseSubsystem->TakeItem(DroppedItemID, 1)) 
    {
        TargetMachine->AddItem(DroppedItemID, 1);
        ManualDroppedOutputItemID = DroppedItemID;
        TargetMachine->TryStartProcess();

        // 가방 동시 동기화 새로고침
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
    return false;
}

void UUI_SynthesizerInteract::UpdateMachineState(FString StateText, FLinearColor StateColor) { if (TXT_MachineState) { TXT_MachineState->SetText(FText::FromString(StateText)); TXT_MachineState->SetColorAndOpacity(FSlateColor(StateColor)); } }
void UUI_SynthesizerInteract::UpdateCraftingProgress(float Percent) { if (PB_CraftingProgress && TXT_ProgressPercent) { PB_CraftingProgress->SetPercent(Percent); TXT_ProgressPercent->SetText(FText::FromString(FString::Printf(TEXT("진행도: %d%%"), FMath::RoundToInt(Percent * 100.0f)))); } }
void UUI_SynthesizerInteract::UpdateMachineName(const FText& MachineName) { if (TXT_MachineName) TXT_MachineName->SetText(MachineName); }
void UUI_SynthesizerInteract::OnRepairClicked() { if (TargetMachine) TargetMachine->RepairUsingWarehouse(); }
void UUI_SynthesizerInteract::UpdateDurabilityUI(float CurrentDurability, float MaxDurability) { if (TXT_DurabilityPercent && PB_Durability) { float SafeMax = (MaxDurability > 0.f) ? MaxDurability : 100.f; PB_Durability->SetPercent(FMath::Clamp(CurrentDurability / SafeMax, 0.0f, 1.0f)); TXT_DurabilityPercent->SetText(FText::FromString(FString::Printf(TEXT("내구도: %d / %d"), FMath::RoundToInt(CurrentDurability), FMath::RoundToInt(SafeMax)))); } }
