#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI_UpgradeNode.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UBorder;

UCLASS()
class WANTED_FACTORY_API UUI_UpgradeNode : public UUserWidget
{
	GENERATED_BODY()

public:
	// 🌟 에디터(UMG 디자이너 창)에서 직접 기계 타입과 해당 칸의 레벨을 지정할 수 있도록 노출합니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade Config")
	FName MachineType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade Config")
	int32 NodeLevel = 1;

	// 상태를 동적으로 새로고침하는 함수
	void RefreshNodeState();

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget)) UButton* BTN_Card;
	UPROPERTY(meta = (BindWidget)) UImage* IMG_MachineIcon;
	UPROPERTY(meta = (BindWidget)) UTextBlock* TXT_LevelLabel;
	UPROPERTY(meta = (BindWidget)) UBorder* B_HighlightBorder;

private:
	UFUNCTION() void OnCardClicked();
};