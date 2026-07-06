#include "UI/UI_QuestWindow.h"
#include "Components/VerticalBox.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "UI_QuestNotify.h"
#include "Components/Border.h"
#include "Components/CheckBox.h"
#include "UIInteractDisplayHelpers.h"

namespace
{
    UDataTable* GetQuestRewardResourceTable()
    {
        static UDataTable* CachedResourceTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/DataTable/DT_ResourceData.DT_ResourceData"));
        return CachedResourceTable;
    }
}

namespace
{
    bool ShouldShowSubQuestZone(const FTutorialQuestStep& Step)
    {
        return Step.QuestId.StartsWith(TEXT("TUT_COMM_"));
    }

    FString BuildSubQuestProgressText(const FQuestState& Quest)
    {
        const bool bIsCompleted = Quest.Status == EQuestStatus::Completed;
        TArray<FString> ObjectiveTexts;
        for (const FQuestObjective& Objective : Quest.Objectives)
        {
            ObjectiveTexts.Add(FString::Printf(TEXT("%d / %d"), Objective.CurrentCount, Objective.Quantity));
        }

        const FString ProgressText = ObjectiveTexts.IsEmpty()
            ? TEXT("0 / 0")
            : FString::Join(ObjectiveTexts, TEXT(", "));

        if (bIsCompleted)
        {
            return FString::Printf(
                TEXT("<Daily>[일일]</>  <Completed>%s</>\n<Completed>진행도 : %s</>"),
                *Quest.Title.ToString(),
                *ProgressText
            );
        }

        return FString::Printf(
            TEXT("<Daily>[일일]</>  %s\n진행도 : %s"),
            *Quest.Title.ToString(),
            *ProgressText
        );
    }

    FString ResolveRewardDisplayName(const UObject* WorldContextObject, FName RewardItemId)
    {
        const FText ResourceDisplayText = UIInteractHelpers::GetResourceDisplayText(
            WorldContextObject,
            GetQuestRewardResourceTable(),
            RewardItemId);
        if (!ResourceDisplayText.IsEmpty() && ResourceDisplayText.ToString() != RewardItemId.ToString())
        {
            return ResourceDisplayText.ToString();
        }

        const UGameInstance* GameInstance = WorldContextObject ? WorldContextObject->GetWorld()->GetGameInstance() : nullptr;
        UMachineSubsystem* MachineSubsystem = GameInstance ? GameInstance->GetSubsystem<UMachineSubsystem>() : nullptr;
        const FText MachineDisplayText = UIInteractHelpers::GetMachineDisplayText(MachineSubsystem, RewardItemId);
        if (!MachineDisplayText.IsEmpty())
        {
            return MachineDisplayText.ToString();
        }

        return RewardItemId.ToString();
    }

    FString BuildRewardText(const UObject* WorldContextObject, const FQuestState& Quest)
    {
        TArray<FString> RewardTexts;
        for (const FQuestRewardItem& Reward : Quest.Rewards)
        {
            RewardTexts.Add(FString::Printf(TEXT("%s x%d"), *ResolveRewardDisplayName(WorldContextObject, Reward.ItemId), Reward.Quantity));
        }
        return RewardTexts.IsEmpty() ? TEXT("No reward") : FString::Join(RewardTexts, TEXT(", "));
    }

    FSlateColor GetMainQuestNormalDescColor()
    {
        return FSlateColor(FLinearColor::White);
    }

    FSlateColor GetMainQuestCompletedDescColor()
    {
        return FSlateColor(FLinearColor(0.55f, 0.55f, 0.55f, 1.0f));
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

void UUI_QuestWindow::OpenQuestWindow()
{
    if (!B_QuestBg || bIsQuestWindowOpen)
    {
        return;
    }

    B_QuestBg->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    bIsQuestWindowOpen = true;
    K2_PlayQuestAnimation(true);
}

void UUI_QuestWindow::UpdateMainQuestUI(const FQuestState& MainQuest)
{
    if (!TXT_MainQuestTitle || !TXT_MainQuestDesc)
    {
        return;
    }

    if (MainQuest.Status != EQuestStatus::Active)
    {
        if (TXT_MainQuestPrefix)
        {
            TXT_MainQuestPrefix->SetText(FText::FromString(TEXT("[메인]")));
            TXT_MainQuestPrefix->SetVisibility(ESlateVisibility::Hidden);
        }
        TXT_MainQuestTitle->SetText(FText::GetEmpty());
        TXT_MainQuestTitle->SetVisibility(ESlateVisibility::Hidden);
        TXT_MainQuestDesc->SetText(FText::FromString(TEXT("진행중인 메인퀘스트가 없습니다")));
        TXT_MainQuestDesc->SetColorAndOpacity(GetMainQuestCompletedDescColor());
        TXT_MainQuestDesc->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        return;
    }

    if (TXT_MainQuestPrefix)
    {
        TXT_MainQuestPrefix->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        TXT_MainQuestPrefix->SetText(FText::FromString(TEXT("[메인]")));
        TXT_MainQuestTitle->SetText(MainQuest.Title);
    }
    else
    {
        FText BracketedTitle = FText::Format(FText::FromString(TEXT("[메인] {0}")), MainQuest.Title);
        TXT_MainQuestTitle->SetText(BracketedTitle);
    }

    TXT_MainQuestTitle->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    TXT_MainQuestDesc->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    TXT_MainQuestDesc->SetColorAndOpacity(GetMainQuestNormalDescColor());
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
    NotifyWidget->PlayNotify(Quest.Title.ToString(), BuildRewardText(this, Quest));
}

void UUI_QuestWindow::UpdateSubQuestTexts(const TArray<FQuestState>& Quests)
{
    TArray<URichTextBlock*> SubBoxes = { TXT_SubQuest_1, TXT_SubQuest_2, TXT_SubQuest_3, TXT_SubQuest_4, TXT_SubQuest_5 };
    TArray<UCheckBox*> SubCheckBoxes = { CB_SubQuest_1, CB_SubQuest_2, CB_SubQuest_3, CB_SubQuest_4, CB_SubQuest_5 };

    for (int32 i = 0; i < SubBoxes.Num(); ++i)
    {
        URichTextBlock* Box = SubBoxes[i];
        UCheckBox* CheckBox = SubCheckBoxes[i];
        if (!Box) continue;

        UWidget* RowWrapper = Box->GetParent();

        if (i < Quests.Num())
        {
            Box->SetText(FText::FromString(BuildSubQuestProgressText(Quests[i])));
            
            // 체크박스
            if (CheckBox)
            {
                // 해당 퀘스트의 Status 상태가 Completed라면 Checked, 아니라면 Unchecked 
                ECheckBoxState NewState = (Quests[i].Status == EQuestStatus::Completed) 
                    ? ECheckBoxState::Checked 
                    : ECheckBoxState::Unchecked;
                
                CheckBox->SetCheckedState(NewState);
            }
            
            if (RowWrapper)
            {
                RowWrapper->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
            }
        }
        else
        {
            if (CheckBox)
            {
                CheckBox->SetCheckedState(ECheckBoxState::Unchecked);
            }

            if (RowWrapper)
            {
                RowWrapper->SetVisibility(ESlateVisibility::Collapsed);
            }
        }
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

    if (TXT_MainQuestPrefix)
    {
        TXT_MainQuestPrefix->SetText(FText::FromString(TEXT("[메인]")));
        TXT_MainQuestTitle->SetText(FText::FromString(Step.Title));
    }
    else
    {
        FText BracketedTitle = FText::Format(FText::FromString(TEXT("[메인] {0}")), FText::FromString(Step.Title));
        TXT_MainQuestTitle->SetText(BracketedTitle);
    }
    
    TXT_MainQuestDesc->SetColorAndOpacity(GetMainQuestNormalDescColor());
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

    // 2. 메인 퀘스트 진행 여부 판정 (서브시스템 연동 완료)
    FQuestState MainQuest;
    // GetCurrentMainQuest가 false를 리턴하면 다음 메인 퀘스트가 없다는 의미(올 클리어)입니다.
    const bool bIsMainQuestActive = QuestManager->GetCurrentMainQuest(MainQuest);

    // 튜토리얼이 진행 중이거나, 유효한 메인 퀘스트가 남아있을 때만 데이터를 노출합니다.
    const bool bShouldShowMainContents = bHasTutorialStep || bIsMainQuestActive;

    // 메인 퀘스트 영역(Zone) 자체는 여백을 채우기 위해 항상 보임 상태로 유지합니다.
    VB_MainQuestZone->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    if (bShouldShowMainContents)
    {
        if (TXT_MainQuestPrefix) TXT_MainQuestPrefix->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        if (TXT_MainQuestTitle) TXT_MainQuestTitle->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        if (TXT_MainQuestDesc)  TXT_MainQuestDesc->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }
    else
    {
        // 메인 퀘스트가 완전히 끝났을 때도 영역은 유지하고 빈 상태 문구를 노출합니다.
        if (TXT_MainQuestPrefix)
        {
            TXT_MainQuestPrefix->SetText(FText::FromString(TEXT("[메인]")));
            TXT_MainQuestPrefix->SetVisibility(ESlateVisibility::Hidden);
        }
        if (TXT_MainQuestTitle) 
        {
            TXT_MainQuestTitle->SetText(FText::GetEmpty());
            TXT_MainQuestTitle->SetVisibility(ESlateVisibility::Hidden);
        }
        if (TXT_MainQuestDesc)  
        {
            TXT_MainQuestDesc->SetText(FText::FromString(TEXT("진행중인 메인퀘스트가 없습니다")));
            TXT_MainQuestDesc->SetColorAndOpacity(GetMainQuestCompletedDescColor());
            TXT_MainQuestDesc->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        }
    }

    // 서브 퀘스트 영역 표시 처리
    VB_SubQuestZone->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
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
