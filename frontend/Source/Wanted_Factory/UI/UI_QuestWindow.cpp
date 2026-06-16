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
    if (GI)
    {
        UQuestManagerSubsystem* QuestManager = GI->GetSubsystem<UQuestManagerSubsystem>();
        if (QuestManager)
        {
            // 기존의 메인/서브 퀘스트 이벤트 라인 유지
            QuestManager->OnSubQuestsGenerated.AddDynamic(this, &UUI_QuestWindow::HandleOnSubQuestsGenerated);
            QuestManager->OnSubQuestRequestFailed.AddDynamic(this, &UUI_QuestWindow::HandleOnSubQuestRequestFailed);
            QuestManager->OnMainQuestChanged.AddDynamic(this, &UUI_QuestWindow::HandleOnMainQuestChanged);

            //튜토리얼 스텝 변동 알림 연결
            QuestManager->OnTutorialStepChanged.AddDynamic(this, &UUI_QuestWindow::HandleOnTutorialStepChanged);
            
            // 게임 시작 시 이미 켜져 있는 첫 튜토리얼 단계("이동하기")가 있다면 즉시 강제 렌더링
            FTutorialQuestStep InitialStep;
            if (QuestManager->GetCurrentTutorialQuestStep(InitialStep))
            {
                HandleOnTutorialStepChanged(InitialStep);
            }
        }
    }
}

void UUI_QuestWindow::ToggleQuestWindow()
{
    if (!VB_QuestLayout) return;

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
    if (!TXT_MainQuestTitle || !TXT_MainQuestDesc) return;

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
    if (!GI) return;

    UQuestManagerSubsystem* QM = GI->GetSubsystem<UQuestManagerSubsystem>();
    if (!QM || QM->GetCurrentMainQuestIndex() == 0) return; // 0번 기습 생성 방지 가드 보존

    if (QuestNotifyWidgetClass)
    {
        APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
        if (PC)
        {
            UUI_QuestNotify* NotifyWidget = CreateWidget<UUI_QuestNotify>(PC, QuestNotifyWidgetClass);
            if (NotifyWidget)
            {
                NotifyWidget->AddToViewport(100);
                NotifyWidget->PlayNotify(NewQuest.Title.ToString(), TEXT("새로운 메인 미션 해제"));
            }
        }
    }
}

void UUI_QuestWindow::OnRequestQuestsClicked()
{
    UGameInstance* GI = GetGameInstance();
    if (GI)
    {
        UQuestManagerSubsystem* QuestManager = GI->GetSubsystem<UQuestManagerSubsystem>();
        if (QuestManager)
        {
            if (TXT_SubQuest_1) TXT_SubQuest_1->SetText(FText::FromString(TEXT("AI 응답 대기 중...")));
            if (TXT_SubQuest_2) TXT_SubQuest_2->SetText(FText::GetEmpty());
            if (TXT_SubQuest_3) TXT_SubQuest_3->SetText(FText::GetEmpty());
            if (TXT_SubQuest_4) TXT_SubQuest_4->SetText(FText::GetEmpty());
            if (TXT_SubQuest_5) TXT_SubQuest_5->SetText(FText::GetEmpty());

            QuestManager->ConnectQuestAgent();
            QuestManager->RequestSubQuests();
        }
    }
}

void UUI_QuestWindow::HandleOnSubQuestsGenerated(const FString& RequestId, const TArray<FQuestState>& Quests)
{
    TArray<UTextBlock*> SubBoxes = { TXT_SubQuest_1, TXT_SubQuest_2, TXT_SubQuest_3, TXT_SubQuest_4, TXT_SubQuest_5 };

    for (UTextBlock* Box : SubBoxes)
    {
        if (Box) Box->SetText(FText::GetEmpty());
    }

    for (int32 i = 0; i < Quests.Num(); ++i)
    {
        if (!SubBoxes.IsValidIndex(i) || !SubBoxes[i]) continue;

        FString StatusIndicator = (Quests[i].Status == EQuestStatus::Completed) ? TEXT(" [완료]") : TEXT(" [진행 중]");
        FString FormattedLine = FString::Printf(TEXT("• %s%s"), *Quests[i].Title.ToString(), *StatusIndicator);
        SubBoxes[i]->SetText(FText::FromString(FormattedLine));
    }
}

void UUI_QuestWindow::HandleOnSubQuestRequestFailed(const FString& RequestId, const FString& ErrorMessage)
{
    if (TXT_SubQuest_1) TXT_SubQuest_1->SetText(FText::FromString(TEXT("서버 연결 실패")));
}

void UUI_QuestWindow::HandleOnTutorialStepChanged(const FTutorialQuestStep& NewStep)
{
    if (!TXT_MainQuestTitle || !TXT_MainQuestDesc) return;

    // csv 파일 컬럼인 title("이동하기")과 description("W, A, S, D를 눌러...")을 전송받아 화면에 박제
    TXT_MainQuestTitle->SetText(FText::FromString(NewStep.Title));
    TXT_MainQuestDesc->SetText(FText::FromString(NewStep.Description));
}