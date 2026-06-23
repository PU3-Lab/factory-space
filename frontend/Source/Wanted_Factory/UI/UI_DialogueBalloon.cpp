#include "UI/UI_DialogueBalloon.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Components/EditableText.h"
#include "Engine/GameInstance.h"
#include "Input/Reply.h"
#include "FactoryAgentClientSubsystem.h" 
#include "FactoryAgentJsonUtils.h"       
#include "Dom/JsonObject.h"

void UUI_DialogueBalloon::NativeConstruct()
{
    Super::NativeConstruct();

    SetIsFocusable(true);
    SetVisibility(ESlateVisibility::Visible);

    // [AI 입력창 바인딩] 엔터키 입력 감지
    if (ET_OperatorInput)
    {
        ET_OperatorInput->OnTextCommitted.AddDynamic(this, &UUI_DialogueBalloon::HandleOnTextCommitted);
    }

    UGameInstance* GI = GetGameInstance();
    if (GI)
    {
        QuestSubsystem = GI->GetSubsystem<UQuestManagerSubsystem>();

        // [AI 웹소켓 서브시스템 바인딩] 응답 및 에러 리스너 등록
        UFactoryAgentClientSubsystem* AgentClient = GI->GetSubsystem<UFactoryAgentClientSubsystem>();
        if (AgentClient)
        {
            AgentClient->OnAgentResponseReceived.AddDynamic(this, &UUI_DialogueBalloon::HandleOnOperatorGuideResponse);
            AgentClient->OnAgentErrorReceived.AddDynamic(this, &UUI_DialogueBalloon::HandleOnOperatorGuideError);
        }
    }

    if (QuestSubsystem)
    {
        QuestSubsystem->OnTutorialStepChanged.AddDynamic(this, &UUI_DialogueBalloon::HandleTutorialStepChanged);
        QuestSubsystem->OnTutorialDialogueLogged.AddDynamic(this, &UUI_DialogueBalloon::HandleTutorialDialogueLogged);
    }

    RefreshDialogueUI();
}

void UUI_DialogueBalloon::NativeDestruct()
{
    // [메모리 누수 방지] 해제 시점에 모든 델리게이트 언바인딩 마감
    if (UGameInstance* GI = GetGameInstance())
    {
        UFactoryAgentClientSubsystem* AgentClient = GI->GetSubsystem<UFactoryAgentClientSubsystem>();
        if (AgentClient)
        {
            AgentClient->OnAgentResponseReceived.RemoveDynamic(this, &UUI_DialogueBalloon::HandleOnOperatorGuideResponse);
            AgentClient->OnAgentErrorReceived.RemoveDynamic(this, &UUI_DialogueBalloon::HandleOnOperatorGuideError);
        }
    }

    Super::NativeDestruct();
}

//  플레이어가 엔터키를 쳤을 때 서버로 질문 전송
void UUI_DialogueBalloon::HandleOnTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{
    if (CommitType != ETextCommit::OnEnter) return;
    const FString QuestionStr = Text.ToString().TrimStartAndEnd();
    if (QuestionStr.IsEmpty()) return;

    UGameInstance* GI = GetGameInstance();
    UFactoryAgentClientSubsystem* AgentClient = GI ? GI->GetSubsystem<UFactoryAgentClientSubsystem>() : nullptr;
    if (!AgentClient) return;

    if (!AgentClient->SendOperatorGuideQuestion(QuestionStr, TEXT("unreal-ui-001")))
    {
        ShowExternalDialogue(TEXT("AI 가이드 요청 전송에 실패했습니다."));
        return;
    }

    ET_OperatorInput->SetText(FText::GetEmpty());
    
    // 기존 ShowExternalDialogue를 재활용하여 TXT_Dialogue 자리에 "분석 중..."을 띄웁니다
    ShowExternalDialogue(TEXT("분석 중...")); 
}

//AI 오퍼레이터의 정답 JSON 패킷 수신 처리
void UUI_DialogueBalloon::HandleOnOperatorGuideResponse(const FString& RequestId, const FString& Agent, const FString& PayloadJson, const FString& RawMessage)
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
            // AI의 최종 답변을 TXT_Dialogue 자리에 선명하게 밀어 넣습니다
            ShowExternalDialogue(Answer);
        }
    }
}

// 통신 에러 발생 시 처리
void UUI_DialogueBalloon::HandleOnOperatorGuideError(const FString& RequestId, const FString& Agent, const FString& ErrorCode, const FString& ErrorMessage, const FString& RawMessage)
{
    if (Agent != TEXT("operator_guide")) return;
    
    FString CombinedMessage = ErrorCode.IsEmpty() ? ErrorMessage : FString::Printf(TEXT("%s: %s"), *ErrorCode, *ErrorMessage);
    
    // 🎯 에러 메시지도 텍스트 박스에 깔끔하게 출력
    ShowExternalDialogue(CombinedMessage);
}

// --- 이하 기존 UI_DialogueBalloon 함수들 동일 유지 ---

void UUI_DialogueBalloon::RefreshDialogueUI()
{
    if (bHasExternalDialogue)
    {
        DisplayCurrentLine();
        return;
    }
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
    if (bHasExternalDialogue) return;
    if (QuestSubsystem && QuestSubsystem->HasPendingTutorialStartDialogue()) return;
    RefreshDialogueUI();
}

void UUI_DialogueBalloon::HandleTutorialDialogueLogged(const FString& QuestId, const FString& TriggerType, const TArray<FTutorialQuestDialogueLine>& Lines)
{
    if (bHasExternalDialogue) return;
    if (Lines.IsEmpty())
    {
        CachedLines.Empty();
        DisplayCurrentLine();
        return;
    }
    if (TriggerType == TEXT("on_complete"))
    {
        CachedLines = Lines;
        DisplayCurrentLine();
        return;
    }
    if (TriggerType != TEXT("on_start")) return;
    CachedLines = Lines;
    DisplayCurrentLine();
}

void UUI_DialogueBalloon::ShowExternalDialogue(const FString& DialogueText)
{
    bHasExternalDialogue = !DialogueText.TrimStartAndEnd().IsEmpty();
    ExternalDialogueText = bHasExternalDialogue ? DialogueText : FString();
    DisplayCurrentLine();
}

void UUI_DialogueBalloon::ClearExternalDialogue()
{
    if (!bHasExternalDialogue && ExternalDialogueText.IsEmpty()) return;
    bHasExternalDialogue = false;
    ExternalDialogueText.Empty();
    RefreshDialogueUI();
}

void UUI_DialogueBalloon::DisplayCurrentLine()
{
    if (bHasExternalDialogue)
    {
        SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        if (DialogueContainer) DialogueContainer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        if (TXT_Dialogue) TXT_Dialogue->SetText(FText::FromString(ExternalDialogueText));
        return;
    }

    if (CachedLines.IsEmpty())
    {
        SetVisibility(ESlateVisibility::Collapsed);
        if (DialogueContainer) DialogueContainer->SetVisibility(ESlateVisibility::Collapsed);
        if (TXT_Dialogue) TXT_Dialogue->SetText(FText::GetEmpty());
        return;
    }

    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    if (DialogueContainer) DialogueContainer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    FString CombinedDialogue;
    for (int32 LineIndex = 0; LineIndex < CachedLines.Num(); ++LineIndex)
    {
        if (!CombinedDialogue.IsEmpty()) CombinedDialogue += TEXT("\n");
        CombinedDialogue += CachedLines[LineIndex].Dialogue;
    }

    if (TXT_Dialogue) TXT_Dialogue->SetText(FText::FromString(CombinedDialogue));
}

FReply UUI_DialogueBalloon::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    FReply Reply = Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
    if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton) return Reply;

    if (bHasExternalDialogue)
    {
        // AI 답변이 노출된 상태에서 화면을 좌클릭하면 기존 대화 로그로 부드럽게 복귀합니다
        ClearExternalDialogue(); 
        return FReply::Handled();
    }

    if (!QuestSubsystem || !QuestSubsystem->HasPendingTutorialStartDialogue()) return Reply;
    QuestSubsystem->RevealPendingTutorialStartDialogue();
    return FReply::Handled();
}