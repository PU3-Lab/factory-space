#include "UI/UI_DialogueBalloon.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Components/EditableText.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Input/Reply.h"
#include "FactoryAgentClientSubsystem.h" 
#include "FactoryAgentJsonUtils.h"       
#include "Dom/JsonObject.h"

void UUI_DialogueBalloon::NativeConstruct()
{
    Super::NativeConstruct();

    SetIsFocusable(true);
    SetVisibility(ESlateVisibility::Visible);
    
    if (ET_OperatorInput)
    {
        ET_OperatorInput->OnTextCommitted.RemoveDynamic(this, &UUI_DialogueBalloon::HandleOnTextCommitted);
        ET_OperatorInput->OnTextCommitted.AddDynamic(this, &UUI_DialogueBalloon::HandleOnTextCommitted);
        ET_OperatorInput->SetText(FText::GetEmpty());
        ET_OperatorInput->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (UGameInstance* GI = GetGameInstance())
    {
        QuestSubsystem = GI->GetSubsystem<UQuestManagerSubsystem>();

        UFactoryAgentClientSubsystem* AgentClient = GI->GetSubsystem<UFactoryAgentClientSubsystem>();
        if (AgentClient)
        {
            AgentClient->OnAgentResponseReceived.AddDynamic(this, &UUI_DialogueBalloon::HandleOnOperatorGuideResponse);
            AgentClient->OnAgentErrorReceived.AddDynamic(this, &UUI_DialogueBalloon::HandleOnOperatorGuideError);
            AgentClient->OnAgentProgressReceived.AddDynamic(this, &UUI_DialogueBalloon::HandleOnOperatorGuideProgress);
        }
    }

    if (QuestSubsystem)
    {
        QuestSubsystem->OnTutorialStepChanged.AddDynamic(this, &UUI_DialogueBalloon::HandleTutorialStepChanged);
        QuestSubsystem->OnTutorialDialogueLogged.AddDynamic(this, &UUI_DialogueBalloon::HandleTutorialDialogueLogged);
    }

    if (!IMG_RightClickPrompt && WidgetTree)
    {
        IMG_RightClickPrompt = Cast<UImage>(WidgetTree->FindWidget(TEXT("IMG_RightClickPrompt")));
    }

    ApplyContinuePromptBrush();
    RefreshDialogueUI();
}

void UUI_DialogueBalloon::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    UpdateContinuePromptBlink(InDeltaTime);
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
            AgentClient->OnAgentProgressReceived.RemoveDynamic(this, &UUI_DialogueBalloon::HandleOnOperatorGuideProgress);
        }
    }

    Super::NativeDestruct();
}

//  플레이어가 엔터키를 쳤을 때 서버로 질문 전송
void UUI_DialogueBalloon::HandleOnTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{
    // 엔터키 커밋일 때만 로직 가동
    if (CommitType != ETextCommit::OnEnter) return;
    
    const FString QuestionStr = Text.ToString().TrimStartAndEnd();
    
    // 엔터를 치면 내용 유무 상관없이 인풋창은 즉시 초기화 및 증발
    ET_OperatorInput->SetText(FText::FromString(QuestionStr));
    ET_OperatorInput->SetVisibility(ESlateVisibility::Collapsed);

    // 포커스를 완벽하게 꺼내 캐릭터 조작 모드로 강제 복구
    if (APlayerController* PC = GetOwningPlayer())
    {
        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = false;
    }

    // 만약 아무것도 안 치고 엔터만 눌렀다면 가이드 요청 없이 닫기만 하고 마무리
    if (QuestionStr.IsEmpty()) return;

    UGameInstance* GI = GetGameInstance();
    UFactoryAgentClientSubsystem* AgentClient = GI ? GI->GetSubsystem<UFactoryAgentClientSubsystem>() : nullptr;
    if (!AgentClient) return;

    if (!AgentClient->SendOperatorGuideQuestion(QuestionStr, TEXT("unreal-ui-001")))
    {
        ShowExternalDialogue(TEXT("AI 가이드 요청 전송에 실패했습니다."));
        return;
    }
    
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
    
    // 에러 메시지도 텍스트 박스에 깔끔하게 출력
    ShowExternalDialogue(CombinedMessage);
}

// --- 이하 기존 UI_DialogueBalloon 함수들 동일 유지 ---

void UUI_DialogueBalloon::HandleOnOperatorGuideProgress(const FString& RequestId, const FString& Agent, const FString& Stage, const FString& Message, const FString& RawMessage)
{
    if (Agent != TEXT("operator_guide")) return;

    const FString ProgressMessage = Message.TrimStartAndEnd();
    if (ProgressMessage.IsEmpty()) return;

    ShowExternalDialogue(ProgressMessage);
}

void UUI_DialogueBalloon::RefreshDialogueUI()
{
    if (bHasExternalDialogue)
    {
        DisplayCurrentLine();
        return;
    }
    CachedLines.Empty();
    CachedTriggerType.Empty();
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
        CachedTriggerType = LoggedTriggerType;
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
    CachedTriggerType = TEXT("on_start");
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
        CachedTriggerType = TriggerType;
        DisplayCurrentLine();
        return;
    }
    if (TriggerType == TEXT("on_complete"))
    {
        CachedLines = Lines;
        CachedTriggerType = TriggerType;
        DisplayCurrentLine();
        return;
    }
    if (TriggerType != TEXT("on_start")) return;
    CachedLines = Lines;
    CachedTriggerType = TriggerType;
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
    // 위젯 본체 루트는 절대로 Collapsed 시키지 않고 항시 살려둡니다.
    // 그래야 하단의 / 인풋 Border 창이 언제든 독립적으로 튀어나올 수 있습니다.
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    if (bHasExternalDialogue)
    {
        if (SB_DialogueData)   SB_DialogueData->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        if (DialogueContainer) DialogueContainer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        if (TXT_Dialogue)      TXT_Dialogue->SetText(FText::FromString(ExternalDialogueText));
        UpdateContinuePromptVisibility();
        return;
    }

    if (CachedLines.IsEmpty())
    {
        // 퀘스트가 끝나 대사가 비어있다면, 인풋 창은 내버려 두고 
        // 말풍선 배경(Image_229)과 글자가 담긴 'Size Box 세트'만 깔끔하게 투명 청소합니다
        if (SB_DialogueData)   SB_DialogueData->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        if (DialogueContainer) DialogueContainer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        if (TXT_Dialogue)      TXT_Dialogue->SetText(FText::FromString(TEXT(" ")));
        UpdateContinuePromptVisibility();
        return;
    }

    // 일반 대사가 존재할 때는 주머니 세트를 다시 이쁘게 노출합니다.
    if (SB_DialogueData)   SB_DialogueData->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    if (DialogueContainer) DialogueContainer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    FString CombinedDialogue;
    for (int32 LineIndex = 0; LineIndex < CachedLines.Num(); ++LineIndex)
    {
        if (!CombinedDialogue.IsEmpty()) CombinedDialogue += TEXT("\n");
        CombinedDialogue += CachedLines[LineIndex].Dialogue;
    }

    if (TXT_Dialogue) TXT_Dialogue->SetText(FText::FromString(CombinedDialogue));
    UpdateContinuePromptVisibility();
}

void UUI_DialogueBalloon::UpdateContinuePromptVisibility()
{
    const bool bShouldShowPrompt = !bHasExternalDialogue
        && !CachedLines.IsEmpty()
        && CachedTriggerType == TEXT("on_complete")
        && (!CachedLines.Last().Dialogue.Contains(TEXT(". 을 눌러서 창 닫기")));

    bShowRightClickPrompt = bShouldShowPrompt;
    if (!IMG_RightClickPrompt)
    {
        return;
    }

    if (!bShowRightClickPrompt)
    {
        IMG_RightClickPrompt->SetVisibility(ESlateVisibility::Collapsed);
        IMG_RightClickPrompt->SetRenderOpacity(0.0f);
        RightClickPromptBlinkTime = 0.0f;
        return;
    }

    IMG_RightClickPrompt->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UUI_DialogueBalloon::UpdateContinuePromptBlink(float InDeltaTime)
{
    if (!IMG_RightClickPrompt || !bShowRightClickPrompt)
    {
        return;
    }

    RightClickPromptBlinkTime += FMath::Max(0.0f, InDeltaTime);
    const float Alpha = 0.35f + (0.65f * (0.5f + 0.5f * FMath::Sin(RightClickPromptBlinkTime * 6.0f)));
    IMG_RightClickPrompt->SetRenderOpacity(Alpha);
}

void UUI_DialogueBalloon::ApplyContinuePromptBrush()
{
    if (!IMG_RightClickPrompt)
    {
        return;
    }

    static const TCHAR* RightClickTexturePath = TEXT("/Game/LDJ/UI/UI_Image/RightClick.RightClick");
    if (UTexture2D* RightClickTexture = LoadObject<UTexture2D>(nullptr, RightClickTexturePath))
    {
        IMG_RightClickPrompt->SetBrushFromTexture(RightClickTexture, true);
    }

    IMG_RightClickPrompt->SetVisibility(ESlateVisibility::Collapsed);
    IMG_RightClickPrompt->SetRenderOpacity(0.0f);
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

void UUI_DialogueBalloon::ToggleAIGuide(APlayerController* PC)
{
    if (!PC || !ET_OperatorInput) return;

    // 1. 이미 AI 답변이 출력 중인 상태라면 원래 대사로 롤백
    if (bHasExternalDialogue)
    {
        ClearExternalDialogue();
        ET_OperatorInput->SetVisibility(ESlateVisibility::Collapsed);
        
        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = false;
        return;
    }

    // 2. 일반 상태에서 / 키를 처음 눌렀을 때 (1타 활성화 보장)
    if (ET_OperatorInput->GetVisibility() != ESlateVisibility::Visible)
    {
        SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        ET_OperatorInput->SetVisibility(ESlateVisibility::Visible);
        
        // 슬레이트 창 자체에 입력창 위젯 포커스를 다이렉트로 강제 주입합니다.
        FInputModeGameAndUI InputModeData;
        InputModeData.SetWidgetToFocus(ET_OperatorInput->GetCachedWidget());
        InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(InputModeData);
        
        PC->bShowMouseCursor = true;
        ET_OperatorInput->SetFocus();
        
        // / 키 입력 로직이 텍스트 필드를 오염시켜 힌트텍스트를 지우던 버그를 강제 세척합니다.
    }
    else
    {
        // 이미 켜져 있는 상태에서 다시 누르면 취소하고 탈출
        ET_OperatorInput->SetVisibility(ESlateVisibility::Collapsed);
        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = false;
    }
}
