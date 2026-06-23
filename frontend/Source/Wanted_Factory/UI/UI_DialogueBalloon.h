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

	UFUNCTION(BlueprintCallable, Category = "Quest|UI")
	void ShowExternalDialogue(const FString& DialogueText);

	UFUNCTION(BlueprintCallable, Category = "Quest|UI")
	void ClearExternalDialogue();
	
	UPROPERTY(meta = (BindWidget))
	class UEditableText* ET_OperatorInput;
	
	virtual void NativeDestruct() override;
private:
	UFUNCTION()
	void HandleTutorialStepChanged(const FTutorialQuestStep& Step);

	UFUNCTION()
	void HandleTutorialDialogueLogged(const FString& QuestId, const FString& TriggerType, const TArray<FTutorialQuestDialogueLine>& Lines);

	void DisplayCurrentLine();
	
	UFUNCTION()
	void HandleOnTextCommitted(const FText& Text, ETextCommit::Type CommitType);

	UFUNCTION()
	void HandleOnOperatorGuideResponse(const FString& RequestId, const FString& Agent, const FString& PayloadJson, const FString& RawMessage);

	UFUNCTION()
	void HandleOnOperatorGuideError(const FString& RequestId, const FString& Agent, const FString& ErrorCode, const FString& ErrorMessage, const FString& RawMessage);
	
	UPROPERTY()
	class UQuestManagerSubsystem* QuestSubsystem;

	TArray<FTutorialQuestDialogueLine> CachedLines;
	bool bHasExternalDialogue = false;
	FString ExternalDialogueText;
};
