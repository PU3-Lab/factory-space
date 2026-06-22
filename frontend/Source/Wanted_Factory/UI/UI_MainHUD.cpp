#include "UI/UI_MainHUD.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "FactoryAgentClientSubsystem.h"
#include "FactoryAgentJsonUtils.h"
#include "PlanetEventManagerSubsystem.h"
#include "UI_DialogueBalloon.h"
#include "UI_QuestNotify.h"
#include "UI_QuestWindow.h"

void UUI_MainHUD::NativeConstruct()
{
    Super::NativeConstruct();

    if (BTN_ToggleGuide)
    {
        BTN_ToggleGuide->OnClicked.AddDynamic(this, &UUI_MainHUD::OnToggleGuideClicked);
    }

    if (ET_OperatorInput)
    {
        ET_OperatorInput->OnTextCommitted.AddDynamic(this, &UUI_MainHUD::HandleOnTextCommitted);
    }

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UFactoryAgentClientSubsystem* AgentClient = GameInstance->GetSubsystem<UFactoryAgentClientSubsystem>())
        {
            AgentClient->OnAgentResponseReceived.AddDynamic(this, &UUI_MainHUD::HandleOnOperatorGuideResponse);
            AgentClient->OnAgentProgressReceived.AddDynamic(this, &UUI_MainHUD::HandleOnOperatorGuideProgress);
            AgentClient->OnAgentErrorReceived.AddDynamic(this, &UUI_MainHUD::HandleOnOperatorGuideError);
        }
    }

    if (GetWorld())
    {
        if (UPlanetEventManagerSubsystem* PlanetManager = GetWorld()->GetSubsystem<UPlanetEventManagerSubsystem>())
        {
            PlanetManager->OnWeatherChanged.AddDynamic(this, &UUI_MainHUD::HandleWeatherChanged);
            PlanetManager->OnPlanetEventStarted.AddDynamic(this, &UUI_MainHUD::HandlePlanetEventStarted);
            PlanetManager->OnPlanetEventEnded.AddDynamic(this, &UUI_MainHUD::HandlePlanetEventEnded);
            RefreshWeatherText(PlanetManager->GetWeatherState());
            RefreshPlanetEventUI(PlanetManager->GetEventState().Type, PlanetManager->GetEventState().Severity);
        }
    }
}

void UUI_MainHUD::NativeDestruct()
{
    if (GetWorld())
    {
        if (UPlanetEventManagerSubsystem* PlanetManager = GetWorld()->GetSubsystem<UPlanetEventManagerSubsystem>())
        {
            PlanetManager->OnWeatherChanged.RemoveDynamic(this, &UUI_MainHUD::HandleWeatherChanged);
            PlanetManager->OnPlanetEventStarted.RemoveDynamic(this, &UUI_MainHUD::HandlePlanetEventStarted);
            PlanetManager->OnPlanetEventEnded.RemoveDynamic(this, &UUI_MainHUD::HandlePlanetEventEnded);
        }
    }

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UFactoryAgentClientSubsystem* AgentClient = GameInstance->GetSubsystem<UFactoryAgentClientSubsystem>())
        {
            AgentClient->OnAgentResponseReceived.RemoveDynamic(this, &UUI_MainHUD::HandleOnOperatorGuideResponse);
            AgentClient->OnAgentProgressReceived.RemoveDynamic(this, &UUI_MainHUD::HandleOnOperatorGuideProgress);
            AgentClient->OnAgentErrorReceived.RemoveDynamic(this, &UUI_MainHUD::HandleOnOperatorGuideError);
        }
    }

    Super::NativeDestruct();
}

void UUI_MainHUD::ToggleQuestWindow()
{
    if (WBP_QuestWindow)
    {
        WBP_QuestWindow->ToggleQuestWindow();
    }
}

void UUI_MainHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!GetWorld())
    {
        return;
    }

    if (UPlanetEventManagerSubsystem* PlanetManager = GetWorld()->GetSubsystem<UPlanetEventManagerSubsystem>())
    {
        if (TXT_DisasterDay)
        {
            const int32 CurrentDay = PlanetManager->GetCurrentDayIndex();
            TXT_DisasterDay->SetText(FText::FromString(FString::Printf(TEXT("DAY %02d"), CurrentDay)));
        }

        if (TXT_InGameTime)
        {
            TXT_InGameTime->SetText(FText::FromString(PlanetManager->GetCurrentTime24String()));
        }
    }
}

void UUI_MainHUD::HandleWeatherChanged(const FPlanetWeatherState& WeatherState)
{
    RefreshWeatherText(WeatherState);
}

void UUI_MainHUD::RefreshWeatherText(const FPlanetWeatherState& WeatherState)
{
    const TCHAR* WindLabel = TEXT("고요");
    if (WeatherState.WindSpeed >= 0.75f)
    {
        WindLabel = TEXT("폭풍");
    }
    else if (WeatherState.WindSpeed >= 0.5f)
    {
        WindLabel = TEXT("강풍");
    }
    else if (WeatherState.WindSpeed >= 0.25f)
    {
        WindLabel = TEXT("산들바람");
    }

    const TCHAR* RainLabel = TEXT("맑음");
    if (WeatherState.Rainfall >= 0.75f)
    {
        RainLabel = TEXT("폭우");
    }
    else if (WeatherState.Rainfall >= 0.5f)
    {
        RainLabel = TEXT("비");
    }
    else if (WeatherState.Rainfall > 0.0f)
    {
        RainLabel = TEXT("이슬비");
    }

    if (TXT_WindSpeed)
    {
        TXT_WindSpeed->SetText(FText::FromString(WindLabel));
    }

    if (TXT_Rainfall)
    {
        TXT_Rainfall->SetText(FText::FromString(RainLabel));
    }
}

void UUI_MainHUD::HandlePlanetEventStarted(EPlanetEventType EventType, float Severity)
{
    RefreshPlanetEventUI(EventType, Severity);
}

void UUI_MainHUD::HandlePlanetEventEnded(EPlanetEventType EventType)
{
    RefreshPlanetEventUI(EPlanetEventType::None, 0.0f);
}

void UUI_MainHUD::RefreshPlanetEventUI(EPlanetEventType EventType, float Severity)
{
    if (!TXT_PlanetEvent && !B_PlanetEvent)
    {
        return;
    }

    FString EventText;
    FLinearColor EventColor(0.15f, 0.15f, 0.15f, 0.75f);
    if (EventType == EPlanetEventType::MagneticStorm)
    {
        EventText = FString::Printf(TEXT("자기 폭풍 발생 %.0f%%"), Severity * 100.0f);
        EventColor = FLinearColor(0.1f, 0.45f, 0.55f, 0.9f);
    }
    else if (EventType == EPlanetEventType::SandStorm)
    {
        EventText = FString::Printf(TEXT("모래 폭풍 발생 %.0f%%"), Severity * 100.0f);
        EventColor = FLinearColor(0.55f, 0.34f, 0.12f, 0.9f);
    }

    if (TXT_PlanetEvent)
    {
        TXT_PlanetEvent->SetText(FText::FromString(EventText));
    }

    if (B_PlanetEvent)
    {
        B_PlanetEvent->SetBrushColor(EventColor);
        B_PlanetEvent->SetVisibility(EventType == EPlanetEventType::None ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
    }
}

void UUI_MainHUD::OnToggleGuideClicked()
{
    ToggleAIGuideWindow();
}

void UUI_MainHUD::ToggleAIGuideWindow()
{
    if (!B_ChatBackground)
    {
        return;
    }

    APlayerController* PC = GetOwningPlayer();
    if (B_ChatBackground->GetVisibility() == ESlateVisibility::Collapsed)
    {
        B_ChatBackground->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        if (TXT_ToggleText)
        {
            TXT_ToggleText->SetText(FText::FromString(TEXT("QnA 닫기")));
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
            TXT_ToggleText->SetText(FText::FromString(TEXT("QnA 열기")));
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
    if (InKeyEvent.GetKey() == EKeys::Slash)
    {
        if (InKeyEvent.IsShiftDown())
        {
            return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
        }

        if (IsGuideWindowOpen())
        {
            ClearGuideDialogueBalloon();
            return FReply::Handled();
        }

        ToggleAIGuideWindow();
        return FReply::Handled();
    }

    if (InKeyEvent.GetKey() == EKeys::U)
    {
        if (WBP_QuestWindow && WBP_QuestWindow->QuestNotifyWidgetClass && GetOwningPlayer())
        {
            UUI_QuestNotify* SubNotify = CreateWidget<UUI_QuestNotify>(GetOwningPlayer(), WBP_QuestWindow->QuestNotifyWidgetClass);
            if (SubNotify)
            {
                SubNotify->AddToViewport(100);
                SubNotify->PlayNotify(TEXT("[서브] 철 주괴 10개 가져오기"), TEXT("철 주괴 x5, 구리 광석 x20, 크레딧 +15"));
                UE_LOG(LogTemp, Log, TEXT("[퀘스트] 서브 퀘스트 알림 테스트 강제 구동 성공"));
            }
        }

        return FReply::Handled();
    }

    return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UUI_MainHUD::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    FReply Reply = Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);

    if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    {
        return Reply;
    }

    if (!IsGuideWindowOpen() || !B_ChatBackground)
    {
        return Reply;
    }

    const FVector2D MousePosition = InMouseEvent.GetScreenSpacePosition();
    if (!B_ChatBackground->GetCachedGeometry().IsUnderLocation(MousePosition))
    {
        return Reply;
    }

    ClearGuideDialogueBalloon();
    return Reply;
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

    UGameInstance* GI = GetGameInstance();
    if (!GI)
    {
        return;
    }

    UFactoryAgentClientSubsystem* AgentClient = GI->GetSubsystem<UFactoryAgentClientSubsystem>();
    if (!AgentClient)
    {
        return;
    }

    if (!AgentClient->SendOperatorGuideQuestion(QuestionStr, TEXT("unreal-client")))
    {
        const FString ErrorMessage = TEXT("AI 가이드 요청 전송에 실패했습니다.");
        if (TXT_GuideResponse)
        {
            TXT_GuideResponse->SetText(FText::FromString(ErrorMessage));
        }

        ShowGuideResponseInDialogueBalloon(ErrorMessage);
        return;
    }

    ET_OperatorInput->SetText(FText::GetEmpty());
    if (TXT_GuideResponse)
    {
        TXT_GuideResponse->SetText(FText::FromString(TEXT("분석 중...")));
    }

    ShowGuideResponseInDialogueBalloon(TEXT("분석 중..."));

    if (IsGuideWindowOpen())
    {
        ToggleAIGuideWindow();
    }
}

void UUI_MainHUD::HandleOnOperatorGuideResponse(const FString& RequestId, const FString& Agent, const FString& PayloadJson, const FString& RawMessage)
{
    if (Agent != TEXT("operator_guide"))
    {
        return;
    }

    TSharedPtr<FJsonObject> PayloadObject;
    if (!FactoryAgentJsonUtils::ParseJsonObject(PayloadJson, PayloadObject) || !PayloadObject.IsValid())
    {
        return;
    }

    FString Answer;
    if (PayloadObject->TryGetStringField(TEXT("final_answer"), Answer)
        || PayloadObject->TryGetStringField(TEXT("answer"), Answer)
        || PayloadObject->TryGetStringField(TEXT("text"), Answer))
    {
        if (TXT_GuideResponse)
        {
            TXT_GuideResponse->SetText(FText::FromString(Answer));
        }

        ShowGuideResponseInDialogueBalloon(Answer);
    }
}

void UUI_MainHUD::HandleOnOperatorGuideProgress(const FString& RequestId, const FString& Agent, const FString& Stage, const FString& Message, const FString& RawMessage)
{
    if (Agent != TEXT("operator_guide"))
    {
        return;
    }

    const FString ProgressMessage = Message.TrimStartAndEnd();
    if (ProgressMessage.IsEmpty())
    {
        return;
    }

    if (TXT_GuideResponse)
    {
        TXT_GuideResponse->SetText(FText::FromString(ProgressMessage));
    }

    ShowGuideResponseInDialogueBalloon(ProgressMessage);
}

void UUI_MainHUD::HandleOnOperatorGuideError(const FString& RequestId, const FString& Agent, const FString& ErrorCode, const FString& ErrorMessage, const FString& RawMessage)
{
    if (Agent != TEXT("operator_guide"))
    {
        return;
    }

    const FString CombinedMessage = TEXT("AI에 문제가 생겨 응답이 불가능합니다.");
    if (TXT_GuideResponse)
    {
        TXT_GuideResponse->SetText(FText::FromString(CombinedMessage));
    }

    ShowGuideResponseInDialogueBalloon(CombinedMessage);
}

UUI_DialogueBalloon* UUI_MainHUD::FindDialogueBalloon() const
{
    if (!GetWorld())
    {
        return nullptr;
    }

    TArray<UUserWidget*> FoundWidgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), FoundWidgets, UUI_DialogueBalloon::StaticClass(), false);
    if (FoundWidgets.IsEmpty())
    {
        return nullptr;
    }

    return Cast<UUI_DialogueBalloon>(FoundWidgets[0]);
}

void UUI_MainHUD::ShowGuideResponseInDialogueBalloon(const FString& Message) const
{
    if (UUI_DialogueBalloon* DialogueBalloon = FindDialogueBalloon())
    {
        DialogueBalloon->ShowExternalDialogue(Message);
    }
}

void UUI_MainHUD::ClearGuideDialogueBalloon() const
{
    if (UUI_DialogueBalloon* DialogueBalloon = FindDialogueBalloon())
    {
        DialogueBalloon->ClearExternalDialogue();
    }
}

void UUI_MainHUD::OnRequestQuestsClicked()
{
    if (WBP_QuestWindow)
    {
        WBP_QuestWindow->OnRequestQuestsClicked();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[HUD] WBP_QuestWindow 인스턴스가 없습니다."));
    }
}
