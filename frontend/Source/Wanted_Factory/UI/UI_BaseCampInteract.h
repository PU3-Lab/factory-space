#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI_BaseCampInteract.generated.h"

class UButton;
class UWidgetSwitcher;
class AMachineBase;

// ── 중앙 거점 서브 화면 분류 상태 구조 정의 ──
UENUM(BlueprintType)
enum class EBaseCampSubMode : uint8
{
	FactoryStatus, // 공장 상태 레이아웃
	LevelUpgrade   // 기계 레벨 업그레이드 레이아웃
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBaseCampClosedSignature);

UCLASS()
class WANTED_FACTORY_API UUI_BaseCampInteract : public UUserWidget
{
	GENERATED_BODY()

public:
	// 타겟 중앙거점 머신 주입 통로
	void SetTargetMachine(AMachineBase* InMachine);
	void RefreshAllUpgradeNodes();
	
	UPROPERTY(BlueprintAssignable, Category = "BaseCamp | UI")
	FBaseCampClosedSignature OnClosed;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ── [우측 고정 카테고리 메뉴 버튼 바인딩] ──
	UPROPERTY(meta = (BindWidget)) UButton* BTN_Tab_FactoryStatus;
	UPROPERTY(meta = (BindWidget)) UButton* BTN_Tab_LevelUpgrade;

	// ── [좌측 메인 화면 제어 스위처 바인딩] ──
	UPROPERTY(meta = (BindWidget)) UWidgetSwitcher* WS_SubPaneSwitcher;

private:
	UPROPERTY() AMachineBase* TargetBaseCamp;

	// 카테고리 탭 전환 함수
	void SwitchSubPaneMode(EBaseCampSubMode NewMode);

	UFUNCTION() void OnStatusTabClicked();
	UFUNCTION() void OnUpgradeTabClicked();
};