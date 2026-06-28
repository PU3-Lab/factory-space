#include "UI/UI_MoldingMachineInteract.h"

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
		TargetMoldingMachine->SetMoldingShape(SelectedItem);
		UE_LOG(LogTemp, Log, TEXT("[성형기 UI] 가공 모드가 다음으로 변경됨: %s"), *SelectedItem);
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
	const float MaxTime = TargetMoldingMachine->GetProcessTime();

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
        if (TXT_InputName)  TXT_InputName->SetText(GetResourceDisplayText(ResourceDataTable, InputName));
        if (TXT_InputCount) TXT_InputCount->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), InputAmount, MaxInputAmount)));
        if (PB_InputBuffer) PB_InputBuffer->SetPercent((MaxInputAmount > 0) ? (float)InputAmount / MaxInputAmount : 0.0f);
        
        // 데이터 테이블에서 에셋 텍스처를 실시간 로드해 바인딩합니다.
        if (IMG_InputIcon && ResourceDataTable)
        {
            IMG_InputIcon->SetVisibility(ESlateVisibility::Visible);
            FResourceData* RowData = ResourceDataTable->FindRow<FResourceData>(InputName, TEXT("FindInputIconContext"));
            if (RowData)
            {
                UTexture2D* IconTex = RowData->ImgAsset.IsValid() ? RowData->ImgAsset.Get() : RowData->ImgAsset.LoadSynchronous();
                if (IconTex) IMG_InputIcon->SetBrushFromTexture(IconTex);
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

    // ── 6. 출력 버퍼(Output Buffer) 동기화 ──
    FName OutputName = NAME_None;
    int32 OutputAmount = 0;
    int32 MaxOutputAmount = TargetMoldingMachine->GetMaxOutput();

    FRecipeTable Recipe = TargetMoldingMachine->GetCurrentRecipe();
    OutputName = Recipe.OutputItem1;

    if (!OutputName.IsNone())
    {
        OutputAmount = TargetMoldingMachine->GetOutputBuffer().FindRef(OutputName);
    }

    if (OutputAmount > 0)
    {
        //출력 명칭도 데이터 테이블 한글 텍스트 명칭으로
        if (TXT_OutputName)  TXT_OutputName->SetText(GetResourceDisplayText(ResourceDataTable, OutputName));
        if (TXT_OutputCount) TXT_OutputCount->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), OutputAmount, MaxOutputAmount)));
        if (PB_OutputBuffer) PB_OutputBuffer->SetPercent((MaxOutputAmount > 0) ? (float)OutputAmount / MaxOutputAmount : 0.0f);
        
        // 출력 아이콘 썸네일 텍스처 실시간 스왑
        if (IMG_OutputIcon && ResourceDataTable)
        {
            IMG_OutputIcon->SetVisibility(ESlateVisibility::Visible);
            FResourceData* RowData = ResourceDataTable->FindRow<FResourceData>(OutputName, TEXT("FindOutputIconContext"));
            if (RowData)
            {
                UTexture2D* IconTex = RowData->ImgAsset.IsValid() ? RowData->ImgAsset.Get() : RowData->ImgAsset.LoadSynchronous();
                if (IconTex) IMG_OutputIcon->SetBrushFromTexture(IconTex);
            }
            IMG_OutputIcon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
        }
    }
    else
    {
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
