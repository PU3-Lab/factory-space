#include "UI_MainHUD.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "QuestManagerSubsystem.h"
#include "PlanetEventManagerSubsystem.h"
#include "FactoryAgentClientSubsystem.h" 
#include "FactoryAgentJsonUtils.h"       
#include "Dom/JsonObject.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "GameFramework/PlayerController.h"
#include "UI_QuestNotify.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/UserWidget.h"
#include "UMG.h"
#include "Kismet/GameplayStatics.h"

void UUI_MainHUD::NativeConstruct()
{
    Super::NativeConstruct();
    
    if (BTN_RequestQuests)
    {
        BTN_RequestQuests->OnClicked.AddDynamic(this, &UUI_MainHUD::OnRequestQuestsClicked);
    }

    if (ET_OperatorInput)
    {
        ET_OperatorInput->OnTextCommitted.AddDynamic(this, &UUI_MainHUD::HandleOnTextCommitted);
    }

    if (BTN_ToggleGuide)
    {
        BTN_ToggleGuide->OnClicked.AddDynamic(this, &UUI_MainHUD::OnToggleGuideClicked);
    }

    UGameInstance* GameInstance = GetGameInstance();
    if (GameInstance)
    {
        UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>();
        if (QuestManager)
        {
            QuestManager->OnSubQuestsGenerated.AddDynamic(this, &UUI_MainHUD::HandleOnSubQuestsGenerated);
            QuestManager->OnSubQuestRequestFailed.AddDynamic(this, &UUI_MainHUD::HandleOnSubQuestRequestFailed);
            
            QuestManager->OnMainQuestChanged.AddDynamic(this, &UUI_MainHUD::HandleOnMainQuestChanged);
        }

        UFactoryAgentClientSubsystem* AgentClient = GameInstance->GetSubsystem<UFactoryAgentClientSubsystem>();
        if (AgentClient)
        {
            AgentClient->OnAgentResponseReceived.AddDynamic(this, &UUI_MainHUD::HandleOnOperatorGuideResponse);
            AgentClient->OnAgentErrorReceived.AddDynamic(this, &UUI_MainHUD::HandleOnOperatorGuideError);
        }
    }
}

void UUI_MainHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (GetWorld())
    {
        UPlanetEventManagerSubsystem* PlanetManager = GetWorld()->GetSubsystem<UPlanetEventManagerSubsystem>();
        if (PlanetManager)
        {
            if (TXT_DisasterDay)
            {
                int32 CurrentDay = PlanetManager->GetCurrentDayIndex();
                TXT_DisasterDay->SetText(FText::FromString(FString::Printf(TEXT("DAY %02d"), CurrentDay)));
            }

            if (TXT_InGameTime)
            {
                TXT_InGameTime->SetText(FText::FromString(PlanetManager->GetCurrentTime24String()));
            }
        }
    }
}

void UUI_MainHUD::OnToggleGuideClicked()
{
    ToggleAIGuideWindow();
}

void UUI_MainHUD::ToggleAIGuideWindow()
{
    if (!B_ChatBackground) return;
    
    APlayerController* PC = GetOwningPlayer();

    if (B_ChatBackground->GetVisibility() == ESlateVisibility::Collapsed)
    {
        B_ChatBackground->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        
        if (TXT_ToggleText)
        {
            TXT_ToggleText->SetText(FText::FromString(TEXT("▲ Tab")));
        }

        if (PC)
        {
            PC->SetInputMode(FInputModeGameAndUI());
            PC->SetShowMouseCursor(true);
        }

        if (ET_OperatorInput)
        {
            ET_OperatorInput->SetFocus();
        }
    }
    else
    {
        B_ChatBackground->SetVisibility(ESlateVisibility::Collapsed);
        
        if (TXT_ToggleText)
        {
            TXT_ToggleText->SetText(FText::FromString(TEXT("▼ Tab 열기")));
        }

        if (PC)
        {
            PC->SetInputMode(FInputModeGameOnly());
            PC->SetShowMouseCursor(false);
        }
    }
}

bool UUI_MainHUD::IsGuideWindowOpen() const
{
    return B_ChatBackground && B_ChatBackground->GetVisibility() != ESlateVisibility::Collapsed;
}

FReply UUI_MainHUD::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Tab)
    {
        ToggleAIGuideWindow();
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UUI_MainHUD::HandleOnTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{
    if (CommitType != ETextCommit::OnEnter) return;

    const FString QuestionStr = Text.ToString().TrimStartAndEnd();
    if (QuestionStr.IsEmpty()) return;

    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance) return;

    UFactoryAgentClientSubsystem* AgentClient = GameInstance->GetSubsystem<UFactoryAgentClientSubsystem>();
    if (!AgentClient) return;

    const bool bSent = AgentClient->SendOperatorGuideQuestion(QuestionStr, TEXT("unreal-ui-001"));
    if (!bSent)
    {
        if (TXT_GuideResponse)
        {
            TXT_GuideResponse->SetText(FText::FromString(TEXT("AI 가이드 요청 전송에 실패했습니다.")));
        }
        return;
    }

    ET_OperatorInput->SetText(FText::GetEmpty());
    if (TXT_GuideResponse)
    {
        TXT_GuideResponse->SetText(FText::FromString(TEXT("분석 중...")));
    }
}

void UUI_MainHUD::HandleOnOperatorGuideResponse(const FString& RequestId, const FString& Agent, const FString& PayloadJson, const FString& RawMessage)
{
    if (Agent != TEXT("operator_guide")) return;

    TSharedPtr<FJsonObject> PayloadObject;
    if (FactoryAgentJsonUtils::ParseJsonObject(PayloadJson, PayloadObject) && PayloadObject.IsValid())
    {
        FString Answer;
        if (PayloadObject->TryGetStringField(TEXT("final_answer"), Answer) ||
            PayloadObject->TryGetStringField(TEXT("answer"), Answer) ||
            PayloadObject->TryGetStringField(TEXT("text"), Answer))
        {
            if (TXT_GuideResponse)
            {
                TXT_GuideResponse->SetText(FText::FromString(Answer));
            }
        }
    }
}

void UUI_MainHUD::HandleOnOperatorGuideError(const FString& RequestId, const FString& Agent, const FString& ErrorCode, const FString& ErrorMessage, const FString& RawMessage)
{
    if (Agent != TEXT("operator_guide")) return;

    if (TXT_GuideResponse)
    {
        const FString CombinedMessage = ErrorCode.IsEmpty()
            ? ErrorMessage
            : FString::Printf(TEXT("%s: %s"), *ErrorCode, *ErrorMessage);
        TXT_GuideResponse->SetText(FText::FromString(CombinedMessage));
    }
}

// 버튼 누를 때 옛날 이름(TXT_Quest) 대신 새로 개편한 서브 퀘스트 칸들을 제어합니다
void UUI_MainHUD::OnRequestQuestsClicked()
{
    UE_LOG(LogTemp, Log, TEXT("[HUD 퀘스트] OnRequestQuestsClicked() 진입"));

    UGameInstance* GameInstance = GetGameInstance();
    if (GameInstance)
    {
        UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>();
        if (QuestManager)
        {
            UE_LOG(LogTemp, Log, TEXT("[HUD 퀘스트] 서브 퀘스트 슬롯 대기 문구 출력 시작"));
            
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

void UUI_MainHUD::HandleOnSubQuestsGenerated(const FString& RequestId, const TArray<FQuestState>& Quests)
{
    TArray<UTextBlock*> SubBoxes;
    if (TXT_SubQuest_1) SubBoxes.Add(TXT_SubQuest_1);
    if (TXT_SubQuest_2) SubBoxes.Add(TXT_SubQuest_2);
    if (TXT_SubQuest_3) SubBoxes.Add(TXT_SubQuest_3);
    if (TXT_SubQuest_4) SubBoxes.Add(TXT_SubQuest_4);
    if (TXT_SubQuest_5) SubBoxes.Add(TXT_SubQuest_5);

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

// 실패 시에도 옛날 변수 대신 첫 번째 서브 퀘스트 박스에 에러를 안전하게 띄웁니다.
void UUI_MainHUD::HandleOnSubQuestRequestFailed(const FString& RequestId, const FString& ErrorMessage)
{
    if (TXT_SubQuest_1)
    {
        TXT_SubQuest_1->SetText(FText::FromString(TEXT("서버 연결 실패")));
    }
}

void UUI_MainHUD::ToggleQuestWindow()
{
    if (!VB_QuestLayout)
    {
        UE_LOG(LogTemp, Error, TEXT("VB_QuestLayout이 Null입니다. WBP 변수 설정을 확인하세요."));
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

void UUI_MainHUD::UpdateMainQuestUI(const FQuestState& MainQuest)
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

void UUI_MainHUD::HandleOnMainQuestChanged(const FQuestState& NewQuest)
{
    // 게임 시작 시 최초 0번 인덱스가 로드될 때 팝업이 기습 생성되는 현상을 차단합니다.
    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    UQuestManagerSubsystem* QM = GI->GetSubsystem<UQuestManagerSubsystem>();
    if (!QM || QM->GetCurrentMainQuestIndex() == 0) 
    {
        return; 
    }
    
    if (QuestNotifyWidgetClass)
    {
        APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
        if (PC)
        {
            // 순수 독립형 알림 위젯 인스턴스
            UUI_QuestNotify* NotifyWidget = CreateWidget<UUI_QuestNotify>(PC, QuestNotifyWidgetClass);
            if (NotifyWidget)
            {
                NotifyWidget->AddToViewport(100);
                
                FString QuestTitle = NewQuest.Title.ToString();
                FString RewardStr = TEXT("새로운 메인 미션 해제!");

                NotifyWidget->PlayNotify(QuestTitle, RewardStr);
            }
        }
    }
}
