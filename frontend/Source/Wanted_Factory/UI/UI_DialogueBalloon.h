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
	UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_Dialogue;
	UPROPERTY(meta = (BindWidget)) class UWidget* DialogueContainer;

	virtual void NativeConstruct() override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

public:
	UFUNCTION(BlueprintCallable, Category = "Quest|UI")
	void RefreshDialogueUI();

private:
	UFUNCTION()
	void HandleTutorialStepChanged(const FTutorialQuestStep& Step);

	UFUNCTION()
	void HandleTutorialDialogueLogged(const FString& QuestId, const FString& TriggerType, const TArray<FTutorialQuestDialogueLine>& Lines);

	void DisplayCurrentLine();

	UPROPERTY()
	class UQuestManagerSubsystem* QuestSubsystem;

	TArray<FTutorialQuestDialogueLine> CachedLines;
};
