#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FactoryAgentClientSubsystem.h"
#include "QuestManagerSubsystem.h"
#include "UI_DialogueBalloon.generated.h"

UCLASS()
class WANTED_FACTORY_API UUI_DialogueBalloon : public UUserWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_Dialogue;
	UPROPERTY(meta = (BindWidget)) class UWidget* DialogueContainer;
	UPROPERTY(meta = (BindWidget)) class USizeBox* SB_DialogueData;
	UPROPERTY(meta = (BindWidgetOptional)) class UImage* IMG_RightClickPrompt;

	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
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
	
	void ToggleAIGuide(class APlayerController* PC);
	
	virtual void NativeDestruct() override;
private:
	UFUNCTION()
	void HandleTutorialStepChanged(const FTutorialQuestStep& Step);

	UFUNCTION()
	void HandleTutorialDialogueLogged(const FString& QuestId, const FString& TriggerType, const TArray<FTutorialQuestDialogueLine>& Lines);

	void DisplayCurrentLine();
	void UpdateContinuePromptVisibility();
	void UpdateContinuePromptBlink(float InDeltaTime);
	void ApplyContinuePromptBrush();
	
	UFUNCTION()
	void HandleOnTextCommitted(const FText& Text, ETextCommit::Type CommitType);

	UFUNCTION()
	void HandleOnOperatorGuideResponse(const FString& RequestId, const FString& Agent, const FString& PayloadJson, const FString& RawMessage);

	UFUNCTION()
	void HandleOnOperatorGuideError(const FString& RequestId, const FString& Agent, const FString& ErrorCode, const FString& ErrorMessage, const FString& RawMessage);

	UFUNCTION()
	void HandleOnOperatorGuideProgress(const FString& RequestId, const FString& Agent, const FString& Stage, const FString& Message, const FString& RawMessage);

	UFUNCTION()
	void HandleOnMaterialGenerationResponse(const FFactoryMaterialGenerationResponse& Response);
	
	UPROPERTY()
	class UQuestManagerSubsystem* QuestSubsystem;

	TArray<FTutorialQuestDialogueLine> CachedLines;
	FString CachedTriggerType;
	bool bHasExternalDialogue = false;
	FString ExternalDialogueText;
	bool bShowRightClickPrompt = false;
	float RightClickPromptBlinkTime = 0.0f;
};
