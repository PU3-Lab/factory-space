#include "UI_BuildModeMain.h"
#include "UI_Quickslot.h"
#include "Components/HorizontalBox.h"
#include "Components/PanelWidget.h"
#include "FactorySpaceTypes.h"
#include "Engine/Engine.h"

void UUI_BuildModeMain::NativeConstruct()
{
	Super::NativeConstruct();

	if (HBox_QuickslotBar)
	{
		TArray<FFactoryData*> AllFactoryRows;
        
		// 데이터 가져오기
		if (FactoryDataTable)
		{
			FactoryDataTable->GetAllRows<FFactoryData>(TEXT("UI_BuildMode_Context"), AllFactoryRows);
		}
		else
		{
			// 빨간색으로 경고 띄우기
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("에러: WBP_BuildModeMain에서 Factory Data Table을 지정해주세요!"));
		}

		int32 ChildCount = HBox_QuickslotBar->GetChildrenCount();
        
		for (int32 i = 0; i < ChildCount; ++i)
		{
			UUI_Quickslot* QuickslotWidget = Cast<UUI_Quickslot>(HBox_QuickslotBar->GetChildAt(i));
			if (QuickslotWidget)
			{
				// 단축키 숫자 1~0 세팅
				int32 DisplayNumber = (i == 9) ? 0 : i + 1;
				FText KeyText = FText::FromString(FString::FromInt(DisplayNumber));
				QuickslotWidget->InitSlot(i, KeyText);

				// 데이터 테이블에서 가져온 이미지 넣기
				if (AllFactoryRows.IsValidIndex(i))
				{
					QuickslotWidget->SetBuildingData(*AllFactoryRows[i]);
				}
			}
		}
	}
}