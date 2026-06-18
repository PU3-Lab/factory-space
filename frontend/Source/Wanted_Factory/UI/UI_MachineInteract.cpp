#include "UI_MachineInteract.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "Engine/DataTable.h"
#include "Resource/ResourceData.h"
#include "Machines/MachineTable.h"
#include "Machines/MachineSubsystem.h"
#include "Machines/LiquidTank.h"
#include "Machines/WarehousePort.h"
#include "ItemDragDropOperation.h"
#include "OJJ_Player.h"
#include "QuestManagerSubsystem.h"
#include "UI/UI_Inventory.h"
#include "PlayerWarehouseSubsystem.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/Border.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
FText GetResourceDisplayText(const UDataTable* ResourceDataTable, FName ItemName)
{
    if (ItemName.IsNone())
    {
        return FText::GetEmpty();
    }

    if (ResourceDataTable)
    {
        if (const FResourceData* RowData = ResourceDataTable->FindRow<FResourceData>(ItemName, TEXT("GetResourceDisplayText")))
        {
            if (!RowData->DisplayName.IsEmpty())
            {
                return FText::FromString(RowData->DisplayName);
            }
        }
    }

    return FText::FromName(ItemName);
}

FText GetMachineDisplayText(UMachineSubsystem* MachineSubsystem, FName MachineTypeName)
{
    if (MachineSubsystem)
    {
        return MachineSubsystem->GetMachineDisplayName(MachineTypeName);
    }

    return MachineTypeName.IsNone() ? FText::GetEmpty() : FText::FromName(MachineTypeName);
}
}

UUI_MachineInteract::UUI_MachineInteract(const FObjectInitializer& ObjectInitializer)
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

void UUI_MachineInteract::SetTargetMachine(AMachineBase* InMachine)
{
    TargetMachine = InMachine;
    ManualDroppedOutputItemID = NAME_None;
    LastInputVisualItemID = NAME_None;
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
                
                if (MachineData.ImgAsset.IsValid())
                {
                    IMG_MachinePreview->SetBrushFromTexture(MachineData.ImgAsset.Get());
                }
                else
                {
                    UTexture2D* LoadedTexture = MachineData.ImgAsset.LoadSynchronous();
                    if (LoadedTexture)
                    {
                        IMG_MachinePreview->SetBrushFromTexture(LoadedTexture);
                    }
                    else
                    {
                        IMG_MachinePreview->SetBrush(FSlateBrush());
                        IMG_MachinePreview->SetVisibility(ESlateVisibility::Hidden);
                    }
                }
            }
            else
            {
                IMG_MachinePreview->SetBrush(FSlateBrush());
                IMG_MachinePreview->SetVisibility(ESlateVisibility::Hidden);
            }
        }
    }
}

void UUI_MachineInteract::NativeConstruct()
{
    Super::NativeConstruct();

    if (BTN_Close)
    {
        BTN_Close->OnClicked.AddDynamic(this, &UUI_MachineInteract::OnCloseClicked);
    }

    if (BTN_Repair)
    {
        BTN_Repair->OnClicked.AddDynamic(this, &UUI_MachineInteract::OnRepairClicked);
    }
}

void UUI_MachineInteract::NativeDestruct()
{
    OnClosed.Broadcast();

    Super::NativeDestruct();
}

void UUI_MachineInteract::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    
    if (!TargetMachine) return;
    
    // 기계가 고집하는 레시피 대신, 기계 입력 인벤토리에 '진짜 들어있는 재료'를 직접 긁어옵니다
    FName InputName = NAME_None;
    int32 InputAmount = 0;

    // 기계 내부 입력 주머니를 순회하여 가장 먼저 발견되는 아이템을 UI 표적으로 삼습니다.
    const TMap<FName, int32>& InputInv = TargetMachine->GetInputInventory();
    for (const auto& Pair : InputInv)
    {
        if (Pair.Value > 0)
        {
            InputName = Pair.Key;
            InputAmount = Pair.Value;
            break; // 재료를 찾았으니 루프 탈출
        }
    }

    // 좌측 입력(Input) UI 갱신
    UpdateInputUI(InputName, InputAmount, TargetMachine->GetMaxInput());

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
    // 우측 출력(Output) UI 갱신
        OutputName = Recipe.OutputItem1;
    if (!ManualDroppedOutputItemID.IsNone())
    {
            OutputName = ManualDroppedOutputItemID;
    }
        OutputAmount = TargetMachine->GetOutputBuffer().FindRef(OutputName);
    }
    
    // 우측 출력(Output) UI 갱신 (이제 None으로 밀리지 않고 iron_ore가 똑바로 유지됩니다)
    UpdateOutputUI(OutputName, OutputAmount, MaxOutputAmount);

    // 상태 텍스트 갱신 
    EMachineState State = TargetMachine->GetMachineState();
    switch (State)
    {
    case EMachineState::Working: UpdateMachineState(TEXT("가동 중"), FLinearColor::Green); break;
    case EMachineState::Idle:    UpdateMachineState(TEXT("대기 중"), FLinearColor::Gray); break;
    case EMachineState::Blocked: UpdateMachineState(TEXT("출력 꽉 참"), FLinearColor::Red); break;
    case EMachineState::NoPower: UpdateMachineState(TEXT("전력 없음"), FLinearColor::Red); break;
    default:                     UpdateMachineState(TEXT("정지됨"), FLinearColor::White); break;
    }

    // 중앙 생산 진행도(프로그래스 바) 갱신
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
    
    float CurrentDur = TargetMachine->GetCurrentDurability();
    float MaxDur = TargetMachine->GetMaxDurability();
    
    UpdateDurabilityUI(CurrentDur, MaxDur);
}

void UUI_MachineInteract::UpdateInputUI(FName ItemName, int32 CurrentAmount, int32 MaxAmount)
{
    const FName DisplayItemName = ItemName.IsNone() ? LastInputVisualItemID : ItemName;
    if (!ItemName.IsNone())
    {
        LastInputVisualItemID = ItemName;
    }

    if (TXT_InputName && TXT_InputCount && PB_InputBuffer)
    {
        TXT_InputName->SetText(GetResourceDisplayText(ResourceDataTable, DisplayItemName));
        
        FString CountStr = FString::Printf(TEXT("%d / %d"), CurrentAmount, MaxAmount);
        TXT_InputCount->SetText(FText::FromString(CountStr));
        
        float FillPercent = (MaxAmount > 0) ? (float)CurrentAmount / MaxAmount : 0.0f;
        PB_InputBuffer->SetPercent(FillPercent);
    }

    if (DisplayItemName.IsNone())
    {
        if (IMG_InputIcon) IMG_InputIcon->SetVisibility(ESlateVisibility::Hidden);
        return;
    }

    // 왜 이미지가 안 바뀌는지 추적
    if (!ResourceDataTable)
    {
        UE_LOG(LogTemp, Error, TEXT("[UI 에러] ResourceDataTable 변수가 Null입니다! WBP_MachineInteract 블루프린트 디테일 창에서 아이템 데이터 테이블을 할당했는지 확인하세요."));
        return;
    }

    if (IMG_InputIcon)
    {
        IMG_InputIcon->SetVisibility(ESlateVisibility::Visible);
        
        FResourceData* RowData = ResourceDataTable->FindRow<FResourceData>(DisplayItemName, TEXT("FindInputIconContext"));
        if (RowData)
        {
            if (RowData->ImgAsset.IsValid()) IMG_InputIcon->SetBrushFromTexture(RowData->ImgAsset.Get());
            else
            {
                UTexture2D* LoadedTexture = RowData->ImgAsset.LoadSynchronous();
                if (LoadedTexture) IMG_InputIcon->SetBrushFromTexture(LoadedTexture);
            }
            
            if (CurrentAmount <= 0) IMG_InputIcon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.15f));
            else                    IMG_InputIcon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
        }
    }
}

void UUI_MachineInteract::UpdateOutputUI(FName ItemName, int32 CurrentAmount, int32 MaxAmount)
{
    const FName DisplayItemName = ItemName.IsNone() ? LastOutputVisualItemID : ItemName;
    if (!ItemName.IsNone())
    {
        LastOutputVisualItemID = ItemName;
    }

    if (TXT_OutputName && TXT_OutputCount && PB_OutputBuffer)
    {
        // 1. 산출물 이름 텍스트 세팅
        TXT_OutputName->SetText(GetResourceDisplayText(ResourceDataTable, DisplayItemName));
        
        // 2. 수량 텍스트 세팅 (예: "850 / 2000")
        FString CountStr = FString::Printf(TEXT("%d / %d"), CurrentAmount, MaxAmount);
        TXT_OutputCount->SetText(FText::FromString(CountStr));
        
        // 3. 출력 버퍼 프로그래스 바(게이지) 세팅
        float FillPercent = (MaxAmount > 0) ? (float)CurrentAmount / MaxAmount : 0.0f;
        PB_OutputBuffer->SetPercent(FillPercent);
    }
    if (DisplayItemName.IsNone())
    {
        if (IMG_OutputIcon) IMG_OutputIcon->SetVisibility(ESlateVisibility::Hidden);
        return;
    }

    if (ResourceDataTable && IMG_OutputIcon)
    {
        if (FResourceData* RowData = ResourceDataTable->FindRow<FResourceData>(DisplayItemName, TEXT("FindOutputIconContext")))
        {
            UTexture2D* IconTexture = nullptr;
            if (RowData->ImgAsset.IsValid())
            {
                IconTexture = RowData->ImgAsset.Get();
            }
            else
            {
                IconTexture = RowData->ImgAsset.LoadSynchronous();
            }

            if (IconTexture)
            {
                IMG_OutputIcon->SetVisibility(ESlateVisibility::Visible);
                IMG_OutputIcon->SetBrushFromTexture(IconTexture);
            }
            else
            {
                IMG_OutputIcon->SetBrush(FSlateBrush());
                IMG_OutputIcon->SetVisibility(ESlateVisibility::Hidden);
                return;
            }
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

void UUI_MachineInteract::UpdateMachineState(FString StateText, FLinearColor StateColor)
{
    if (TXT_MachineState)
    {
        TXT_MachineState->SetText(FText::FromString(StateText));
        TXT_MachineState->SetColorAndOpacity(FSlateColor(StateColor));
    }
}

void UUI_MachineInteract::UpdateCraftingProgress(float Percent)
{
    if (PB_CraftingProgress && TXT_ProgressPercent)
    {
        PB_CraftingProgress->SetPercent(Percent);
        
        int32 PercentInt = FMath::RoundToInt(Percent * 100.0f);
        
        FString ProgressStr = FString::Printf(TEXT("진행도: %d%%"), PercentInt);
        
        TXT_ProgressPercent->SetText(FText::FromString(ProgressStr));
    }
}
void UUI_MachineInteract::UpdateMachineName(const FText& MachineName)
{
    if (TXT_MachineName)
    {
        TXT_MachineName->SetText(MachineName);
    }
}

void UUI_MachineInteract::OnCloseClicked()
{
    RemoveFromParent();
}

void UUI_MachineInteract::OnRepairClicked()
{
    if (!TargetMachine)
    {
        return;
    }

    TargetMachine->RepairUsingWarehouse();
}

void UUI_MachineInteract::UpdateDurabilityUI(float CurrentDurability, float MaxDurability)
{
    if (TXT_DurabilityPercent && PB_Durability)
    {
        // 프로그래스 바 비율 계산 (0.0 ~ 1.0)
        float SafeMax = (MaxDurability > 0.f) ? MaxDurability : 100.f;
        float Percent = FMath::Clamp(CurrentDurability / SafeMax, 0.0f, 1.0f);
        PB_Durability->SetPercent(Percent);

        // 수치 데이터 정수형 반올림 계산
        int32 CurrentInt = FMath::RoundToInt(CurrentDurability);
        int32 MaxInt = FMath::RoundToInt(SafeMax);
        int32 PercentInt = FMath::RoundToInt(Percent * 100.0f);

        // 내구도 현재값 / 최대값
        FString DurabilityStr = FString::Printf(TEXT("내구도: %d / %d"), CurrentInt, MaxInt);

        TXT_DurabilityPercent->SetText(FText::FromString(DurabilityStr));
    }
}

bool UUI_MachineInteract::NativeOnDrop(const FGeometry& MyGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    UItemDragDropOperation* ItemDragOp = Cast<UItemDragDropOperation>(InOperation);
    if (!ItemDragOp || !TargetMachine) return false;
    
    // 가시성이 꺼져서 지오메트리가 터지는 이미지 대신, 
    // 항상 크기가 고정되어 켜져 있는 B_OutputDropZone 보더 영역을 타겟으로 검사합니다
    if (B_OutputDropZone)
    {
        FVector2D DropScreenPos = InDragDropEvent.GetScreenSpacePosition();
        
        // 보더 영역 내부로 마우스가 정확히 골인했는지 대조
        bool bIsOverZone = USlateBlueprintLibrary::IsUnderLocation(
            B_OutputDropZone->GetCachedGeometry(), 
            DropScreenPos
        );

        if (!bIsOverZone)
        {
            return false; // 마우스 포인터가 칸 밖으로 나갔다면 투입 실패 처리
        }
    }
    else
    {
        return false;
    }

    FName DroppedItemID = ItemDragOp->DraggedItemID;
    if (DroppedItemID.IsNone())
    {
        return false;
    }

    if (AWarehousePort* WarehousePort = Cast<AWarehousePort>(TargetMachine))
    {
        const FName PreviousOutputItem = WarehousePort->GetSelectedOutputItem();
        WarehousePort->SetSelectedOutputItem(DroppedItemID);
        if (WarehousePort->GetSelectedOutputItem() != DroppedItemID && PreviousOutputItem != DroppedItemID)
        {
            return false;
        }

        ManualDroppedOutputItemID = DroppedItemID;

        if (UGameInstance* GI = GetGameInstance())
        {
            if (UQuestManagerSubsystem* QuestManager = GI->GetSubsystem<UQuestManagerSubsystem>())
            {
                QuestManager->NotifyTutorialEvent(TEXT("WarehouseOutputItemSet"), DroppedItemID);
            }
        }

        UE_LOG(LogTemp, Log, TEXT("[WarehousePort] Selected output item: %s"), *DroppedItemID.ToString());
        return true;
    }

    if (ALiquidTank* LiquidTank = Cast<ALiquidTank>(TargetMachine))
    {
        const FName PreviousOutputLiquid = LiquidTank->GetSelectedOutputLiquid();
        LiquidTank->SetSelectedOutputLiquid(DroppedItemID);
        if (LiquidTank->GetSelectedOutputLiquid() != DroppedItemID && PreviousOutputLiquid != DroppedItemID)
        {
            return false;
        }

        ManualDroppedOutputItemID = DroppedItemID;

        UE_LOG(LogTemp, Log, TEXT("[LiquidTank] Selected output liquid: %s"), *DroppedItemID.ToString());
        return true;
    }

    // --- 이하 서브시스템 아이템 차감 및 이미지/ID 세팅 로직 동일 ---
    int32 CurrentInputAmount = TargetMachine->GetInputInventory().FindRef(DroppedItemID);
    if (CurrentInputAmount >= TargetMachine->GetMaxInput()) return false;

    UGameInstance* GI = GetGameInstance();
    if (GI)
    {
        UPlayerWarehouseSubsystem* WarehouseSubsystem = GI->GetSubsystem<UPlayerWarehouseSubsystem>();
        if (WarehouseSubsystem && WarehouseSubsystem->TakeItem(DroppedItemID, 1)) 
        {
            TargetMachine->AddItem(DroppedItemID, 1);
            ManualDroppedOutputItemID = DroppedItemID;
            
            // 드롭 성공 시 알맹이 이미지 컴포넌트 데이터 갱신 및 가시성 ON!
            if (ResourceDataTable && IMG_OutputIcon)
            {
                FResourceData* RowData = ResourceDataTable->FindRow<FResourceData>(DroppedItemID, TEXT("FindDroppedOutputIconContext"));
                if (RowData)
                {
                    UTexture2D* IconTexture = nullptr;
                    if (RowData->ImgAsset.IsValid())
                    {
                        IconTexture = RowData->ImgAsset.Get();
                    }
                    else
                    {
                        IconTexture = RowData->ImgAsset.LoadSynchronous();
                    }

                    if (IconTexture)
                    {
                        IMG_OutputIcon->SetVisibility(ESlateVisibility::Visible);
                        IMG_OutputIcon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
                        IMG_OutputIcon->SetBrushFromTexture(IconTexture);
                    }
                    else
                    {
                        IMG_OutputIcon->SetBrush(FSlateBrush());
                        IMG_OutputIcon->SetVisibility(ESlateVisibility::Hidden);
                    }
                }
            }

            // 가방 UI 새로고침
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
