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
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/Border.h"

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
    
    // 🌟 [핵심 수정] 기계가 고집하는 레시피 대신, 기계 입력 인벤토리에 '진짜 들어있는 재료'를 직접 긁어옵니다!
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

    FRecipeTable Recipe = TargetMachine->GetCurrentRecipe();
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

    // 🌟 [디버그 구역] 왜 이미지가 안 바뀌는지 원격 추적
    if (!ResourceDataTable)
    {
        UE_LOG(LogTemp, Error, TEXT("[UI 에러] ResourceDataTable 변수가 Null입니다! WBP_MachineInteract 블루프린트 디테일 창에서 아이템 데이터 테이블을 할당했는지 확인하세요."));
        return;
    }

    if (IMG_InputIcon)
    {
        IMG_InputIcon->SetVisibility(ESlateVisibility::Visible);
        
        FResourceData* RowData = ResourceDataTable->FindRow<FResourceData>(ItemName, TEXT("FindInputIconContext"));
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
            
            UE_LOG(LogTemp, Log, TEXT("[UI 성공] %s 아이템의 아이콘을 데이터 테이블에서 찾아 성공적으로 갱신했습니다!"), *ItemName.ToString());
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[UI 경고] 데이터 테이블에서 '%s' 라는 이름의 행(Row)을 찾을 수 없습니다. 대소문자나 이름을 확인하세요."), *ItemName.ToString());
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
    UItemDragDropOperation* ItemDragOp = Cast<UItemDragDropOperation>(InOperation);
    if (!ItemDragOp || !TargetMachine) return false;
    
    // 1. 마우스가 이미지 칸(DropZone) 위에 있는지 검사
    if (B_InputDropZone)
    {
        FVector2D DropScreenPos = InDragDropEvent.GetScreenSpacePosition();
        if (!USlateBlueprintLibrary::IsUnderLocation(B_InputDropZone->GetCachedGeometry(), DropScreenPos))
        {
            return false; 
        }
    }

    FName DroppedItemID = ItemDragOp->DraggedItemID;

    // 기계가 원하는 걸 검사하는 멍청한 비교문(CurrentRecipe 대조)을 흔적도 없이 삭제했습니다
    // 이제 유저가 던진 아이템이 무엇이든(iron_ore든, copper_ore든) 무조건 통과합니다.

    // 2. 수량 제한 검사 (기계 수용량이 꽉 찬 게 아니라면 허용)
    int32 CurrentInputAmount = TargetMachine->GetInputInventory().FindRef(DroppedItemID);
    if (CurrentInputAmount >= TargetMachine->GetMaxInput())
    {
        return false;
    }

    // 3. 내 가방에서 1개 빼고 기계에 1개 넣기
    UGameInstance* GI = GetGameInstance();
    if (GI)
    {
        UPlayerWarehouseSubsystem* WarehouseSubsystem = GI->GetSubsystem<UPlayerWarehouseSubsystem>();
        if (WarehouseSubsystem && WarehouseSubsystem->TakeItem(DroppedItemID, 1)) 
        {
            TargetMachine->AddItem(DroppedItemID, 1);
            
            // 가방 UI 새로고침
            APlayerController* PC = GetOwningPlayer();
            if (PC)
            {
                AOJJ_Player* OJJPlayer = Cast<AOJJ_Player>(PC->GetPawn());
                if (OJJPlayer && OJJPlayer->GetInventoryWidgetInstance()) 
                {
                    OJJPlayer->GetInventoryWidgetInstance()->RefreshInventoryWindow();
                }
            }
            
            UE_LOG(LogTemp, Log, TEXT("[드롭 성공] 유저가 원하는 아이템(%s)을 기계에 성공적으로 투입했습니다"), *DroppedItemID.ToString());
            return true; 
        }
    }

    return false;
}