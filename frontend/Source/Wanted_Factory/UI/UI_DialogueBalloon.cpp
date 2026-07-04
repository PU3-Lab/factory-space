#include "UI/UI_DialogueBalloon.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Components/EditableText.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "DrawDebugHelpers.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Input/Reply.h"
#include "FactoryManagerSubsystem.h"
#include "FactoryAgentClientSubsystem.h" 
#include "FactoryAgentJsonUtils.h"       
#include "MachineBase.h"
#include "Machines/PowerGridNode.h"
#include "Dom/JsonObject.h"
#include "InputCoreTypes.h"
#include "Wanted_Factory.h"

void UUI_DialogueBalloon::NativeConstruct()
{
    Super::NativeConstruct();

    SetIsFocusable(true);
    SetVisibility(ESlateVisibility::Visible);

    if (B_OperatorInput)
    {
        B_OperatorInput->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }
    
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
            AgentClient->OnAgentResponseReceived.AddDynamic(this, &UUI_DialogueBalloon::HandleOnProcessOptimizerResponse);
            AgentClient->OnAgentErrorReceived.AddDynamic(this, &UUI_DialogueBalloon::HandleOnOperatorGuideError);
            AgentClient->OnAgentProgressReceived.AddDynamic(this, &UUI_DialogueBalloon::HandleOnOperatorGuideProgress);
            AgentClient->OnMaterialGenerationResponseReceived.AddDynamic(this, &UUI_DialogueBalloon::HandleOnMaterialGenerationResponse);
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
    UpdateTrackedProcessOptimizerIssues();
    RefreshTrackedProcessOptimizerHighlights();
}

void UUI_DialogueBalloon::NativeDestruct()
{
    // [硫붾え由??꾩닔 諛⑹?] ?댁젣 ?쒖젏??紐⑤뱺 ?몃━寃뚯씠???몃컮?몃뵫 留덇컧
    if (UGameInstance* GI = GetGameInstance())
    {
        UFactoryAgentClientSubsystem* AgentClient = GI->GetSubsystem<UFactoryAgentClientSubsystem>();
        if (AgentClient)
        {
            AgentClient->OnAgentResponseReceived.RemoveDynamic(this, &UUI_DialogueBalloon::HandleOnOperatorGuideResponse);
            AgentClient->OnAgentResponseReceived.RemoveDynamic(this, &UUI_DialogueBalloon::HandleOnProcessOptimizerResponse);
            AgentClient->OnAgentErrorReceived.RemoveDynamic(this, &UUI_DialogueBalloon::HandleOnOperatorGuideError);
            AgentClient->OnAgentProgressReceived.RemoveDynamic(this, &UUI_DialogueBalloon::HandleOnOperatorGuideProgress);
            AgentClient->OnMaterialGenerationResponseReceived.RemoveDynamic(this, &UUI_DialogueBalloon::HandleOnMaterialGenerationResponse);
        }
    }

    Super::NativeDestruct();
}

//  ?뚮젅?댁뼱媛 ?뷀꽣?ㅻ? 爾ㅼ쓣 ???쒕쾭濡?吏덈Ц ?꾩넚
void UUI_DialogueBalloon::HandleOnTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{
    // ?뷀꽣??而ㅻ컠???뚮쭔 濡쒖쭅 媛??
    if (CommitType != ETextCommit::OnEnter) return;
    
    const FString QuestionStr = Text.ToString().TrimStartAndEnd();
    
    // ?뷀꽣瑜?移섎㈃ ?댁슜 ?좊Т ?곴??놁씠 ?명뭼李쎌? 利됱떆 珥덇린??諛?利앸컻
    ET_OperatorInput->SetText(FText::FromString(QuestionStr));
    ET_OperatorInput->SetVisibility(ESlateVisibility::Collapsed);

    // ?ъ빱?ㅻ? ?꾨꼍?섍쾶 爰쇰궡 罹먮┃??議곗옉 紐⑤뱶濡?媛뺤젣 蹂듦뎄
    if (APlayerController* PC = GetOwningPlayer())
    {
        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = false;
    }

    // 留뚯빟 ?꾨Т寃껊룄 ??移섍퀬 ?뷀꽣留??뚮??ㅻ㈃ 媛?대뱶 ?붿껌 ?놁씠 ?リ린留??섍퀬 留덈Т由?
    if (QuestionStr.IsEmpty()) return;
    LastSubmittedOperatorGuideQuestion = QuestionStr;
    bShowLastSubmittedOperatorGuideQuestion = true;

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

//AI ?ㅽ띁?덉씠?곗쓽 ?뺣떟 JSON ?⑦궥 ?섏떊 泥섎━
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
            ShowExternalDialogue(Answer);
        }
    }
}

// ?듭떊 ?먮윭 諛쒖깮 ??泥섎━
void UUI_DialogueBalloon::HandleOnOperatorGuideError(const FString& RequestId, const FString& Agent, const FString& ErrorCode, const FString& ErrorMessage, const FString& RawMessage)
{
    if (Agent != TEXT("operator_guide")) return;
    
    FString CombinedMessage = ErrorCode.IsEmpty() ? ErrorMessage : FString::Printf(TEXT("%s: %s"), *ErrorCode, *ErrorMessage);
    
    ShowExternalDialogue(CombinedMessage);
}

// --- ?댄븯 湲곗〈 UI_DialogueBalloon ?⑥닔???숈씪 ?좎? ---

void UUI_DialogueBalloon::HandleOnOperatorGuideProgress(const FString& RequestId, const FString& Agent, const FString& Stage, const FString& Message, const FString& RawMessage)
{
    if (Agent != TEXT("operator_guide")) return;

    const FString ProgressMessage = Message.TrimStartAndEnd();
    if (ProgressMessage.IsEmpty()) return;

    ShowExternalDialogue(ProgressMessage);
}

void UUI_DialogueBalloon::HandleOnMaterialGenerationResponse(const FFactoryMaterialGenerationResponse& Response)
{
    const FString DialogueMessage = Response.Message.TrimStartAndEnd();
    if (DialogueMessage.IsEmpty())
    {
        return;
    }

    bShowLastSubmittedOperatorGuideQuestion = false;
    ShowExternalDialogue(DialogueMessage);
}

void UUI_DialogueBalloon::HandleOnProcessOptimizerResponse(
    const FString& RequestId,
    const FString& Agent,
    const FString& PayloadJson,
    const FString& RawMessage)
{
    if (Agent != TEXT("process_optimizer"))
    {
        return;
    }

    TSharedPtr<FJsonObject> PayloadObject;
    if (!FactoryAgentJsonUtils::ParseJsonObject(PayloadJson, PayloadObject) || !PayloadObject.IsValid())
    {
        return;
    }

    bProcessOptimizerRequestInFlight = false;

    const bool bIsAnalyzeResponse =
        RequestId.StartsWith(TEXT("optimizer-analyze-")) ||
        PayloadObject->HasField(TEXT("suggestions"));
    const bool bIsStateUpdateResponse =
        RequestId.StartsWith(TEXT("optimizer-state-")) ||
        PayloadObject->HasField(TEXT("optimization_alert"));

    if (bIsStateUpdateResponse && !bIsAnalyzeResponse)
    {
        LOG_LC(TEXT("Process optimizer state update response ignored by dialogue UI. request_id=%s"), *RequestId);
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>* SuggestionValues = nullptr;
    TArray<FTrackedProcessOptimizerIssue> NewIssues;
    if (PayloadObject->TryGetArrayField(TEXT("suggestions"), SuggestionValues) &&
        SuggestionValues != nullptr &&
        SuggestionValues->Num() > 0)
    {
        for (const TSharedPtr<FJsonValue>& SuggestionValue : *SuggestionValues)
        {
            if (!SuggestionValue.IsValid() || SuggestionValue->Type != EJson::Object)
            {
                continue;
            }

            const TSharedPtr<FJsonObject> SuggestionObject = SuggestionValue->AsObject();
            if (!SuggestionObject.IsValid())
            {
                continue;
            }

            const TSharedPtr<FJsonObject>* TargetObject = nullptr;
            if (!SuggestionObject->TryGetObjectField(TEXT("target"), TargetObject) ||
                TargetObject == nullptr ||
                !TargetObject->IsValid())
            {
                continue;
            }

            const FString TargetId =
                FactoryAgentJsonUtils::GetStringField(*TargetObject, TEXT("id")).TrimStartAndEnd();
            if (TargetId.IsEmpty())
            {
                continue;
            }

            FTrackedProcessOptimizerIssue Issue;
            Issue.TargetId = TargetId;
            Issue.IssueType = ClassifyProcessOptimizerIssue(SuggestionObject);
            NewIssues.Add(Issue);
        }
    }

    if (NewIssues.Num() > 0)
    {
        SetTrackedProcessOptimizerIssues(NewIssues);
        ShowExternalDialogue(TEXT("발견한 문제를 강조 표시했습니다"));
        return;
    }

    TrackedProcessOptimizerIssues.Reset();
    if (UWorld* World = GetWorld())
    {
        FlushPersistentDebugLines(World);
    }

    ShowExternalDialogue(TEXT("문제가 발견되지 않았습니다"));
}
void UUI_DialogueBalloon::SetTrackedProcessOptimizerIssues(const TArray<FTrackedProcessOptimizerIssue>& NewIssues)
{
    TrackedProcessOptimizerIssues.Reset();
    bCanAnnounceProcessOptimizerResolution = false;
    if (NewIssues.Num() == 0)
    {
        return;
    }

    UGameInstance* GI = GetGameInstance();
    UFactoryManagerSubsystem* FactoryManager = GI ? GI->GetSubsystem<UFactoryManagerSubsystem>() : nullptr;
    if (!FactoryManager)
    {
        return;
    }

    for (const FTrackedProcessOptimizerIssue& NewIssue : NewIssues)
    {
        FTrackedProcessOptimizerIssue ResolvedIssue = NewIssue;
        AMachineBase* Machine = FactoryManager->FindMachineByAgentTargetId(NewIssue.TargetId);
        if (!Machine)
        {
            LOG_LC(TEXT("Process optimizer target was not found in world: %s"), *NewIssue.TargetId);
            continue;
        }

        ResolvedIssue.Machine = Machine;
        ResolvedIssue.InitialDurability = Machine->GetCurrentDurability();
        TrackedProcessOptimizerIssues.Add(ResolvedIssue);
        bCanAnnounceProcessOptimizerResolution = true;

        LOG_LC(
            TEXT("Process optimizer target tracked: id=%s actor=%s issue_type=%d location=(X=%.2f,Y=%.2f,Z=%.2f)"),
            *ResolvedIssue.TargetId,
            *Machine->GetName(),
            static_cast<int32>(ResolvedIssue.IssueType),
            Machine->GetActorLocation().X,
            Machine->GetActorLocation().Y,
            Machine->GetActorLocation().Z);
    }
}

void UUI_DialogueBalloon::RefreshTrackedProcessOptimizerHighlights()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    for (const FTrackedProcessOptimizerIssue& Issue : TrackedProcessOptimizerIssues)
    {
        const AMachineBase* Machine = Issue.Machine.Get();
        if (!Machine)
        {
            continue;
        }

        const FVector MachineLocation = Machine->GetActorLocation();
        const FColor HighlightColor =
            Issue.IssueType == ETrackedProcessOptimizerIssueType::Power ? FColor::Cyan : FColor::Yellow;

        DrawDebugSphere(World, MachineLocation, 180.0f, 24, HighlightColor, false, 0.15f, 0, 4.0f);
        DrawDebugBox(World, MachineLocation, FVector(120.0f, 120.0f, 120.0f), HighlightColor, false, 0.15f, 0, 3.0f);
    }
}

void UUI_DialogueBalloon::UpdateTrackedProcessOptimizerIssues()
{
    if (TrackedProcessOptimizerIssues.Num() == 0)
    {
        return;
    }

    const int32 RemovedCount = TrackedProcessOptimizerIssues.RemoveAll(
        [this](const FTrackedProcessOptimizerIssue& Issue)
        {
            const bool bResolved = IsTrackedProcessOptimizerIssueResolved(Issue);
            if (bResolved)
            {
                LOG_LC(
                    TEXT("Process optimizer target resolved in game: id=%s issue_type=%d"),
                    *Issue.TargetId,
                    static_cast<int32>(Issue.IssueType));
            }

            return bResolved;
        });

    if (RemovedCount > 0 &&
        TrackedProcessOptimizerIssues.Num() == 0 &&
        !bProcessOptimizerRequestInFlight &&
        bCanAnnounceProcessOptimizerResolution)
    {
        bCanAnnounceProcessOptimizerResolution = false;
        ShowExternalDialogue(TEXT("문제의 해결을 확인했습니다"));
    }
}

bool UUI_DialogueBalloon::IsTrackedProcessOptimizerIssueResolved(const FTrackedProcessOptimizerIssue& Issue) const
{
    const AMachineBase* Machine = Issue.Machine.Get();
    if (!Machine)
    {
        return true;
    }

    switch (Issue.IssueType)
    {
    case ETrackedProcessOptimizerIssueType::Durability:
        return !Machine->isBroken() &&
            Machine->GetCurrentDurability() > Issue.InitialDurability &&
            Machine->GetCurrentDurability() > Machine->GetLowDurabilityWarningThreshold();

    case ETrackedProcessOptimizerIssueType::Power:
        if (const APowerGridNode* PowerGridNode = Cast<APowerGridNode>(Machine))
        {
            UGameInstance* GI = GetGameInstance();
            const UFactoryManagerSubsystem* FactoryManager = GI ? GI->GetSubsystem<UFactoryManagerSubsystem>() : nullptr;
            return FactoryManager && FactoryManager->IsPowerGridNodeEnergized(PowerGridNode);
        }

        return Machine->NeedsPower() && Machine->HasEnoughPower();

    case ETrackedProcessOptimizerIssueType::Unknown:
    default:
        return false;
    }
}

ETrackedProcessOptimizerIssueType UUI_DialogueBalloon::ClassifyProcessOptimizerIssue(const TSharedPtr<FJsonObject>& SuggestionObject) const
{
    if (!SuggestionObject.IsValid())
    {
        return ETrackedProcessOptimizerIssueType::Unknown;
    }

    const TSharedPtr<FJsonObject>* TargetObject = nullptr;
    FString TargetId;
    if (SuggestionObject->TryGetObjectField(TEXT("target"), TargetObject) &&
        TargetObject != nullptr &&
        TargetObject->IsValid())
    {
        TargetId = FactoryAgentJsonUtils::GetStringField(*TargetObject, TEXT("id")).ToLower();
    }

    const FString CombinedText = (
        FactoryAgentJsonUtils::GetStringField(SuggestionObject, TEXT("problem")) + TEXT(" ") +
        FactoryAgentJsonUtils::GetStringField(SuggestionObject, TEXT("reason")) + TEXT(" ") +
        FactoryAgentJsonUtils::GetStringField(SuggestionObject, TEXT("recommended_action")))
        .ToLower();

    if (CombinedText.Contains(TEXT("durability")) ||
        CombinedText.Contains(TEXT("maintenance")) ||
        CombinedText.Contains(TEXT("broken")) ||
        CombinedText.Contains(TEXT("repair")) ||
        CombinedText.Contains(TEXT("fix")) ||
        CombinedText.Contains(TEXT("inspect")) ||
        CombinedText.Contains(TEXT("고장")) ||
        CombinedText.Contains(TEXT("수리")) ||
        CombinedText.Contains(TEXT("점검")))
    {
        return ETrackedProcessOptimizerIssueType::Durability;
    }

    if (TargetId.Contains(TEXT("powerplant")) &&
        (CombinedText.Contains(TEXT("broken")) ||
         CombinedText.Contains(TEXT("repair")) ||
         CombinedText.Contains(TEXT("inspect"))))
    {
        return ETrackedProcessOptimizerIssueType::Durability;
    }

    if (CombinedText.Contains(TEXT("no_power")) ||
        CombinedText.Contains(TEXT("power shortage")) ||
        CombinedText.Contains(TEXT("power loss")) ||
        CombinedText.Contains(TEXT("전력")) ||
        CombinedText.Contains(TEXT("전원")) ||
        CombinedText.Contains(TEXT("정전")))
    {
        return ETrackedProcessOptimizerIssueType::Power;
    }

    return ETrackedProcessOptimizerIssueType::Unknown;
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

    if (bHasExternalDialogue)
    {
        SetKeyboardFocus();

        if (APlayerController* PC = GetOwningPlayer())
        {
            FInputModeGameAndUI InputModeData;
            InputModeData.SetWidgetToFocus(GetCachedWidget());
            InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            PC->SetInputMode(InputModeData);
            PC->bShowMouseCursor = false;
        }
    }
}

void UUI_DialogueBalloon::ClearExternalDialogue()
{
    if (!bHasExternalDialogue && ExternalDialogueText.IsEmpty()) return;
    bHasExternalDialogue = false;
    ExternalDialogueText.Empty();
    bShowLastSubmittedOperatorGuideQuestion = false;
    RefreshDialogueUI();
}

void UUI_DialogueBalloon::ResetProcessOptimizerTracking()
{
    TrackedProcessOptimizerIssues.Reset();
    bProcessOptimizerRequestInFlight = false;
    bCanAnnounceProcessOptimizerResolution = false;
    bShowLastSubmittedOperatorGuideQuestion = false;

    if (UWorld* World = GetWorld())
    {
        FlushPersistentDebugLines(World);
    }
}

void UUI_DialogueBalloon::BeginProcessOptimizerRequest()
{
    TrackedProcessOptimizerIssues.Reset();
    bProcessOptimizerRequestInFlight = true;
    bCanAnnounceProcessOptimizerResolution = false;
    bShowLastSubmittedOperatorGuideQuestion = false;

    if (UWorld* World = GetWorld())
    {
        FlushPersistentDebugLines(World);
    }

    ShowExternalDialogue(TEXT("공장 상태를 확인하는 중입니다"));
}

void UUI_DialogueBalloon::DisplayCurrentLine()
{
    // ?꾩젽 蹂몄껜 猷⑦듃???덈?濡?Collapsed ?쒗궎吏 ?딄퀬 ??떆 ?대젮?〓땲??
    // 洹몃옒???섎떒??/ ?명뭼 Border 李쎌씠 ?몄젣???낅┰?곸쑝濡???대굹?????덉뒿?덈떎.
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    if (B_OperatorInput) B_OperatorInput->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    if (bHasExternalDialogue)
    {
        if (SB_DialogueData)   SB_DialogueData->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        if (DialogueContainer) DialogueContainer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        if (TXT_Question)
        {
            TXT_Question->SetText(
                bShowLastSubmittedOperatorGuideQuestion && !LastSubmittedOperatorGuideQuestion.TrimStartAndEnd().IsEmpty()
                    ? FText::FromString(FString::Printf(TEXT("> %s"), *LastSubmittedOperatorGuideQuestion))
                    : FText::GetEmpty());
        }
        if (TXT_Dialogue)      TXT_Dialogue->SetText(FText::FromString(ExternalDialogueText));
        UpdateContinuePromptVisibility();
        return;
    }

    if (CachedLines.IsEmpty())
    {
        // ?섏뒪?멸? ?앸굹 ??ш? 鍮꾩뼱?덈떎硫? ?명뭼 李쎌? ?대쾭???먭퀬 
        // 留먰뭾??諛곌꼍(Image_229)怨?湲?먭? ?닿릿 'Size Box ?명듃'留?源붾걫?섍쾶 ?щ챸 泥?냼?⑸땲??
        if (SB_DialogueData)   SB_DialogueData->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        if (DialogueContainer) DialogueContainer->SetVisibility(ESlateVisibility::Hidden);
        if (TXT_Question)      TXT_Question->SetText(FText::GetEmpty());
        if (TXT_Dialogue)      TXT_Dialogue->SetText(FText::GetEmpty());
        UpdateContinuePromptVisibility();
        return;
    }

    // ?쇰컲 ??ш? 議댁옱???뚮뒗 二쇰㉧???명듃瑜??ㅼ떆 ?댁걯寃??몄텧?⑸땲??
    if (SB_DialogueData)   SB_DialogueData->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    if (DialogueContainer) DialogueContainer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    if (TXT_Question)      TXT_Question->SetText(FText::GetEmpty());

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
        && (!CachedLines.Last().Dialogue.Contains(TEXT(". ???뚮윭??李??リ린")));

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
        // AI ?듬????몄텧???곹깭?먯꽌 ?붾㈃??醫뚰겢由?븯硫?湲곗〈 ???濡쒓렇濡?遺?쒕읇寃?蹂듦??⑸땲??
        ClearExternalDialogue(); 
        return FReply::Handled();
    }

    if (!QuestSubsystem || !QuestSubsystem->HasPendingTutorialStartDialogue()) return Reply;
    QuestSubsystem->RevealPendingTutorialStartDialogue();
    return FReply::Handled();
}

FReply UUI_DialogueBalloon::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    FReply Reply = Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
    if (InKeyEvent.GetKey() != EKeys::Enter)
    {
        return Reply;
    }

    if (ET_OperatorInput && ET_OperatorInput->GetVisibility() == ESlateVisibility::Visible)
    {
        return Reply;
    }

    if (!bHasExternalDialogue)
    {
        return Reply;
    }

    ClearExternalDialogue();

    if (APlayerController* PC = GetOwningPlayer())
    {
        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = false;
    }

    return FReply::Handled();
}

void UUI_DialogueBalloon::ToggleAIGuide(APlayerController* PC)
{
    if (!PC || !ET_OperatorInput) return;

    // 1. ?대? AI ?듬???異쒕젰 以묒씤 ?곹깭?쇰㈃ ?먮옒 ??щ줈 濡ㅻ갚
    if (bHasExternalDialogue)
    {
        ClearExternalDialogue();
        ET_OperatorInput->SetVisibility(ESlateVisibility::Collapsed);
        
        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = false;
        return;
    }

    // 2. ?쇰컲 ?곹깭?먯꽌 / ?ㅻ? 泥섏쓬 ?뚮?????(1? ?쒖꽦??蹂댁옣)
    if (ET_OperatorInput->GetVisibility() != ESlateVisibility::Visible)
    {
        SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        ET_OperatorInput->SetVisibility(ESlateVisibility::Visible);
        
        // ?щ젅?댄듃 李??먯껜???낅젰李??꾩젽 ?ъ빱?ㅻ? ?ㅼ씠?됲듃濡?媛뺤젣 二쇱엯?⑸땲??
        FInputModeGameAndUI InputModeData;
        InputModeData.SetWidgetToFocus(ET_OperatorInput->GetCachedWidget());
        InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(InputModeData);
        
        PC->bShowMouseCursor = true;
        ET_OperatorInput->SetText(FText::GetEmpty());
        ET_OperatorInput->SetFocus();
        
        // / ???낅젰 濡쒖쭅???띿뒪???꾨뱶瑜??ㅼ뿼?쒖폒 ?뚰듃?띿뒪?몃? 吏?곕뜕 踰꾧렇瑜?媛뺤젣 ?몄쿃?⑸땲??
    }
    else
    {
        // ?대? 耳쒖졇 ?덈뒗 ?곹깭?먯꽌 ?ㅼ떆 ?꾨Ⅴ硫?痍⑥냼?섍퀬 ?덉텧
        ET_OperatorInput->SetVisibility(ESlateVisibility::Collapsed);
        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = false;
    }
}

