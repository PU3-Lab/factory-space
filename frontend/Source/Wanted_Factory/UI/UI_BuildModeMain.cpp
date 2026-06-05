#include "UI_BuildModeMain.h"
#include "Components/Button.h"
#include "OJJ_BuildController.h"
#include "Kismet/GameplayStatics.h"

void UUI_BuildModeMain::NativeConstruct()
{
    Super::NativeConstruct();
    
    if (BTN_Slot_Machine)    BTN_Slot_Machine->OnClicked.AddDynamic(this, &UUI_BuildModeMain::OnMachineClicked);
    if (BTN_Slot_Conveyor)   BTN_Slot_Conveyor->OnClicked.AddDynamic(this, &UUI_BuildModeMain::OnConveyorClicked);
    if (BTN_Slot_PowerNode)  BTN_Slot_PowerNode->OnClicked.AddDynamic(this, &UUI_BuildModeMain::OnPowerNodeClicked);
    if (BTN_Slot_Shield)     BTN_Slot_Shield->OnClicked.AddDynamic(this, &UUI_BuildModeMain::OnShieldClicked);
    if (BTN_Slot_PowerPlant) BTN_Slot_PowerPlant->OnClicked.AddDynamic(this, &UUI_BuildModeMain::OnPowerPlantClicked);
    if (BTN_Slot_PowerLine)  BTN_Slot_PowerLine->OnClicked.AddDynamic(this, &UUI_BuildModeMain::OnPowerLineClicked);
}

// ExecutePlacementMode 호출
void UUI_BuildModeMain::OnMachineClicked()    { ExecutePlacementMode(0); } // 1번 슬롯
void UUI_BuildModeMain::OnPowerPlantClicked() { ExecutePlacementMode(6); } // 7번 슬롯
void UUI_BuildModeMain::OnShieldClicked()     { ExecutePlacementMode(7); } // 8번 슬롯
void UUI_BuildModeMain::OnPowerNodeClicked()  { ExecutePlacementMode(8); } // 9번 슬롯
void UUI_BuildModeMain::OnConveyorClicked()   { ExecutePlacementMode(9); } // 0번 슬롯
void UUI_BuildModeMain::OnPowerLineClicked()  { ExecutePlacementMode(10); } // -번 슬롯

// switch-case 제어 타겟 함수
void UUI_BuildModeMain::ExecutePlacementMode(int32 SlotIndex)
{
    AOJJ_BuildController* BuildController = Cast<AOJJ_BuildController>(
       UGameplayStatics::GetActorOfClass(GetWorld(), AOJJ_BuildController::StaticClass()));

    if (!BuildController) return;
    
    switch (SlotIndex)
    {
        case 0:  BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Machine); break;     // 1번 슬롯 칸: 기계
        case 6:  BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::PowerPlant); break;  // 7번 슬롯 칸: 발전소
        case 7:  BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Shield); break;      // 8번 슬롯 칸: 쉴드
        case 8:  BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::PowerNode); break;   // 9번 슬롯 칸: 전력 노드
        case 9:  BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Conveyor); break;    // 0번 슬롯 칸: 컨베이어
        case 10: BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::PowerLine); break;   // -번 슬롯 칸: 전선
        default: break;
    }
}