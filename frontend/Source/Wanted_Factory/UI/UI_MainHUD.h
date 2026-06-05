#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI_MainHUD.generated.h"

UCLASS()
class WANTED_FACTORY_API UUI_MainHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	// 외부나 타이머 등에서 서브퀘스트를 새로 요청하고 싶을 때 부르는 함수
	UFUNCTION(BlueprintCallable, Category = "HUD | Quest")
	void RefreshSubQuests();

protected:
	
	UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_Quest_1;
	UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_Quest_2;
	UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_Quest_3;
	UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_Quest_4;
	UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_Quest_5;

private:
	
	UFUNCTION()
	void HandleOnSubQuestTitlesUpdated(const FString& RequestId, const TArray<FString>& Titles);
};