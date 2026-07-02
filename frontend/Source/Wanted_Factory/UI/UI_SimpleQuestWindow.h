#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI_SimpleQuestWindow.generated.h"

UCLASS()
class WANTED_FACTORY_API UUI_SimpleQuestWindow : public UUserWidget
{
	GENERATED_BODY()

protected:
	// 메인 퀘스트 이름과 현재 진행해야 할 목표 텍스트 바인딩
	UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_MainQuestTitle;
	UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_MainQuestObjective;

public:
	// 퀘스트 정보를 받아와서 화면 글자를 갱신해 줄 함수
	void UpdateSimpleQuest(const FText& Title, const FText& Objective);
};