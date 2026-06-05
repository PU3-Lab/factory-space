#include "UI_MachineInteract.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"

void UUI_MachineInteract::SetTargetMachine(AMachineBase* InMachine)
{
    TargetMachine = InMachine;
    // 기계가 정상적으로 연결되었다면 이름표를 딱 한 번 갱신
    if (TargetMachine)
    {
        UpdateMachineName(TargetMachine->GetMachineType().ToString());
    }
}

void UUI_MachineInteract::NativeConstruct()
{
    Super::NativeConstruct();

    if (BTN_Close)
    {
        BTN_Close->OnClicked.AddDynamic(this, &UUI_MachineInteract::OnCloseClicked);
    }
}

void UUI_MachineInteract::NativeDestruct()
{
    // 모든 닫힘 경로를 한 곳에서 통지 — BTN_Close(OnCloseClicked→RemoveFromParent)든
    // 외부 RemoveFromParent든 위젯 파괴 직전 1회 Broadcast. 기존 라인 무수정, 추가만.
    OnClosed.Broadcast();

    Super::NativeDestruct();
}

void UUI_MachineInteract::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    
    if (!TargetMachine)
    {
        DummyProgress += InDeltaTime * 0.5f;
        if (DummyProgress > 1.0f) DummyProgress = 0.0f; 
        UpdateCraftingProgress(DummyProgress);
        
        float DummyDurability = 100.f - (DummyProgress * 100.f); 
        UpdateDurabilityUI(DummyDurability, 100.f);
        
        return;
    }
    
    if (!TargetMachine) return;
    // 기계가 현재 돌리고 있는 레시피 정보 가져오기
    FRecipeTable Recipe = TargetMachine->GetCurrentRecipe();

    // 좌측 입력(Input) UI 갱신
    FName InputName = Recipe.InputItem1;
    
    // 인벤토리(TMap)를 뒤져서 해당 재료가 몇 개 있는지 찾습니다. (없으면 0개)
    int32 InputAmount = TargetMachine->GetInputInventory().FindRef(InputName);
    UpdateInputUI(InputName, InputAmount, TargetMachine->GetMaxInput());

    // 우측 출력(Output) UI 갱신
    FName OutputName = Recipe.OutputItem1;
    // 버퍼(TMap)를 뒤져서 완성품이 몇 개 있는지 찾습니다.
    int32 OutputAmount = TargetMachine->GetOutputBuffer().FindRef(OutputName);
    UpdateOutputUI(OutputName, OutputAmount, TargetMachine->GetMaxOutput());

    // 상태 텍스트 갱신 (EMachineState 값에 따라 글씨와 색상 변경)
    EMachineState State = TargetMachine->GetMachineState();
    switch (State)
    {
        case EMachineState::Working: UpdateMachineState(TEXT("가동 중"), FLinearColor::Green); break;
        case EMachineState::Idle:    UpdateMachineState(TEXT("대기 중"), FLinearColor::Gray); break;
        case EMachineState::Blocked: UpdateMachineState(TEXT("출력 꽉 참"), FLinearColor::Red); break;
        case EMachineState::NoPower: UpdateMachineState(TEXT("전력 없음"), FLinearColor::Red); break;
        default:                     UpdateMachineState(TEXT("정지됨"), FLinearColor::White); break;
    }

    // 6. 중앙 프로그래스 바(진행률) 갱신
    float MaxTime = TargetMachine->GetProcessTime();
    if (State == EMachineState::Working && MaxTime > 0.0f)
    {
        // 엔진의 TimerManager를 통해 기계의 타이머에 남은 시간을 물어봅니다.
        float RemainTime = GetWorld()->GetTimerManager().GetTimerRemaining(TargetMachine->GetProcessTimer());
        
        // (진행된 시간 / 총 시간)으로 0.0 ~ 1.0 퍼센트를 구합니다.
        float Progress = 1.0f - (RemainTime / MaxTime);
        UpdateCraftingProgress(Progress);
    }
    else
    {
        // 가동 중이 아니면 0%로 고정
        UpdateCraftingProgress(0.0f);
    }
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

void UUI_MachineInteract::UpdateDurabilityUI(float CurrentDurability, float MaxDurability)
{
    if (TXT_DurabilityPercent && PB_Durability)
    {
        float SafeMax = (MaxDurability > 0.f) ? MaxDurability : 100.f;
        float Percent = FMath::Clamp(CurrentDurability / SafeMax, 0.0f, 1.0f);
        
        PB_Durability->SetPercent(Percent);

        // 2. 하나의 텍스트에 "내구도 현재값 / 최대값 (퍼센트)" 형태로 조립하기
        int32 CurrentInt = FMath::RoundToInt(CurrentDurability);
        int32 MaxInt = FMath::RoundToInt(SafeMax);
        
        FString DurabilityStr = FString::Printf(TEXT("내구도: %d / %d%%"), CurrentInt, MaxInt);

        TXT_DurabilityPercent->SetText(FText::FromString(DurabilityStr));
    }
}