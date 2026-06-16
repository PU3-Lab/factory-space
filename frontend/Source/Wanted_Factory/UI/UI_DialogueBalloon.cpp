#include "UI/UI_DialogueBalloon.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/GameInstance.h"
#include "Input/Reply.h"

void UUI_DialogueBalloon::NativeConstruct()
{
    Super::NativeConstruct();

    SetIsFocusable(true);
    SetVisibility(ESlateVisibility::Visible);

    if (UGameInstance* GI = GetGameInstance())
    {
        QuestSubsystem = GI->GetSubsystem<UQuestManagerSubsystem>();
    }

    if (QuestSubsystem)
    {
        QuestSubsystem->OnTutorialStepChanged.AddDynamic(this, &UUI_DialogueBalloon::HandleTutorialStepChanged);
        QuestSubsystem->OnTutorialDialogueLogged.AddDynamic(this, &UUI_DialogueBalloon::HandleTutorialDialogueLogged);
    }

    RefreshDialogueUI();
}

void UUI_DialogueBalloon::RefreshDialogueUI()
{
    CachedLines.Empty();

    if (!QuestSubsystem)
    {
        DisplayCurrentLine();
        return;
    }

    if (QuestSubsystem->HasPendingTutorialStartDialogue())
    {
        FString LoggedQuestId;
        FString LoggedTriggerType;
        QuestSubsystem->GetLastTutorialDialogueLog(LoggedQuestId, LoggedTriggerType, CachedLines);
        DisplayCurrentLine();
        return;
    }

    FTutorialQuestStep CurrentStep;
    if (!QuestSubsystem->GetCurrentTutorialQuestStep(CurrentStep))
    {
        DisplayCurrentLine();
        return;
    }

    QuestSubsystem->GetTutorialDialogueLines(CurrentStep.QuestId, TEXT("on_start"), CachedLines);
    DisplayCurrentLine();
}

void UUI_DialogueBalloon::HandleTutorialStepChanged(const FTutorialQuestStep& Step)
{
    if (QuestSubsystem && QuestSubsystem->HasPendingTutorialStartDialogue())
    {
        return;
    }

    RefreshDialogueUI();
}

void UUI_DialogueBalloon::HandleTutorialDialogueLogged(
    const FString& QuestId,
    const FString& TriggerType,
    const TArray<FTutorialQuestDialogueLine>& Lines)
{
    if (TriggerType == TEXT("on_complete"))
    {
        CachedLines = Lines;
        DisplayCurrentLine();
        return;
    }

    if (TriggerType != TEXT("on_start"))
    {
        return;
    }

    CachedLines = Lines;
    DisplayCurrentLine();
}

void UUI_DialogueBalloon::DisplayCurrentLine()
{
    if (CachedLines.IsEmpty())
    {
        if (DialogueContainer)
        {
            DialogueContainer->SetVisibility(ESlateVisibility::Collapsed);
        }

        if (TXT_Dialogue)
        {
            TXT_Dialogue->SetText(FText::GetEmpty());
        }

        return;
    }

    if (DialogueContainer)
    {
        DialogueContainer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }

    FString CombinedDialogue;
    for (int32 LineIndex = 0; LineIndex < CachedLines.Num(); ++LineIndex)
    {
        if (!CombinedDialogue.IsEmpty())
        {
            CombinedDialogue += TEXT("\n");
        }

        CombinedDialogue += CachedLines[LineIndex].Dialogue;
    }

    if (TXT_Dialogue)
    {
        TXT_Dialogue->SetText(FText::FromString(CombinedDialogue));
    }
}

FReply UUI_DialogueBalloon::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    FReply Reply = Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);

    if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    {
        return Reply;
    }

    if (!QuestSubsystem || !QuestSubsystem->HasPendingTutorialStartDialogue())
    {
        return Reply;
    }

    QuestSubsystem->RevealPendingTutorialStartDialogue();
    return FReply::Handled();
}
