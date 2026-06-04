#include "UI_BuildModeMain.h"
#include "UI_Quickslot.h"
#include "Components/HorizontalBox.h"
#include "Components/PanelWidget.h"
#include "FactorySpaceTypes.h"
#include "Engine/Engine.h"
#include "OJJ_BuildController.h" 
#include "Kismet/GameplayStatics.h"

void UUI_BuildModeMain::NativeConstruct()
{
	Super::NativeConstruct();
	
	APlayerController* PC = GetOwningPlayer();
	if (PC && PC->InputComponent)
	{
		PC->InputComponent->BindKey(EKeys::One,   IE_Pressed, this, &UUI_BuildModeMain::OnKey1Pressed);
		PC->InputComponent->BindKey(EKeys::Two,   IE_Pressed, this, &UUI_BuildModeMain::OnKey2Pressed);
		PC->InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &UUI_BuildModeMain::OnKey3Pressed);
		PC->InputComponent->BindKey(EKeys::Four,  IE_Pressed, this, &UUI_BuildModeMain::OnKey4Pressed);
		PC->InputComponent->BindKey(EKeys::Five,  IE_Pressed, this, &UUI_BuildModeMain::OnKey5Pressed);
		PC->InputComponent->BindKey(EKeys::Six,   IE_Pressed, this, &UUI_BuildModeMain::OnKey6Pressed);
		PC->InputComponent->BindKey(EKeys::Seven, IE_Pressed, this, &UUI_BuildModeMain::OnKey7Pressed);
		PC->InputComponent->BindKey(EKeys::Eight, IE_Pressed, this, &UUI_BuildModeMain::OnKey8Pressed);
		PC->InputComponent->BindKey(EKeys::Nine,  IE_Pressed, this, &UUI_BuildModeMain::OnKey9Pressed);
		PC->InputComponent->BindKey(EKeys::Zero,  IE_Pressed, this, &UUI_BuildModeMain::OnKey0Pressed);
	}

	if (HBox_QuickslotBar)
	{
		TArray<FFactoryData*> AllFactoryRows;
		if (FactoryDataTable)
		{
			FactoryDataTable->GetAllRows<FFactoryData>(TEXT("UI_BuildMode_Context"), AllFactoryRows);
		}

		int32 ChildCount = HBox_QuickslotBar->GetChildrenCount();
        
		for (int32 i = 0; i < ChildCount; ++i)
		{
			UUI_Quickslot* QuickslotWidget = Cast<UUI_Quickslot>(HBox_QuickslotBar->GetChildAt(i));
			if (QuickslotWidget)
			{
				int32 DisplayNumber = (i == 9) ? 0 : i + 1;
				FText KeyText = FText::FromString(FString::FromInt(DisplayNumber));
				QuickslotWidget->InitSlot(i, KeyText);

				if (AllFactoryRows.IsValidIndex(i))
				{
					QuickslotWidget->SetBuildingData(*AllFactoryRows[i]);
				}

				// 각 슬롯이 클릭될 때 HandleQuickslotClicked 함수가 실행
				QuickslotWidget->OnSlotClickedDelegate.AddDynamic(this, &UUI_BuildModeMain::HandleQuickslotClicked);
			}
		}
	}
}

void UUI_BuildModeMain::HandleQuickslotClicked(int32 SlotIndex)
{
	UUI_Quickslot* ClickedSlot = Cast<UUI_Quickslot>(HBox_QuickslotBar->GetChildAt(SlotIndex));
	if (!ClickedSlot || ClickedSlot->IsEmpty()) return;
	
	const FFactoryData& SelectedData = ClickedSlot->GetAssignedData();
	
	FString UIDebugMsg = FString::Printf(TEXT("UI 클릭 성공 슬롯번호: %d, 읽은 데이터(ID): %s"), SlotIndex, *SelectedData.FactoryID.ToString());
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, UIDebugMsg);
	
	// 나중에 더미말고 재준이형꺼로 바꾸기
	AOJJ_BuildController* BuildController = Cast<AOJJ_BuildController>(
	   UGameplayStatics::GetActorOfClass(GetWorld(), AOJJ_BuildController::StaticClass()));
	
	if (!BuildController) 
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, TEXT("안내: 현재 맵에 BuildController가 없으므로 건설 연동은 생략합니다."));
		return;
	}

	// 기능 연동 로직
	if (SelectedData.FactoryID == "Conveyor") 
	{
		BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Conveyor);
	}
	else 
	{
		BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Machine);
	}
}

void UUI_BuildModeMain::OnKey1Pressed() { HandleQuickslotClicked(0); }
void UUI_BuildModeMain::OnKey2Pressed() { HandleQuickslotClicked(1); }
void UUI_BuildModeMain::OnKey3Pressed() { HandleQuickslotClicked(2); }
void UUI_BuildModeMain::OnKey4Pressed() { HandleQuickslotClicked(3); }
void UUI_BuildModeMain::OnKey5Pressed() { HandleQuickslotClicked(4); }
void UUI_BuildModeMain::OnKey6Pressed() { HandleQuickslotClicked(5); }
void UUI_BuildModeMain::OnKey7Pressed() { HandleQuickslotClicked(6); }
void UUI_BuildModeMain::OnKey8Pressed() { HandleQuickslotClicked(7); }
void UUI_BuildModeMain::OnKey9Pressed() { HandleQuickslotClicked(8); }
void UUI_BuildModeMain::OnKey0Pressed() { HandleQuickslotClicked(9); }