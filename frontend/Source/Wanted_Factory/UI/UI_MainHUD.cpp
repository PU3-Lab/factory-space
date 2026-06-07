#include "UI_MainHUD.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "QuestManagerSubsystem.h"
#include "PlanetEventManagerSubsystem.h"
#include "FactoryAgentClientSubsystem.h" // 웹소켓 서브시스템 헤더
#include "FactoryAgentJsonUtils.h"       // JSON 파싱 유틸리티 헤더
#include "Dom/JsonObject.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/UserWidget.h"
#include "UMG.h"
#include "Kismet/GameplayStatics.h"

void UUI_MainHUD::NativeConstruct()
{
    Super::NativeConstruct();
    
    // 퀘스트 요청 버튼 바인딩
    if (BTN_RequestQuests)
    {
        BTN_RequestQuests->OnClicked.AddDynamic(this, &UUI_MainHUD::OnRequestQuestsClicked);
    }

    // 오퍼레이터 채팅 입력창 바인딩
    if (ET_OperatorInput)
    {
        ET_OperatorInput->OnTextCommitted.AddDynamic(this, &UUI_MainHUD::HandleOnTextCommitted);
    }

    // 토글 버튼 클릭 이벤트를 바인딩
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
            // 퀘스트 생성 성공 이벤트 바인딩
            QuestManager->OnSubQuestsGenerated.AddDynamic(this, &UUI_MainHUD::HandleOnSubQuestsGenerated);
            
            QuestManager->OnSubQuestRequestFailed.AddDynamic(this, &UUI_MainHUD::HandleOnSubQuestRequestFailed);
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
            // 1. 몇 일차 갱신
            if (TXT_DisasterDay)
            {
                int32 CurrentDay = PlanetManager->GetCurrentDayIndex();
                TXT_DisasterDay->SetText(FText::FromString(FString::Printf(TEXT("DAY %02d"), CurrentDay)));
            }

            // 2. 인게임 시간 갱신
            if (TXT_InGameTime)
            {
                TXT_InGameTime->SetText(FText::FromString(PlanetManager->GetCurrentTime24String()));
            }
        }
    }
}

void UUI_MainHUD::OnToggleGuideClicked()
{
    UE_LOG(LogTemp, Warning, TEXT("토글 버튼 클릭됨"));

    if (!B_ChatBackground)
    {
        UE_LOG(LogTemp, Error, TEXT("B_ChatBackground가 Null입니다 변수 이름을 다시 확인하세요."));
        return;
    }
    
    if (!B_ChatBackground) return;

    // 현재 배경 창이 꺼져(Collapsed) 있다면? ➡️ 켜준다
    if (B_ChatBackground->GetVisibility() == ESlateVisibility::Collapsed)
    {
        // 1. 창을 화면에 표시 (SelfHitTestInvisible이 마우스 클릭 관통 방지에 좋습니다)
        B_ChatBackground->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        
        // 2. 버튼 텍스트 변경
        if (TXT_ToggleText)
        {
            TXT_ToggleText->SetText(FText::FromString(TEXT("▲ AI 가이드 접기")));
        }
    }
    // 현재 배경 창이 켜져 있다면? ➡️ 꺼준다
    else
    {
        // 1. 창과 내부 공간까지 싹 제거(Collapsed)
        B_ChatBackground->SetVisibility(ESlateVisibility::Collapsed);
        
        // 2. 버튼 텍스트 변경
        if (TXT_ToggleText)
        {
            TXT_ToggleText->SetText(FText::FromString(TEXT("▼ AI 가이드 열기")));
        }
    }
}

void UUI_MainHUD::HandleOnTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{
    if (CommitType != ETextCommit::OnEnter)
    {
        return;
    }

    const FString QuestionStr = Text.ToString().TrimStartAndEnd();
    if (QuestionStr.IsEmpty())
    {
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance)
    {
        return;
    }

    UFactoryAgentClientSubsystem* AgentClient = GameInstance->GetSubsystem<UFactoryAgentClientSubsystem>();
    if (!AgentClient)
    {
        return;
    }

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

//  question 답변 데이터 파싱해서 출력
void UUI_MainHUD::HandleOnOperatorGuideResponse(const FString& RequestId, const FString& Agent, const FString& PayloadJson, const FString& RawMessage)
{
    // namespace 규칙에 적혀있던 "operator_guide" 에이전트 신호인지 필터링
    if (Agent != TEXT("operator_guide")) return;

    TSharedPtr<FJsonObject> PayloadObject;
    // 페이로드 JSON을 오브젝트화합니다.
    if (FactoryAgentJsonUtils::ParseJsonObject(PayloadJson, PayloadObject) && PayloadObject.IsValid())
    {
        FString Answer;
        if (
            PayloadObject->TryGetStringField(TEXT("final_answer"), Answer) ||
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

void UUI_MainHUD::HandleOnOperatorGuideError(
    const FString& RequestId,
    const FString& Agent,
    const FString& ErrorCode,
    const FString& ErrorMessage,
    const FString& RawMessage)
{
    if (Agent != TEXT("operator_guide"))
    {
        return;
    }

    if (TXT_GuideResponse)
    {
        const FString CombinedMessage = ErrorCode.IsEmpty()
            ? ErrorMessage
            : FString::Printf(TEXT("%s: %s"), *ErrorCode, *ErrorMessage);
        TXT_GuideResponse->SetText(FText::FromString(CombinedMessage));
    }
}

// 버튼을 누르는 순간 에이전트 팀 전송 파이프라인 트리거
void UUI_MainHUD::OnRequestQuestsClicked()
{
    UE_LOG(LogTemp, Log, TEXT("[HUD 퀘스트] OnRequestQuestsClicked() 함수 내부 진ip 성공"));

    UGameInstance* GameInstance = GetGameInstance();
    if (GameInstance)
    {
        UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>();
        if (QuestManager)
        {
            // 텍스트가 안 바뀌는 버그 추적용 (위젯 유효성 검사)
            UE_LOG(LogTemp, Log, TEXT("[HUD 퀘스트] 텍스트 위젯 상태 체크 - TXT_Quest_1 유효성: %s"), TXT_Quest_1 ? TEXT("True") : TEXT("Null"));

            if (TXT_Quest_1) TXT_Quest_1->SetText(FText::FromString(TEXT("AI 응답 대기 중...")));
            if (TXT_Quest_2) TXT_Quest_2->SetText(FText::FromString(TEXT("")));
            if (TXT_Quest_3) TXT_Quest_3->SetText(FText::FromString(TEXT("")));
            if (TXT_Quest_4) TXT_Quest_4->SetText(FText::FromString(TEXT("")));
            if (TXT_Quest_5) TXT_Quest_5->SetText(FText::FromString(TEXT("")));

            // 에이전트에 서버 연결 명령 후 요청 시도
            QuestManager->ConnectQuestAgent(); 
            QuestManager->RequestSubQuests();
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[HUD 퀘스트] QuestManagerSubsystem을 찾을 수 없습니다!"));
        }
    }
}

void UUI_MainHUD::HandleOnSubQuestsGenerated(const FString& RequestId, const TArray<FQuestState>& Quests)
{
    TArray<UTextBlock*> QuestTextBoxes;
    if (TXT_Quest_1) QuestTextBoxes.Add(TXT_Quest_1);
    if (TXT_Quest_2) QuestTextBoxes.Add(TXT_Quest_2);
    if (TXT_Quest_3) QuestTextBoxes.Add(TXT_Quest_3);
    if (TXT_Quest_4) QuestTextBoxes.Add(TXT_Quest_4);
    if (TXT_Quest_5) QuestTextBoxes.Add(TXT_Quest_5);

    for (int32 i = 0; i < QuestTextBoxes.Num(); ++i)
    {
        if (i < Quests.Num())
        {
            FText QuestTitle = Quests[i].Title;
            
            // 자동 줄바꿈 
            QuestTextBoxes[i]->SetAutoWrapText(true);
            
            QuestTextBoxes[i]->SetText(QuestTitle);
            QuestTextBoxes[i]->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            QuestTextBoxes[i]->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}

// 서버가 꺼져있거나 통신 에러 시 텍스트 처리 규칙
void UUI_MainHUD::HandleOnSubQuestRequestFailed(const FString& RequestId, const FString& ErrorMessage)
{
    if (TXT_Quest_1)
    {
        TXT_Quest_1->SetText(FText::FromString(TEXT("서버 연결 실패")));
    }
}

void UUI_MainHUD::ToggleQuestWindow()
{
    if (!QuestToggleAnim || !VB_QuestLayout)
    {
        UE_LOG(LogTemp, Error, TEXT("QuestToggleAnim 또는 VB_QuestLayout이 Null입니다. WBP 세팅을 확인하세요."));
        return;
    }

    // 1. 현재 열려 있다면? ➡️ 닫는 애니메이션 재생 (역방향)
    if (bIsQuestWindowOpen)
    {
        // 🌟 에러를 일으키던 Enum 명칭을 아예 싹 걷어내고 전용 역방향 함수를 호출합니다.
        // PlayAnimationReverse(애니메이션에셋, 재생속도, 에디터애니메이션보존여부)
        PlayAnimationReverse(QuestToggleAnim, 1.0f);
        bIsQuestWindowOpen = false;
    }
    // 2. 현재 닫혀 있다면? ➡️ 열리는 애니메이션 재생 (정방향)
    else
    {
        VB_QuestLayout->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        
        // 🌟 전용 정방향 함수를 호출합니다.
        PlayAnimationForward(QuestToggleAnim, 1.0f);
        bIsQuestWindowOpen = true;
    }
}