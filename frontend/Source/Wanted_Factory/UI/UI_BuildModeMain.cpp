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
    //case 1: BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Storage); break;      // 1번 슬롯: 창고
    case 2: BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Conveyor); break;     // 2번 슬롯: 컨베이어
    //case 3: BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Smelter); break;      // 3번 슬롯: 제련기
    case 4: BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Grinder); break;      // 4번 슬롯: 분쇄기
    case 5: BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Miner); break;        // 5번 슬롯: 채굴기
    
        // case 6번은 현재 비어있으므로 필요 시 나중에 추가 가능합니다.

    case 7: BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::PowerPlant); break;   // 7번 슬롯: 발전소
    case 8: BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::PowerNode); break;   // 8번 슬롯: 송전탑 (기존 PowerNode)
    case 9: BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::PowerLine); break;   // 9번 슬롯: 송전선
    case 0: BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Shield); break;      // 0번 슬롯: 차폐막 (기존 Shield)
    
    default: break;
    }
}