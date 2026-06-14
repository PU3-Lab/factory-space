#include "UI_MachineInteract.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "Engine/DataTable.h"
#include "Resource/ResourceData.h"
#include "Machines/MachineTable.h"
#include "ItemDragDropOperation.h"
#include "OJJ_Player.h"
#include "UI/UI_Inventory.h"
#include "PlayerWarehouseSubsystem.h"

void UUI_MachineInteract::SetTargetMachine(AMachineBase* InMachine)
{
    TargetMachine = InMachine;
    
    if (TargetMachine)
    {
        FName MachineTypeName = TargetMachine->GetMachineType();
        UpdateMachineName(MachineTypeName.ToString());
        
        if (MachineDataTable && IMG_MachinePreview)
        {
            FMachineTableRow* RowData = MachineDataTable->FindRow<FMachineTableRow>(MachineTypeName, TEXT("FindMachinePreviewContext"));

            if (RowData)
            {
                IMG_MachinePreview->SetVisibility(ESlateVisibility::Visible);
                
                if (RowData->ImgAsset.IsValid())
                {
                    IMG_MachinePreview->SetBrushFromTexture(RowData->ImgAsset.Get());
                }
                else
                {
                    UTexture2D* LoadedTexture = RowData->ImgAsset.LoadSynchronous();
                    if (LoadedTexture)
                    {
                        IMG_MachinePreview->SetBrushFromTexture(LoadedTexture);
                    }
                }
            }
            else
            {
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
    
    FRecipeTable Recipe = TargetMachine->GetCurrentRecipe();

    // 좌측 입력(Input) UI 갱신
    FName InputName = Recipe.InputItem1;
    int32 InputAmount = TargetMachine->GetInputInventory().FindRef(InputName);
    UpdateInputUI(InputName, InputAmount, TargetMachine->GetMaxInput());

    // 우측 출력(Output) UI 갱신
    FName OutputName = Recipe.OutputItem1;
    int32 OutputAmount = TargetMachine->GetOutputBuffer().FindRef(OutputName);
    UpdateOutputUI(OutputName, OutputAmount, TargetMachine->GetMaxOutput());

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
    if (TXT_InputName && TXT_InputCount && PB_InputBuffer)
    {
        TXT_InputName->SetText(FText::FromName(ItemName));
        
        FString CountStr = FString::Printf(TEXT("%d / %d"), CurrentAmount, MaxAmount);
        TXT_InputCount->SetText(FText::FromString(CountStr));
        
        float FillPercent = (MaxAmount > 0) ? (float)CurrentAmount / MaxAmount : 0.0f;
        PB_InputBuffer->SetPercent(FillPercent);
    }
    if (ItemName.IsNone())
    {
        if (IMG_InputIcon) IMG_InputIcon->SetVisibility(ESlateVisibility::Hidden);
        return;
    }

    if (ResourceDataTable && IMG_InputIcon)
    {
        IMG_InputIcon->SetVisibility(ESlateVisibility::Visible);
        
        if (FResourceData* RowData = ResourceDataTable->FindRow<FResourceData>(ItemName, TEXT("FindInputIconContext")))
        {
            if (RowData->ImgAsset.IsValid()) IMG_InputIcon->SetBrushFromTexture(RowData->ImgAsset.Get());
            else
            {
                UTexture2D* LoadedTexture = RowData->ImgAsset.LoadSynchronous();
                if (LoadedTexture) IMG_InputIcon->SetBrushFromTexture(LoadedTexture);
            }
            if (CurrentAmount <= 0)
            {
                IMG_InputIcon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.15f));
            }
            else
            {
                IMG_InputIcon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
            }
        }
    }
}

void UUI_MachineInteract::UpdateOutputUI(FName ItemName, int32 CurrentAmount, int32 MaxAmount)
{
    if (TXT_OutputName && TXT_OutputCount && PB_OutputBuffer)
    {
        // 1. 산출물 이름 텍스트 세팅
        TXT_OutputName->SetText(FText::FromName(ItemName));
        
        // 2. 수량 텍스트 세팅 (예: "850 / 2000")
        FString CountStr = FString::Printf(TEXT("%d / %d"), CurrentAmount, MaxAmount);
        TXT_OutputCount->SetText(FText::FromString(CountStr));
        
        // 3. 출력 버퍼 프로그래스 바(게이지) 세팅
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

        if (FResourceData* RowData = ResourceDataTable->FindRow<FResourceData>(ItemName, TEXT("FindOutputIconContext")))
        {
            if (RowData->ImgAsset.IsValid()) IMG_OutputIcon->SetBrushFromTexture(RowData->ImgAsset.Get());
            else
            {
                UTexture2D* LoadedTexture = RowData->ImgAsset.LoadSynchronous();
                if (LoadedTexture) IMG_OutputIcon->SetBrushFromTexture(LoadedTexture);
            }
            if (CurrentAmount <= 0)
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
void UUI_MachineInteract::UpdateMachineName(FString MachineName)
{
    if (TXT_MachineName)
    {
        TXT_MachineName->SetText(FText::FromString(MachineName));
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
    // 1. 공중에 떠돌던 마우스 오퍼레이션을 아이템용 클래스로 캐스팅
    UItemDragDropOperation* ItemDragOp = Cast<UItemDragDropOperation>(InOperation);
    if (!ItemDragOp || !TargetMachine) return false;

    FName DroppedItemID = ItemDragOp->DraggedItemID;

    // 2. 현재 기계가 돌고 있는 레시피 구조체 가져오기
    FRecipeTable CurrentRecipe = TargetMachine->GetCurrentRecipe();
    
    // 유저가 던진 아이템이 이 기계가 '요구하는 재료1번'과 일치하는가?
    if (DroppedItemID != CurrentRecipe.InputItem1)
    {
        return false; 
    }

    // 이미 기계 입력 버퍼가 가득 차 있다면 투입 거부
    int32 CurrentInputAmount = TargetMachine->GetInputInventory().FindRef(DroppedItemID);
    if (CurrentInputAmount >= TargetMachine->GetMaxInput())
    {
        return false;
    }

    // 3. 서브시스템에 접근하여 내 가방(유저 인벤토리)에서 해당 아이템 1개 빼기
    UGameInstance* GI = GetGameInstance();
    if (GI)
    {
        UPlayerWarehouseSubsystem* WarehouseSubsystem = GI->GetSubsystem<UPlayerWarehouseSubsystem>();
        if (WarehouseSubsystem)
        {
            bool bSuccess = WarehouseSubsystem->TakeItem(DroppedItemID, 1); 
            
            if (bSuccess)
            {
                // 4. 서브시스템에서 차감 완료되었으니, 실제 기계 데이터 인벤토리에 1개 가산
                TargetMachine->AddItem(DroppedItemID, 1);
                
                // 오타가 난 네임스페이스 범위를 지우고 정석 포인터 타입으로 교체
                APlayerController* PC = GetOwningPlayer();
                if (PC)
                {
                    AOJJ_Player* OJJPlayer = Cast<AOJJ_Player>(PC->GetPawn());
                    
                    if (OJJPlayer && OJJPlayer->GetInventoryWidgetInstance()) 
                    {
                        // 유저님의 플레이어 가방 위젯 인스턴스에 접근해 강제 1회 리프레시
                        OJJPlayer->GetInventoryWidgetInstance()->RefreshInventoryWindow();
                    }
                }
                
                return true; 
            }
        }
    }

    return false;
}