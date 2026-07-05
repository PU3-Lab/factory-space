#include "UI/UI_MoldingMachineInteract.h"

#include "RecipeManagerSubsystem.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Resource/ResourceData.h"
#include "UI/UIInteractDisplayHelpers.h"
#include "Machines/MachineSubsystem.h"
#include "Machines/MoldingMachine.h"


using namespace UIInteractHelpers;

UUI_MoldingMachineInteract::UUI_MoldingMachineInteract(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UDataTable> ResourceTableFinder(
		TEXT("/Game/DataTable/DT_ResourceData.DT_ResourceData"));
	if (ResourceTableFinder.Succeeded())
	{
		ResourceDataTable = ResourceTableFinder.Object;
	}
}

void UUI_MoldingMachineInteract::SetTargetMachine(AMachineBase* InMachine)
{
	TargetMoldingMachine = Cast<AMoldingMachine>(InMachine);
}

void UUI_MoldingMachineInteract::NativeConstruct()
{
	Super::NativeConstruct();

	if (BTN_Repair)
	{
		BTN_Repair->OnClicked.RemoveDynamic(this, &UUI_MoldingMachineInteract::OnRepairClicked);
		BTN_Repair->OnClicked.AddDynamic(this, &UUI_MoldingMachineInteract::OnRepairClicked);
	}

	if (!CBS_MoldingShape)
	{
		return;
	}

	CBS_MoldingShape->OnSelectionChanged.RemoveDynamic(this, &UUI_MoldingMachineInteract::HandleOnShapeChanged);
	CBS_MoldingShape->OnSelectionChanged.AddDynamic(this, &UUI_MoldingMachineInteract::HandleOnShapeChanged);
	CBS_MoldingShape->ClearOptions();
	CBS_MoldingShape->AddOption(TEXT("판"));
	CBS_MoldingShape->AddOption(TEXT("봉"));
	CBS_MoldingShape->AddOption(TEXT("선"));

	if (TargetMoldingMachine)
	{
		CBS_MoldingShape->SetSelectedOption(TargetMoldingMachine->GetMoldingShape());
	}
}

void UUI_MoldingMachineInteract::HandleOnShapeChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (TargetMoldingMachine && !SelectedItem.IsEmpty())
	{
		// 1. 기계 내부의 가공 모양 문자열("판", "봉", "선")을 먼저 주입합니다.
		TargetMoldingMachine->SetMoldingShape(SelectedItem);
		TargetMoldingMachine->StopProcess();
		TargetMoldingMachine->TryStartProcess();

		UE_LOG(LogTemp, Log, TEXT("[성형기 UI] 가공 모드가 다음으로 변경됨: %s"), *SelectedItem);
	}
}

void UUI_MoldingMachineInteract::OnRepairClicked()
{
	if (TargetMoldingMachine)
	{
		TargetMoldingMachine->RepairUsingWarehouse();
	}
}

void UUI_MoldingMachineInteract::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!TargetMoldingMachine)
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UMachineSubsystem* MachineSubsystem = GameInstance ? GameInstance->GetSubsystem<UMachineSubsystem>() : nullptr;
	if (!MachineSubsystem)
	{
		return;
	}

	FMachineTableRow MachineData;
	if (MachineSubsystem->FindMachineData(TargetMoldingMachine->GetMachineType(), MachineData))
	{
		if (TXT_MachineName)
		{
			TXT_MachineName->SetText(FText::FromString(MachineData.DisplayName));
		}

		if (IMG_MachinePreview)
		{
			UTexture2D* MachineTex =
				MachineData.ImgAsset.IsValid() ? MachineData.ImgAsset.Get() : MachineData.ImgAsset.LoadSynchronous();
			if (MachineTex)
			{
				IMG_MachinePreview->SetBrushFromTexture(MachineTex);
			}
		}
	}

	if (TXT_MachineState)
	{
		const EMachineState State = TargetMoldingMachine->GetMachineState();
		switch (State)
		{
		case EMachineState::Working:
			TXT_MachineState->SetText(FText::FromString(TEXT("가공 중")));
			break;
		case EMachineState::Idle:
			TXT_MachineState->SetText(FText::FromString(TEXT("대기 중")));
			break;
		case EMachineState::Blocked:
			TXT_MachineState->SetText(FText::FromString(TEXT("출력 막힘")));
			break;
		case EMachineState::NoPower:
			TXT_MachineState->SetText(FText::FromString(TEXT("전력 없음")));
			break;
		default:
			TXT_MachineState->SetText(FText::FromString(TEXT("정지")));
			break;
		}
	}

	const float CurrentDur = TargetMoldingMachine->GetCurrentDurability();
	const float MaxDur = TargetMoldingMachine->GetMaxDurability();

	if (PB_Durability)
	{
		const float DurPercent = (MaxDur > 0.f) ? (CurrentDur / MaxDur) : 0.f;
		PB_Durability->SetPercent(FMath::Clamp(DurPercent, 0.f, 1.f));
	}

	if (TXT_DurabilityPercent)
	{
		const FString DurStr = FString::Printf(TEXT("내구도 %d / %d"), FMath::RoundToInt(CurrentDur), FMath::RoundToInt(MaxDur));
		TXT_DurabilityPercent->SetText(FText::FromString(DurStr));
	}

	const EMachineState CurrentState = TargetMoldingMachine->GetMachineState();
	const float MaxTime = TargetMoldingMachine->GetEffectiveProcessTime(TargetMoldingMachine->GetProcessTime());

	if (CurrentState == EMachineState::Working && MaxTime > 0.0f)
	{
		const float RemainTime = GetWorld()->GetTimerManager().GetTimerRemaining(TargetMoldingMachine->GetProcessTimer());
		const float Progress = FMath::Clamp(1.0f - (RemainTime / MaxTime), 0.0f, 1.0f);

		if (PB_CraftingProgress)
		{
			PB_CraftingProgress->SetPercent(Progress);
		}

		if (TXT_ProgressPercent)
		{
			const FString ProgStr = FString::Printf(TEXT("진행률 %d%%"), FMath::RoundToInt(Progress * 100.0f));
			TXT_ProgressPercent->SetText(FText::FromString(ProgStr));
		}
	}
	else
	{
		if (PB_CraftingProgress)
		{
			PB_CraftingProgress->SetPercent(0.0f);
		}

		if (TXT_ProgressPercent)
		{
			TXT_ProgressPercent->SetText(FText::FromString(TEXT("진행률 0%")));
		}
	}

	// ── 5. 입력 인벤토리(Input Inventory) 동기화 ──
    FName InputName = NAME_None;
    int32 InputAmount = 0;
    int32 MaxInputAmount = TargetMoldingMachine->GetMaxInput();

    const TMap<FName, int32>& InputInv = TargetMoldingMachine->GetInputInventory();
    for (const auto& Pair : InputInv)
    {
        if (Pair.Value > 0)
        {
            InputName = Pair.Key;
            InputAmount = Pair.Value;
            break;
        }
    }

    if (!InputName.IsNone())
    {
        // 데이터 테이블을 거쳐 한글 이름으로 변환합니다.
        if (TXT_InputName)  TXT_InputName->SetText(GetResourceDisplayText(this, ResourceDataTable, InputName));
        if (TXT_InputCount) TXT_InputCount->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), InputAmount, MaxInputAmount)));
        if (PB_InputBuffer) PB_InputBuffer->SetPercent((MaxInputAmount > 0) ? (float)InputAmount / MaxInputAmount : 0.0f);
        
        // 데이터 테이블에서 에셋 텍스처를 실시간 로드해 바인딩합니다.
        if (IMG_InputIcon)
        {
            if (UTexture2D* IconTex = GetResourceIconTexture(this, ResourceDataTable, InputName))
            {
                IMG_InputIcon->SetVisibility(ESlateVisibility::Visible);
                IMG_InputIcon->SetBrushFromTexture(IconTex);
            }
            IMG_InputIcon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
        }
    }
    else
    {
        if (TXT_InputName)  TXT_InputName->SetText(FText::FromString(TEXT("비어 있음")));
        if (TXT_InputCount) TXT_InputCount->SetText(FText::FromString(FString::Printf(TEXT("0 / %d"), MaxInputAmount)));
        if (PB_InputBuffer) PB_InputBuffer->SetPercent(0.0f);
        if (IMG_InputIcon)  IMG_InputIcon->SetVisibility(ESlateVisibility::Hidden);
    }

    // ── 6. 출력 버퍼(Output Buffer) 동기화 및 데이터 테이블 동적 로컬라이징 ──
    FName OutputName = NAME_None;
    int32 OutputAmount = 0;
    const int32 MaxOutputAmount = TargetMoldingMachine->GetMaxOutput();

    // 우선 기계가 현재 완벽하게 가동 중인 진짜 레시피가 있다면 그걸 최우선으로 가져옵니다.
    FRecipeTable Recipe = TargetMoldingMachine->GetCurrentRecipe();
    OutputName = Recipe.OutputItem1;

    // 레시피가 비어있거나 가동 직전이더라도, 
    // 투입된 원자재와 드롭다운 외형을 기반으로 RecipeManagerSubsystem에서 동적 탐색합니다.
    if (OutputName.IsNone() && !InputName.IsNone())
    {
        URecipeManagerSubsystem* RecipeManager = GameInstance ? GameInstance->GetSubsystem<URecipeManagerSubsystem>() : nullptr;
        if (RecipeManager)
        {
            // ① 유저가 선택한 드롭다운 텍스트를 시스템 Enum 규격(EResourceShape)으로 매핑
            EResourceShape DesiredShape = EResourceShape::None;
            FString CurrentShapeText = TargetMoldingMachine->GetMoldingShape();
            
            if (CurrentShapeText == TEXT("판"))      DesiredShape = EResourceShape::plate;
            else if (CurrentShapeText == TEXT("봉")) DesiredShape = EResourceShape::bar;
            else if (CurrentShapeText == TEXT("선")) DesiredShape = EResourceShape::wire;

            // ② 서브시스템을 통해 현재 투입된 원자재(철괴, 구리괴, 주석괴 등)로 만들 수 있는 모든 설계도를 dynamic하게 긁어옵니다.
            TArray<FRecipeTable> FoundRecipes;
            if (RecipeManager->FindRecipesByInputItem(InputName, FoundRecipes))
            {
                for (const FRecipeTable& Candidate : FoundRecipes)
                {
                    // 현재 기계 타입(MoldingMachine)의 레시피가 아니라면 스킵
                    if (Candidate.MachineType != TargetMoldingMachine->GetMachineType()) continue;

                    // ③ 후보 결과물 ID를 자원 테이블(DT_ResourceData)에서 조회하여, 
                    // 해당 자원의 shape(plate/bar/wire)가 유저가 선택한 모양과 일치하는 단 하나의 아웃풋을 찾아냅니다!
                    if (ResourceDataTable && !Candidate.OutputItem1.IsNone())
                    {
                        FResourceData* RowData = ResourceDataTable->FindRow<FResourceData>(Candidate.OutputItem1, TEXT("UI_Molding_DynamicPredict"));
                        if (RowData && RowData->shape == DesiredShape)
                        {
                            OutputName = Candidate.OutputItem1; // 🎯 동적 매핑 성공!
                            break; 
                        }
                    }
                }
            }
        }
    }

    // 확정된 OutputName을 기반으로 출력 버퍼 창고에 쌓여있는 실시간 수량을 가져옵니다 (0개부터 시작)
    if (!OutputName.IsNone())
    {
        OutputAmount = TargetMoldingMachine->GetOutputBuffer().FindRef(OutputName);
    }

    // ── [출력 UI 렌더링 구역] 수량 관계없이 이름과 아이콘 가이드라인 유지 ──
    if (!OutputName.IsNone())
    {
        if (TXT_OutputName)  TXT_OutputName->SetText(GetResourceDisplayText(this, ResourceDataTable, OutputName));
        if (TXT_OutputCount) TXT_OutputCount->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), OutputAmount, MaxOutputAmount)));
        if (PB_OutputBuffer) PB_OutputBuffer->SetPercent((MaxOutputAmount > 0) ? (float)OutputAmount / MaxOutputAmount : 0.0f);
        
        if (IMG_OutputIcon)
        {
            if (UTexture2D* IconTex = GetResourceIconTexture(this, ResourceDataTable, OutputName))
            {
                IMG_OutputIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible); // 팀원 패치 코드 유지
                IMG_OutputIcon->SetBrushFromTexture(IconTex);
            }
            
            // 수량이 0개일 때는 15% 불투명도로 흐릿한 가이드라인 연출, 쌓이면 선명하게(1.0) 연출
            if (OutputAmount <= 0) IMG_OutputIcon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.3f));
            else                   IMG_OutputIcon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
        }
    }
    else
    {
        // 공장에 재료가 아예 안 들어왔거나 매칭 레시피가 없는 순수 초기 공회전 상태
        if (TXT_OutputName)  TXT_OutputName->SetText(FText::FromString(TEXT("비어 있음")));
        if (TXT_OutputCount) TXT_OutputCount->SetText(FText::FromString(FString::Printf(TEXT("0 / %d"), MaxOutputAmount)));
        if (PB_OutputBuffer) PB_OutputBuffer->SetPercent(0.0f);
        if (IMG_OutputIcon)  IMG_OutputIcon->SetVisibility(ESlateVisibility::Hidden);
    }
}

void UUI_MoldingMachineInteract::NativeDestruct()
{
	OnClosed.Broadcast();
	Super::NativeDestruct();
}
