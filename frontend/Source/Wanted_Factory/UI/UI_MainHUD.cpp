#include "UI/UI_MainHUD.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Engine/GameInstance.h"
#include "PlanetEventManagerSubsystem.h"
#include "FactoryAgentClientSubsystem.h" 
#include "FactoryAgentJsonUtils.h"       
#include "Dom/JsonObject.h"
#include "UI_QuestWindow.h" 
#include "UI_QuestNotify.h"

void UUI_MainHUD::NativeConstruct()
{
    Super::NativeConstruct();
    
    if (BTN_ToggleGuide) BTN_ToggleGuide->OnClicked.AddDynamic(this, &UUI_MainHUD::OnToggleGuideClicked);
    if (ET_OperatorInput) ET_OperatorInput->OnTextCommitted.AddDynamic(this, &UUI_MainHUD::HandleOnTextCommitted);

    UGameInstance* GameInstance = GetGameInstance();
    if (GameInstance)
    {
        UFactoryAgentClientSubsystem* AgentClient = GameInstance->GetSubsystem<UFactoryAgentClientSubsystem>();
        if (AgentClient)
        {
            AgentClient->OnAgentResponseReceived.AddDynamic(this, &UUI_MainHUD::HandleOnOperatorGuideResponse);
            AgentClient->OnAgentErrorReceived.AddDynamic(this, &UUI_MainHUD::HandleOnOperatorGuideError);
        }
    }

    if (GetWorld())
    {
        UPlanetEventManagerSubsystem* PlanetManager = GetWorld()->GetSubsystem<UPlanetEventManagerSubsystem>();
        if (PlanetManager)
        {
            PlanetManager->OnWeatherChanged.AddDynamic(this, &UUI_MainHUD::HandleWeatherChanged);
            RefreshWeatherText(PlanetManager->GetWeatherState());
        }
    }
}

void UUI_MainHUD::NativeDestruct()
{
    if (GetWorld())
    {
        UPlanetEventManagerSubsystem* PlanetManager = GetWorld()->GetSubsystem<UPlanetEventManagerSubsystem>();
        if (PlanetManager)
        {
            PlanetManager->OnWeatherChanged.RemoveDynamic(this, &UUI_MainHUD::HandleWeatherChanged);
        }
    }

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UFactoryAgentClientSubsystem* AgentClient = GameInstance->GetSubsystem<UFactoryAgentClientSubsystem>())
        {
            AgentClient->OnAgentResponseReceived.RemoveDynamic(this, &UUI_MainHUD::HandleOnOperatorGuideResponse);
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

void UUI_MainHUD::HandleWeatherChanged(const FPlanetWeatherState& WeatherState)
{
    RefreshWeatherText(WeatherState);
}

void UUI_MainHUD::RefreshWeatherText(const FPlanetWeatherState& WeatherState)
{
    const TCHAR* WindLabel = TEXT("고요함");
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

void UUI_MainHUD::OnToggleGuideClicked() { ToggleAIGuideWindow(); }

void UUI_MainHUD::ToggleAIGuideWindow()
{
    if (!B_ChatBackground) return;
    APlayerController* PC = GetOwningPlayer();

    if (B_ChatBackground->GetVisibility() == ESlateVisibility::Collapsed)
    {
        B_ChatBackground->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        if (TXT_ToggleText) TXT_ToggleText->SetText(FText::FromString(TEXT("▲ Tab 열기")));
        if (PC) { PC->SetInputMode(FInputModeGameAndUI()); PC->SetShowMouseCursor(true); }
        if (ET_OperatorInput) ET_OperatorInput->SetFocus();
    }
    else
    {
        B_ChatBackground->SetVisibility(ESlateVisibility::Collapsed);
        if (TXT_ToggleText) TXT_ToggleText->SetText(FText::FromString(TEXT("▼ Tab 닫기")));
        if (PC) { PC->SetInputMode(FInputModeGameOnly()); PC->SetShowMouseCursor(false); }
    }
}

bool UUI_MainHUD::IsGuideWindowOpen() const
{
    return B_ChatBackground && B_ChatBackground->GetVisibility() != ESlateVisibility::Collapsed;
}

FReply UUI_MainHUD::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Tab) { ToggleAIGuideWindow(); return FReply::Handled(); }
    if (InKeyEvent.GetKey() == EKeys::U)
    {
        if (WBP_QuestWindow && WBP_QuestWindow->QuestNotifyWidgetClass && GetOwningPlayer())
        {
            // 정석대로 자식의 변수를 사용해 생성합니다.
            UUI_QuestNotify* SubNotify = CreateWidget<UUI_QuestNotify>(GetOwningPlayer(), WBP_QuestWindow->QuestNotifyWidgetClass);
            if (SubNotify)
            {
                SubNotify->AddToViewport(100);
                
                SubNotify->PlayNotify(
                    TEXT("[서브] 철 주괴 10개 가공 및 제련"), 
                    TEXT("철 주괴 x5, 구리 광석 x20, 신뢰도 +15")
                );
                
                UE_LOG(LogTemp, Log, TEXT("[치트키] 서브 퀘스트 완료 팝업 강제 구동 성공"));
            }
        }
        return FReply::Handled();
    }

    return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UUI_MainHUD::HandleOnTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{
    if (CommitType != ETextCommit::OnEnter) return;
    const FString QuestionStr = Text.ToString().TrimStartAndEnd();
    if (QuestionStr.IsEmpty()) return;

    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    UFactoryAgentClientSubsystem* AgentClient = GI->GetSubsystem<UFactoryAgentClientSubsystem>();
    if (!AgentClient) return;

    if (!AgentClient->SendOperatorGuideQuestion(QuestionStr, TEXT("unreal-ui-001")))
    {
        if (TXT_GuideResponse) TXT_GuideResponse->SetText(FText::FromString(TEXT("AI 가이드 요청 전송에 실패했습니다.")));
        return;
    }

    ET_OperatorInput->SetText(FText::GetEmpty());
    if (TXT_GuideResponse) TXT_GuideResponse->SetText(FText::FromString(TEXT("분석 중...")));
}

void UUI_MainHUD::HandleOnOperatorGuideResponse(const FString& RequestId, const FString& Agent, const FString& PayloadJson, const FString& RawMessage)
{
    if (Agent != TEXT("operator_guide")) return;
    TSharedPtr<FJsonObject> PayloadObject;
    if (FactoryAgentJsonUtils::ParseJsonObject(PayloadJson, PayloadObject) && PayloadObject.IsValid())
    {
        FString Answer;
        if (PayloadObject->TryGetStringField(TEXT("final_answer"), Answer) || PayloadObject->TryGetStringField(TEXT("answer"), Answer) || PayloadObject->TryGetStringField(TEXT("text"), Answer))
        {
            if (TXT_GuideResponse) TXT_GuideResponse->SetText(FText::FromString(Answer));
        }
    }
}

void UUI_MainHUD::HandleOnOperatorGuideError(const FString& RequestId, const FString& Agent, const FString& ErrorCode, const FString& ErrorMessage, const FString& RawMessage)
{
    if (Agent != TEXT("operator_guide")) return;
    if (TXT_GuideResponse)
    {
        FString CombinedMessage = ErrorCode.IsEmpty() ? ErrorMessage : FString::Printf(TEXT("%s: %s"), *ErrorCode, *ErrorMessage);
        TXT_GuideResponse->SetText(FText::FromString(CombinedMessage));
    }
}

void UUI_MainHUD::OnRequestQuestsClicked()
{
    // 내부에 심어진 진짜 퀘스트 창의 함수를 대신 실행합니다.
    if (WBP_QuestWindow)
    {
        WBP_QuestWindow->OnRequestQuestsClicked();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[HUD 에러] 플레이어가 서브 퀘스트를 요청했으나 WBP_QuestWindow 인스턴스가 누락되었습니다."));
    }
}
