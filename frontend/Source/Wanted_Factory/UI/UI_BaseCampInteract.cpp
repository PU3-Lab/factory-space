#include "UI/UI_BaseCampInteract.h"
#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "UI_UpgradeNode.h"
#include "MachineBase.h"
#include "FactoryManagerSubsystem.h"
#include "UI_FactoryStatusRow.h"
#include "Resource/ResourceData.h"

void UUI_BaseCampInteract::SetTargetMachine(AMachineBase* InMachine)
{
    TargetBaseCamp = InMachine;
}

void UUI_BaseCampInteract::NativeConstruct()
{
    Super::NativeConstruct();

    if (BTN_Tab_FactoryStatus)
    {
       BTN_Tab_FactoryStatus->OnClicked.RemoveDynamic(this, &UUI_BaseCampInteract::OnStatusTabClicked);
       BTN_Tab_FactoryStatus->OnClicked.AddDynamic(this, &UUI_BaseCampInteract::OnStatusTabClicked);
    }

    if (BTN_Tab_LevelUpgrade)
    {
       BTN_Tab_LevelUpgrade->OnClicked.RemoveDynamic(this, &UUI_BaseCampInteract::OnUpgradeTabClicked);
       BTN_Tab_LevelUpgrade->OnClicked.AddDynamic(this, &UUI_BaseCampInteract::OnUpgradeTabClicked);
    }

    // 초기 화면 상태 지정
    SwitchSubPaneMode(EBaseCampSubMode::LevelUpgrade);
    RefreshAllUpgradeNodes();
}

void UUI_BaseCampInteract::NativeDestruct()
{
    // 🌟 [신호 방송 복구] 거점 UI 창이 완전히 닫히거나 부서질 때, 수신 대기 중인 플레이어에게 신호를 뿜어 마우스 포커스를 게임 전용으로 리셋시킵니다.
    OnClosed.Broadcast();
    
    Super::NativeDestruct();
}

void UUI_BaseCampInteract::OnStatusTabClicked()
{
    SwitchSubPaneMode(EBaseCampSubMode::FactoryStatus);
}

void UUI_BaseCampInteract::OnUpgradeTabClicked()
{
    SwitchSubPaneMode(EBaseCampSubMode::LevelUpgrade);
}

void UUI_BaseCampInteract::SwitchSubPaneMode(EBaseCampSubMode NewMode)
{
    if (!WS_SubPaneSwitcher) return;

    int32 TargetIndex = (NewMode == EBaseCampSubMode::FactoryStatus) ? 0 : 1;
    WS_SubPaneSwitcher->SetActiveWidgetIndex(TargetIndex);
    
    if (NewMode == EBaseCampSubMode::FactoryStatus)
    {
        RefreshFactoryStatus();
    }
}

void UUI_BaseCampInteract::RefreshFactoryStatus()
{
    if (!TXT_PowerStatus || !SB_ResourceList) return;

    SB_ResourceList->ClearChildren();

    UGameInstance* GI = GetGameInstance();
    UFactoryManagerSubsystem* FactoryManager = GI ? GI->GetSubsystem<UFactoryManagerSubsystem>() : nullptr;
    if (!FactoryManager) return;

    // 전력 데이터 반영
    FFactoryPowerOverview PowerOverview = FactoryManager->GetFactoryPowerOverview();
    FString PowerString = FString::Printf(TEXT("%.1fW / %.1fW"), PowerOverview.CurrentDemandPower, PowerOverview.CurrentAvailablePower);
    TXT_PowerStatus->SetText(FText::FromString(PowerString));

    // 자원 목록 데이터 통계 연동
    TArray<FFactoryItemProductionStat> ProductionStats = FactoryManager->GetItemProductionStats();

    for (const FFactoryItemProductionStat& Stat : ProductionStats)
    {
        if (Stat.ItemID.IsNone()) continue;

        FText SolvedItemName = FText::FromName(Stat.ItemID);
        UTexture2D* SolvedIcon = nullptr;

        if (ResourceDataTable)
        {
            FResourceData* RowData = ResourceDataTable->FindRow<FResourceData>(Stat.ItemID, TEXT("BaseCamp_Status_Lookup"));
            if (RowData)
            {
                if (!RowData->DisplayName.IsEmpty())
                {
                    SolvedItemName = FText::FromString(RowData->DisplayName);
                }
                SolvedIcon = RowData->ImgAsset.IsValid() ? RowData->ImgAsset.Get() : RowData->ImgAsset.LoadSynchronous();
            }
        }

        if (FactoryStatusRowClass)
        {
            UUI_FactoryStatusRow* NewRow = CreateWidget<UUI_FactoryStatusRow>(this, FactoryStatusRowClass);
            if (NewRow)
            {
                // 첫 번째 인자에 'SolvedIcon'을 추가하여 헤더의 4개 인수 규칙과 완벽하게 일치시킵니다
                // 기존의 NewRow->SetItemIcon(SolvedIcon); 줄은 제거하시거나 주석 처리하시면 됩니다.
                NewRow->SetRowData(SolvedIcon, SolvedItemName, Stat.ActualProductionPerSecond, Stat.TheoreticalProductionPerSecond);

                SB_ResourceList->AddChild(NewRow);
            }
        }
    }
}

void UUI_BaseCampInteract::RefreshAllUpgradeNodes()
{
    if (!WidgetTree) return;
    TArray<UWidget*> AllWidgets;
    WidgetTree->GetAllWidgets(AllWidgets);
    for (UWidget* Widget : AllWidgets)
    {
       if (UUI_UpgradeNode* UpgradeNode = Cast<UUI_UpgradeNode>(Widget))
       {
          UpgradeNode->RefreshNodeState();
       }
    }
}