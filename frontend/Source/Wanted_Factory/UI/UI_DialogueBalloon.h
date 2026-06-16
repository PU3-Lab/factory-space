#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QuestManagerSubsystem.h" 
#include "UI_DialogueBalloon.generated.h"

UCLASS()
class WANTED_FACTORY_API UUI_DialogueBalloon : public UUserWidget
{
	GENERATED_BODY()

protected:
	// 말풍선 위젯 바인딩
	UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_Dialogue;
	UPROPERTY(meta = (BindWidget)) class UWidget* DialogueContainer; // 말풍선 배경 테두리

	virtual void NativeConstruct() override;

	// 말풍선 영역 클릭 감지를 위한 언리얼 내장 마우스 다운 함수 오버라이드
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

public:
	// 이찬님 명세: 최근 로그된 대사 조회 연동 함수
	UFUNCTION(BlueprintCallable, Category = "Quest|UI")
	void RefreshDialogueUI();

private:
	void DisplayCurrentLine();

	UPROPERTY()
	class UQuestManagerSubsystem* QuestSubsystem;

	TArray<FTutorialQuestDialogueLine> CachedLines;
	int32 CurrentLineIndex = 0;
};