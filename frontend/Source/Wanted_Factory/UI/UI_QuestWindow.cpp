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

FString BuildRewardText(const FQuestState& Quest)
{
    TArray<FString> RewardTexts;
    for (const FQuestRewardItem& Reward : Quest.Rewards)
    {
        RewardTexts.Add(FString::Printf(TEXT("%s x%d"), *Reward.ItemId.ToString(), Reward.Quantity));
    }

    return RewardTexts.IsEmpty()
        ? TEXT("No reward")
        : FString::Join(RewardTexts, TEXT(", "));
}
}

void UUI_QuestWindow::NativeConstruct()
{
    Super::NativeConstruct();

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

    TArray<FQuestState> CurrentSubQuests;
    QuestManager->GetSubQuests(CurrentSubQuests);
    CacheSubQuestStatuses(CurrentSubQuests);
    UpdateSubQuestTexts(CurrentSubQuests);

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
    CacheSubQuestStatuses(Quests);
    UpdateSubQuestTexts(Quests);
}

void UUI_QuestWindow::HandleOnSubQuestsUpdated(const TArray<FQuestState>& Quests)
{
    for (const FQuestState& Quest : Quests)
    {
        const EQuestStatus* PreviousStatus = CachedSubQuestStatuses.Find(Quest.QuestId);
        if (PreviousStatus && *PreviousStatus != EQuestStatus::Completed && Quest.Status == EQuestStatus::Completed)
        {
            ShowSubQuestCompletedNotify(Quest);
        }
    }

    CacheSubQuestStatuses(Quests);
    UpdateSubQuestTexts(Quests);
}

void UUI_QuestWindow::CacheSubQuestStatuses(const TArray<FQuestState>& Quests)
{
    CachedSubQuestStatuses.Empty();

    for (const FQuestState& Quest : Quests)
    {
        CachedSubQuestStatuses.Add(Quest.QuestId, Quest.Status);
    }
}

void UUI_QuestWindow::ShowSubQuestCompletedNotify(const FQuestState& Quest)
{
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
    NotifyWidget->PlayNotify(Quest.Title.ToString(), BuildRewardText(Quest));
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
    if (!VB_MainQuestZone || !VB_SubQuestZone) return;

    UGameInstance* GI = GetGameInstance();
    UQuestManagerSubsystem* QuestManager = GI ? GI->GetSubsystem<UQuestManagerSubsystem>() : nullptr;
    if (!QuestManager) return;

    // 1. 튜토리얼 스텝 존재 여부 체크
    FTutorialQuestStep CurrentStep;
    const bool bHasTutorialStep = QuestManager->GetCurrentTutorialQuestStep(CurrentStep);

    //메인 퀘스트가 완전히 끝났는지 상태 판정식 결합
    bool bIsMainQuestActive = false;
    FQuestState MainQuest;

    // 튜토리얼이나 진행 중인 메인 퀘스트가 "진행 중"일 때만 노출을 허용합니다.
    const bool bShouldShowMainZone = bHasTutorialStep || bIsMainQuestActive;

    if (bShouldShowMainZone)
    {
        VB_MainQuestZone->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }
    else
    {
        // 서브퀘스트가 위로 밀리지 않습니다.
        VB_MainQuestZone->SetVisibility(ESlateVisibility::Hidden);

        if (TXT_MainQuestTitle) TXT_MainQuestTitle->SetText(FText::GetEmpty());
        if (TXT_MainQuestDesc)  TXT_MainQuestDesc->SetText(FText::GetEmpty());
    }
    // 서브 퀘스트 영역 최종 표시 처리 호출
    UpdateSubQuestZoneVisibility();
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

void UUI_QuestWindow::CloseQuestWindow()
{
    // 현재 켜져있는 상태라면 기존 Toggle 로직을 실행시켜 
    // bIsQuestWindowOpen = false 처리와 함께 K2_PlayQuestAnimation(false)를 구동
    if (bIsQuestWindowOpen)
    {
        ToggleQuestWindow();
    }
}
