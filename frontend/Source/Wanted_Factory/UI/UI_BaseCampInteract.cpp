#include "UI/UI_BaseCampInteract.h"
#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "Blueprint/WidgetTree.h"
#include "MachineBase.h"
#include "UI_UpgradeNode.h"

void UUI_BaseCampInteract::SetTargetMachine(AMachineBase* InMachine)
{
	TargetBaseCamp = InMachine;
}

void UUI_BaseCampInteract::NativeConstruct()
{
	Super::NativeConstruct();

	// 1. 우측 메뉴 버튼 클릭 이벤트 멱등 바인딩
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

	// 2. 초기 디폴트 화면 상태 세팅 (이미지 기준 "레벨 업그레이드"를 먼저 활성화)
	SwitchSubPaneMode(EBaseCampSubMode::LevelUpgrade);
	RefreshAllUpgradeNodes();
}

void UUI_BaseCampInteract::NativeDestruct()
{
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

	// 위젯 스위처 내부의 인덱스 번호를 기반으로 화면을 통째로 교체합니다.
	// 0번 페이지 = 공장 상태 정보창, 1번 페이지 = 레벨 업그레이드 트리
	int32 TargetIndex = (NewMode == EBaseCampSubMode::FactoryStatus) ? 0 : 1;
	WS_SubPaneSwitcher->SetActiveWidgetIndex(TargetIndex);
    
	UE_LOG(LogTemp, Log, TEXT("[중앙 거점 UI] 서브 패널 인덱스 활성화 변경 -> %d"), TargetIndex);
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