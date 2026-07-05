#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FactoryAgentClientSubsystem.h"
#include "QuestManagerSubsystem.h"
#include "UI_DialogueBalloon.generated.h"

class UBorder;
class AMachineBase;
class FJsonObject;

enum class ETrackedProcessOptimizerIssueType : uint8
{
	Unknown,
	Durability,
	Power
};

struct FTrackedProcessOptimizerIssue
{
	FString TargetId;
	ETrackedProcessOptimizerIssueType IssueType = ETrackedProcessOptimizerIssueType::Unknown;
	TWeakObjectPtr<AMachineBase> Machine;
	float InitialDurability = 0.0f;
};

UCLASS()
class WANTED_FACTORY_API UUI_DialogueBalloon : public UUserWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_Dialogue;
	UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_Question;
	UPROPERTY(meta = (BindWidget)) class UWidget* DialogueContainer;
	UPROPERTY(meta = (BindWidget)) class USizeBox* SB_DialogueData;
	UPROPERTY(meta = (BindWidgetOptional)) class UImage* IMG_RightClickPrompt;
	UPROPERTY(meta = (BindWidget)) UBorder* B_OperatorInput;

	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

public:
	UFUNCTION(BlueprintCallable, Category = "Quest|UI")
	void RefreshDialogueUI();

	UFUNCTION(BlueprintCallable, Category = "Quest|UI")
	void ShowExternalDialogue(const FString& DialogueText);

	UFUNCTION(BlueprintCallable, Category = "Quest|UI")
	void ClearExternalDialogue();

	UFUNCTION(BlueprintCallable, Category = "Quest|UI")
	void ResetProcessOptimizerTracking();

	UFUNCTION(BlueprintCallable, Category = "Quest|UI")
	void BeginProcessOptimizerRequest();
	
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

	UFUNCTION()
	void HandleOnProcessOptimizerResponse(const FString& RequestId, const FString& Agent, const FString& PayloadJson, const FString& RawMessage);

	void SetTrackedProcessOptimizerIssues(const TArray<FTrackedProcessOptimizerIssue>& NewIssues);
	void UpdateTrackedProcessOptimizerIssues();
	void HighlightOperatorGuideTargets(const TSharedPtr<FJsonObject>& PayloadObject, const FString& Answer);
	void ClearOperatorGuideHighlights();
	bool IsTrackedProcessOptimizerIssueResolved(const FTrackedProcessOptimizerIssue& Issue) const;
	ETrackedProcessOptimizerIssueType ClassifyProcessOptimizerIssue(const TSharedPtr<FJsonObject>& SuggestionObject) const;
	
	UPROPERTY()
	class UQuestManagerSubsystem* QuestSubsystem;

	TArray<FTutorialQuestDialogueLine> CachedLines;
	FString CachedTriggerType;
	bool bHasExternalDialogue = false;
	FString ExternalDialogueText;
	FString LastSubmittedOperatorGuideQuestion;
	bool bShowLastSubmittedOperatorGuideQuestion = false;
	bool bShowRightClickPrompt = false;
	float RightClickPromptBlinkTime = 0.0f;
	TArray<FTrackedProcessOptimizerIssue> TrackedProcessOptimizerIssues;
	TArray<TWeakObjectPtr<AMachineBase>> OperatorGuideHighlightedMachines;
	bool bProcessOptimizerRequestInFlight = false;
	bool bCanAnnounceProcessOptimizerResolution = false;
};
