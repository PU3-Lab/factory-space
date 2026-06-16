#include "UI/UI_QuestWindow.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "UI_QuestNotify.h"

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
}

void UUI_QuestWindow::ToggleQuestWindow()
{
    if (!VB_QuestLayout)
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
        VB_QuestLayout->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
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
        TXT_MainQuestTitle->SetText(FText::FromString(TEXT("진행 중인 메인 미션 없음")));
        TXT_MainQuestDesc->SetText(FText::GetEmpty());
        return;
    }

    TXT_MainQuestTitle->SetText(MainQuest.Title);
    TXT_MainQuestDesc->SetText(MainQuest.Description);
}

void UUI_QuestWindow::HandleOnMainQuestChanged(const FQuestState& NewQuest)
{
    UpdateMainQuestUI(NewQuest);

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
    NotifyWidget->PlayNotify(NewQuest.Title.ToString(), TEXT("새로운 메인 미션 해제"));
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

    if (TXT_SubQuest_1) TXT_SubQuest_1->SetText(FText::FromString(TEXT("AI 응답 대기 중...")));
    if (TXT_SubQuest_2) TXT_SubQuest_2->SetText(FText::GetEmpty());
    if (TXT_SubQuest_3) TXT_SubQuest_3->SetText(FText::GetEmpty());
    if (TXT_SubQuest_4) TXT_SubQuest_4->SetText(FText::GetEmpty());
    if (TXT_SubQuest_5) TXT_SubQuest_5->SetText(FText::GetEmpty());

    QuestManager->ConnectQuestAgent();
    QuestManager->RequestSubQuests();
}

void UUI_QuestWindow::HandleOnSubQuestsGenerated(const FString& RequestId, const TArray<FQuestState>& Quests)
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

        const FString StatusIndicator = (Quests[i].Status == EQuestStatus::Completed) ? TEXT(" [완료]") : TEXT(" [진행 중]");
        const FString FormattedLine = FString::Printf(TEXT("- %s%s"), *Quests[i].Title.ToString(), *StatusIndicator);
        SubBoxes[i]->SetText(FText::FromString(FormattedLine));
    }
}

void UUI_QuestWindow::HandleOnSubQuestRequestFailed(const FString& RequestId, const FString& ErrorMessage)
{
    if (TXT_SubQuest_1)
    {
        TXT_SubQuest_1->SetText(FText::FromString(TEXT("서브 퀘스트 요청 실패")));
    }
}

void UUI_QuestWindow::DisplayTutorialStep(const FTutorialQuestStep& Step)
{
    if (!TXT_MainQuestTitle || !TXT_MainQuestDesc)
    {
        return;
    }

    TXT_MainQuestTitle->SetText(FText::FromString(Step.Title));
    TXT_MainQuestDesc->SetText(FText::FromString(Step.Description));
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
        }
        return;
    }

    if (TriggerType == TEXT("on_start") && QuestManager->GetCurrentTutorialQuestStep(Step))
    {
        DisplayTutorialStep(Step);
    }
}
