#include "UI_BuildModeMain.h"
#include "Components/Button.h"
#include "Components/ContentWidget.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "OJJ_BuildController.h"
#include "Machines/MachineSubsystem.h"
#include "QuestManagerSubsystem.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	void CollectImageWidgetsRecursive(UWidget* Widget, TArray<UImage*>& OutImages)
	{
		if (!Widget)
		{
			return;
		}

		if (UImage* Image = Cast<UImage>(Widget))
		{
			OutImages.Add(Image);
			return;
		}

		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 ChildIndex = 0; ChildIndex < Panel->GetChildrenCount(); ++ChildIndex)
			{
				CollectImageWidgetsRecursive(Panel->GetChildAt(ChildIndex), OutImages);
			}
			return;
		}

		if (UContentWidget* Content = Cast<UContentWidget>(Widget))
		{
			CollectImageWidgetsRecursive(Content->GetContent(), OutImages);
		}
	}

	void ApplyMachineIconToButton(UButton* Button, const FMachineTableRow& MachineData)
	{
		if (!Button)
		{
			return;
		}

		UTexture2D* Texture = MachineData.ImgAsset.IsValid()
			? MachineData.ImgAsset.Get()
			: MachineData.ImgAsset.LoadSynchronous();
		if (!Texture)
		{
			return;
		}

		TArray<UImage*> Images;
		CollectImageWidgetsRecursive(Button->GetContent(), Images);
		for (UImage* Image : Images)
		{
			if (Image)
			{
				Image->SetBrushFromTexture(Texture);
			}
		}

		if (Images.Num() == 0)
		{
			FButtonStyle ButtonStyle = Button->GetStyle();
			ButtonStyle.Normal.SetResourceObject(Texture);
			ButtonStyle.Hovered.SetResourceObject(Texture);
			ButtonStyle.Pressed.SetResourceObject(Texture);
			ButtonStyle.Disabled.SetResourceObject(Texture);
			Button->SetStyle(ButtonStyle);
		}
	}
}

void UUI_BuildModeMain::NativeConstruct()
{
    Super::NativeConstruct();
    
    // 1. 위젯 버튼과 클릭 함수 1:1 바인딩
    if (BTN_Slot_1_Storage)        BTN_Slot_1_Storage->OnClicked.AddDynamic(this, &UUI_BuildModeMain::OnStorageClicked);
    if (BTN_Slot_2_Conveyor)       BTN_Slot_2_Conveyor->OnClicked.AddDynamic(this, &UUI_BuildModeMain::OnConveyorClicked);
    if (BTN_Slot_3_Smelter)        BTN_Slot_3_Smelter->OnClicked.AddDynamic(this, &UUI_BuildModeMain::OnSmelterClicked);
    if (BTN_Slot_4_Grinder)        BTN_Slot_4_Grinder->OnClicked.AddDynamic(this, &UUI_BuildModeMain::OnGrinderClicked);
    if (BTN_Slot_5_Miner)          BTN_Slot_5_Miner->OnClicked.AddDynamic(this, &UUI_BuildModeMain::OnMinerClicked);
    
    if (BTN_Slot_7_PowerPlant)     BTN_Slot_7_PowerPlant->OnClicked.AddDynamic(this, &UUI_BuildModeMain::OnPowerPlantClicked);
    if (BTN_Slot_8_PowerGridNode)  BTN_Slot_8_PowerGridNode->OnClicked.AddDynamic(this, &UUI_BuildModeMain::OnPowerGridNodeClicked);
    if (BTN_Slot_9_PowerLine)      BTN_Slot_9_PowerLine->OnClicked.AddDynamic(this, &UUI_BuildModeMain::OnPowerLineClicked);
    if (BTN_Slot_0_MagneticShield) BTN_Slot_0_MagneticShield->OnClicked.AddDynamic(this, &UUI_BuildModeMain::OnMagneticShieldClicked);

    RefreshSlotIcons();
}

void UUI_BuildModeMain::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    RefreshSlotIcons();
}

void UUI_BuildModeMain::OnStorageClicked()        { ExecutePlacementMode(1); } // 1번: 창고
void UUI_BuildModeMain::OnConveyorClicked()       { ExecutePlacementMode(2); } // 2번: 컨베이어
void UUI_BuildModeMain::OnSmelterClicked()        { ExecutePlacementMode(3); } // 3번: 제련기
void UUI_BuildModeMain::OnGrinderClicked()        { ExecutePlacementMode(4); } // 4번: 분쇄기
void UUI_BuildModeMain::OnMinerClicked()          { ExecutePlacementMode(5); } // 5번: 채굴기

void UUI_BuildModeMain::OnPowerPlantClicked()     { ExecutePlacementMode(7); } // 7번: 발전소
void UUI_BuildModeMain::OnPowerGridNodeClicked()  { ExecutePlacementMode(8); } // 8번: 송전탑
void UUI_BuildModeMain::OnPowerLineClicked()      { ExecutePlacementMode(9); } // 9번: 송전선
void UUI_BuildModeMain::OnMagneticShieldClicked() { ExecutePlacementMode(0); } // 0번: 차폐막

// 3. 집중 제어 switch-case 함수
void UUI_BuildModeMain::RefreshSlotIcons()
{
    UGameInstance* GameInstance = GetGameInstance();
    UMachineSubsystem* MachineSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UMachineSubsystem>()
        : nullptr;
    if (!MachineSubsystem)
    {
        return;
    }

    const int32 ConveyorLevel = MachineSubsystem->GetMachineLevel(TEXT("Conveyor"));
    const int32 SmelterLevel = MachineSubsystem->GetMachineLevel(TEXT("Smelter"));
    const int32 GrinderLevel = MachineSubsystem->GetMachineLevel(TEXT("Grinder"));
    const int32 MinerLevel = MachineSubsystem->GetMachineLevel(TEXT("MinerMachine"));

    if (ConveyorLevel != CachedConveyorLevel)
    {
        FMachineTableRow MachineData;
        if (MachineSubsystem->FindMachineData(TEXT("Conveyor"), MachineData))
        {
            ApplyMachineIconToButton(BTN_Slot_2_Conveyor, MachineData);
            CachedConveyorLevel = ConveyorLevel;
        }
    }

    if (SmelterLevel != CachedSmelterLevel)
    {
        FMachineTableRow MachineData;
        if (MachineSubsystem->FindMachineData(TEXT("Smelter"), MachineData))
        {
            ApplyMachineIconToButton(BTN_Slot_3_Smelter, MachineData);
            CachedSmelterLevel = SmelterLevel;
        }
    }

    if (GrinderLevel != CachedGrinderLevel)
    {
        FMachineTableRow MachineData;
        if (MachineSubsystem->FindMachineData(TEXT("Grinder"), MachineData))
        {
            ApplyMachineIconToButton(BTN_Slot_4_Grinder, MachineData);
            CachedGrinderLevel = GrinderLevel;
        }
    }

    if (MinerLevel != CachedMinerLevel)
    {
        FMachineTableRow MachineData;
        if (MachineSubsystem->FindMachineData(TEXT("MinerMachine"), MachineData))
        {
            ApplyMachineIconToButton(BTN_Slot_5_Miner, MachineData);
            CachedMinerLevel = MinerLevel;
        }
    }
}

void UUI_BuildModeMain::ExecutePlacementMode(int32 SlotIndex)
{
    AOJJ_BuildController* BuildController = Cast<AOJJ_BuildController>(
       UGameplayStatics::GetActorOfClass(GetWorld(), AOJJ_BuildController::StaticClass()));

    if (!BuildController) return;
    
    // 단축키 번호
    switch (SlotIndex)
    {
        case 1: BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Warehouse); break;   // 1번: 창고
        case 2: BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Conveyor); break;    // 2번: 컨베이어
        case 3: BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Smelter); break;     // 3번: 제련기
        case 4: BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Grinder); break;     // 4번: 분쇄기
        case 5: BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Miner); break;       // 5번: 채굴기
        case 6: BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Pump); break;        // 6번 : 수력발전소
        case 7: BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::PowerPlant); break;  // 7번: 발전소
        case 8: BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::PowerNode); break;   // 8번: 송전탑
        case 9: BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::PowerLine); break;   // 9번: 송전선
        case 0: BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Shield); break;      // 0번: 차폐막
        
        default: break;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UQuestManagerSubsystem* QuestManager = GameInstance
        ? GameInstance->GetSubsystem<UQuestManagerSubsystem>()
        : nullptr;
    if (!QuestManager)
    {
        return;
    }

    switch (SlotIndex)
    {
        case 1: QuestManager->NotifyTutorialEvent(TEXT("SelectWarehouseMode")); break;
        case 2: QuestManager->NotifyTutorialEvent(TEXT("SelectConveyorMode")); break;
        case 3: QuestManager->NotifyTutorialEvent(TEXT("SelectSmelterMode")); break;
        case 5: QuestManager->NotifyTutorialEvent(TEXT("SelectMinerMode")); break;
        case 7: QuestManager->NotifyTutorialEvent(TEXT("SelectPowerPlantMode")); break;
        case 8: QuestManager->NotifyTutorialEvent(TEXT("SelectPowerNodeMode")); break;
        case 9: QuestManager->NotifyTutorialEvent(TEXT("SelectPowerLineMode")); break;
        default: break;
    }
}
