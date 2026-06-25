#include "UI/UI_QuestWindow.h"
#include "Components/VerticalBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "UI_QuestNotify.h"
#include "Components/Border.h"

namespace
{
bool ShouldShowSubQuestZone(const FTutorialQuestStep& Step)
{
    return Step.QuestId.StartsWith(TEXT("TUT_COMM_"))
        || Step.QuestId.StartsWith(TEXT("TUT_SIGNAL_"));
}

FString BuildSubQuestProgressText(const FQuestState& Quest)
{
    TArray<FString> ObjectiveTexts;
    for (const FQuestObjective& Objective : Quest.Objectives)
    {
        ObjectiveTexts.Add(FString::Printf(TEXT("%d/%d"), Objective.CurrentCount, Objective.Quantity));
    }

    const FString ProgressText = ObjectiveTexts.IsEmpty()
        ? TEXT("0/0")
        : FString::Join(ObjectiveTexts, TEXT(", "));
    const FString StatusText = Quest.Status == EQuestStatus::Completed
        ? TEXT("[Completed]")
        : TEXT("[In Progress]");

    return FString::Printf(TEXT("- %s (%s) %s"), *Quest.Title.ToString(), *ProgressText, *StatusText);
}
}

void UUI_QuestWindow::NativeConstruct()
{
    Super::NativeConstruct();

    if (BTN_RequestQuests)
    {
        BTN_RequestQuests->OnClicked.AddDynamic(this, &UUI_QuestWindow::OnRequestQuestsClicked);
    }

    UGameInstance* GI = GetGameInstance();
    if (!GI)
    {
        return;
    }

    UQuestManagerSubsystem* QuestManager = GI->GetSubsystem<UQuestManagerSubsystem>();
    if (!QuestManager)
    {
        return;
    }

    QuestManager->OnSubQuestsGenerated.AddDynamic(this, &UUI_QuestWindow::HandleOnSubQuestsGenerated);
    QuestManager->OnSubQuestsUpdated.AddDynamic(this, &UUI_QuestWindow::HandleOnSubQuestsUpdated);
    QuestManager->OnSubQuestRequestFailed.AddDynamic(this, &UUI_QuestWindow::HandleOnSubQuestRequestFailed);
    QuestManager->OnMainQuestChanged.AddDynamic(this, &UUI_QuestWindow::HandleOnMainQuestChanged);
    QuestManager->OnTutorialStepChanged.AddDynamic(this, &UUI_QuestWindow::HandleOnTutorialStepChanged);
    QuestManager->OnTutorialDialogueLogged.AddDynamic(this, &UUI_QuestWindow::HandleOnTutorialDialogueLogged);

    FTutorialQuestStep InitialStep;
    if (QuestManager->HasPendingTutorialStartDialogue())
    {
        FString LoggedQuestId;
        FString LoggedTriggerType;
        TArray<FTutorialQuestDialogueLine> LoggedLines;
        QuestManager->GetLastTutorialDialogueLog(LoggedQuestId, LoggedTriggerType, LoggedLines);

        if (LoggedTriggerType == TEXT("on_complete") && QuestManager->GetTutorialQuestStepById(LoggedQuestId, InitialStep))
        {
            DisplayTutorialStep(InitialStep);
        }
    }
    else if (QuestManager->GetCurrentTutorialQuestStep(InitialStep))
    {
        DisplayTutorialStep(InitialStep);
    }

    UpdateQuestZoneVisibility();
    UpdateSubQuestZoneVisibility();
}

void UUI_QuestWindow::ToggleQuestWindow()
{
    if (!B_QuestBg)
    {
        return;
    }

    if (bIsQuestWindowOpen)
    {
        bIsQuestWindowOpen = false;
        K2_PlayQuestAnimation(false);
    }
    else
    {
        B_QuestBg->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        bIsQuestWindowOpen = true;
        K2_PlayQuestAnimation(true);
    }
}

void UUI_QuestWindow::UpdateMainQuestUI(const FQuestState& MainQuest)
{
    if (!TXT_MainQuestTitle || !TXT_MainQuestDesc)
    {
        return;
    }

    if (MainQuest.Status != EQuestStatus::Active)
    {
        TXT_MainQuestTitle->SetText(FText::FromString(TEXT("No active main quest")));
        TXT_MainQuestDesc->SetText(FText::GetEmpty());
        return;
    }

    TXT_MainQuestTitle->SetText(MainQuest.Title);
    TXT_MainQuestDesc->SetText(MainQuest.Description);
}

void UUI_QuestWindow::HandleOnMainQuestChanged(const FQuestState& NewQuest)
{
    UpdateMainQuestUI(NewQuest);
    UpdateSubQuestZoneVisibility();

    UGameInstance* GI = GetGameInstance();
    if (!GI)
    {
        return;
    }

    UQuestManagerSubsystem* QM = GI->GetSubsystem<UQuestManagerSubsystem>();
    if (!QM || QM->GetCurrentMainQuestIndex() == 0)
    {
        return;
    }

    if (!QuestNotifyWidgetClass)
    {
        return;
    }

    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC)
    {
        return;
    }

    UUI_QuestNotify* NotifyWidget = CreateWidget<UUI_QuestNotify>(PC, QuestNotifyWidgetClass);
    if (!NotifyWidget)
    {
        return;
    }

    NotifyWidget->AddToViewport(100);
    NotifyWidget->PlayNotify(NewQuest.Title.ToString(), TEXT("New main quest unlocked"));
}

void UUI_QuestWindow::OnRequestQuestsClicked()
{
    UGameInstance* GI = GetGameInstance();
    if (!GI)
    {
        return;
    }

    UQuestManagerSubsystem* QuestManager = GI->GetSubsystem<UQuestManagerSubsystem>();
    if (!QuestManager)
    {
        return;
    }

    if (TXT_SubQuest_1) TXT_SubQuest_1->SetText(FText::FromString(TEXT("Waiting for AI response...")));
    if (TXT_SubQuest_2) TXT_SubQuest_2->SetText(FText::GetEmpty());
    if (TXT_SubQuest_3) TXT_SubQuest_3->SetText(FText::GetEmpty());
    if (TXT_SubQuest_4) TXT_SubQuest_4->SetText(FText::GetEmpty());
    if (TXT_SubQuest_5) TXT_SubQuest_5->SetText(FText::GetEmpty());

    QuestManager->ConnectQuestAgent();
    QuestManager->RequestSubQuests();
}

void UUI_QuestWindow::HandleOnSubQuestsGenerated(const FString& RequestId, const TArray<FQuestState>& Quests)
{
    UpdateSubQuestTexts(Quests);
}

void UUI_QuestWindow::HandleOnSubQuestsUpdated(const TArray<FQuestState>& Quests)
{
    UpdateSubQuestTexts(Quests);
}

void UUI_QuestWindow::UpdateSubQuestTexts(const TArray<FQuestState>& Quests)
{
    TArray<UTextBlock*> SubBoxes = { TXT_SubQuest_1, TXT_SubQuest_2, TXT_SubQuest_3, TXT_SubQuest_4, TXT_SubQuest_5 };

    for (UTextBlock* Box : SubBoxes)
    {
        if (Box)
        {
            Box->SetText(FText::GetEmpty());
        }
    }

    for (int32 i = 0; i < Quests.Num(); ++i)
    {
        if (!SubBoxes.IsValidIndex(i) || !SubBoxes[i])
        {
            continue;
        }

        SubBoxes[i]->SetText(FText::FromString(BuildSubQuestProgressText(Quests[i])));
    }
}

void UUI_QuestWindow::HandleOnSubQuestRequestFailed(const FString& RequestId, const FString& ErrorMessage)
{
    if (TXT_SubQuest_1)
    {
        TXT_SubQuest_1->SetText(FText::FromString(TEXT("Sub quest request failed")));
    }
}

void UUI_QuestWindow::DisplayTutorialStep(const FTutorialQuestStep& Step)
{
    if (!TXT_MainQuestTitle || !TXT_MainQuestDesc)
    {
        return;
    }

    UGameInstance* GI = GetGameInstance();
    UQuestManagerSubsystem* QuestManager = GI ? GI->GetSubsystem<UQuestManagerSubsystem>() : nullptr;

    TXT_MainQuestTitle->SetText(FText::FromString(Step.Title));
    TXT_MainQuestDesc->SetText(FText::FromString(
        QuestManager
            ? QuestManager->GetTutorialStepDisplayDescription(Step)
            : Step.Description));
}

void UUI_QuestWindow::UpdateQuestZoneVisibility()
{
    if (!VB_MainQuestZone || !VB_SubQuestZone)
    {
        return;
    }

    UGameInstance* GI = GetGameInstance();
    UQuestManagerSubsystem* QuestManager = GI ? GI->GetSubsystem<UQuestManagerSubsystem>() : nullptr;
    if (!QuestManager)
    {
        VB_MainQuestZone->SetVisibility(ESlateVisibility::Collapsed);
        VB_SubQuestZone->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    FTutorialQuestStep CurrentStep;
    const bool bHasTutorialStep = QuestManager->GetCurrentTutorialQuestStep(CurrentStep);
    const ESlateVisibility MainQuestVisibility = bHasTutorialStep
        ? ESlateVisibility::SelfHitTestInvisible
        : ESlateVisibility::Collapsed;

    VB_MainQuestZone->SetVisibility(MainQuestVisibility);

    if (!bHasTutorialStep)
    {
        if (TXT_MainQuestTitle)
        {
            TXT_MainQuestTitle->SetText(FText::GetEmpty());
        }

        if (TXT_MainQuestDesc)
        {
            TXT_MainQuestDesc->SetText(FText::GetEmpty());
        }

        VB_SubQuestZone->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }
}

void UUI_QuestWindow::UpdateSubQuestZoneVisibility()
{
    if (!VB_SubQuestZone)
    {
        return;
    }

    UGameInstance* GI = GetGameInstance();
    UQuestManagerSubsystem* QuestManager = GI ? GI->GetSubsystem<UQuestManagerSubsystem>() : nullptr;
    if (!QuestManager)
    {
        VB_SubQuestZone->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    FTutorialQuestStep CurrentStep;
    if (!QuestManager->GetCurrentTutorialQuestStep(CurrentStep))
    {
        VB_SubQuestZone->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        return;
    }

    const ESlateVisibility NextVisibility = ShouldShowSubQuestZone(CurrentStep)
        ? ESlateVisibility::SelfHitTestInvisible
        : ESlateVisibility::Collapsed;
    VB_SubQuestZone->SetVisibility(NextVisibility);
}

void UUI_QuestWindow::HandleOnTutorialStepChanged(const FTutorialQuestStep& NewStep)
{
    UGameInstance* GI = GetGameInstance();
    UQuestManagerSubsystem* QuestManager = GI ? GI->GetSubsystem<UQuestManagerSubsystem>() : nullptr;
    if (QuestManager && QuestManager->HasPendingTutorialStartDialogue())
    {
        return;
    }

    DisplayTutorialStep(NewStep);
    UpdateQuestZoneVisibility();
    UpdateSubQuestZoneVisibility();
}

void UUI_QuestWindow::HandleOnTutorialDialogueLogged(
    const FString& QuestId,
    const FString& TriggerType,
    const TArray<FTutorialQuestDialogueLine>& Lines)
{
    UGameInstance* GI = GetGameInstance();
    UQuestManagerSubsystem* QuestManager = GI ? GI->GetSubsystem<UQuestManagerSubsystem>() : nullptr;
    if (!QuestManager)
    {
        return;
    }

    FTutorialQuestStep Step;
    if (TriggerType == TEXT("on_complete"))
    {
        if (QuestManager->GetTutorialQuestStepById(QuestId, Step))
        {
            DisplayTutorialStep(Step);
            UpdateQuestZoneVisibility();
            UpdateSubQuestZoneVisibility();
        }
        return;
    }

    if (TriggerType == TEXT("on_start") && QuestManager->GetCurrentTutorialQuestStep(Step))
    {
        DisplayTutorialStep(Step);
        UpdateQuestZoneVisibility();
        UpdateSubQuestZoneVisibility();
        return;
    }

    UpdateQuestZoneVisibility();
    UpdateSubQuestZoneVisibility();
}
