#include "UI/UI_MainHUD.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "PlanetEventManagerSubsystem.h"
#include "UI_QuestNotify.h"
#include "UI_QuestWindow.h"

namespace
{
    // [HUD 아이콘] Image에 텍스처 브러시 적용 + 표시. Texture가 null이면 숨김(Collapsed). 분기 추가를 한 줄로
    // 줄이는 단일 헬퍼 — null 체크/표시 토글을 한곳에 모아 RefreshWeatherText/RefreshPlanetEventUI에서 재사용.
    void ApplyHudIcon(UImage* Image, UTexture2D* Texture, const FLinearColor& Color = FLinearColor::White)
    {
        if (!Image)
        {
            return;
        }
        if (Texture)
        {
            Image->SetBrushFromTexture(Texture);
            Image->SetColorAndOpacity(Color); // 표시될 때만 틴트 적용(숨김이면 무의미).
            Image->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        }
        else
        {
            Image->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}

void UUI_MainHUD::NativeConstruct()
{
    Super::NativeConstruct();

    HideSaveIndicator();

    UGameInstance* GI = GetGameInstance();
    UQuestManagerSubsystem* QuestManager = GI ? GI->GetSubsystem<UQuestManagerSubsystem>() : nullptr;

    RefreshQuestWindowMode();
    
    if (QuestManager)
    {
        QuestManager->OnTutorialStepChanged.RemoveDynamic(this, &UUI_MainHUD::HandleTutorialStepChangedForSimpleQuest);
        QuestManager->OnTutorialStepChanged.AddDynamic(this, &UUI_MainHUD::HandleTutorialStepChangedForSimpleQuest);
        QuestManager->OnTutorialDialogueLogged.RemoveDynamic(this, &UUI_MainHUD::HandleTutorialDialogueLoggedForQuestWindow);
        QuestManager->OnTutorialDialogueLogged.AddDynamic(this, &UUI_MainHUD::HandleTutorialDialogueLoggedForQuestWindow);
        RefreshSimpleQuestWindow();
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
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(SaveIndicatorTimerHandle);
    }

    if (GetWorld())
    {
        if (UPlanetEventManagerSubsystem* PlanetManager = GetWorld()->GetSubsystem<UPlanetEventManagerSubsystem>())
        {
            PlanetManager->OnWeatherChanged.RemoveDynamic(this, &UUI_MainHUD::HandleWeatherChanged);
            PlanetManager->OnPlanetEventStarted.RemoveDynamic(this, &UUI_MainHUD::HandlePlanetEventStarted);
            PlanetManager->OnPlanetEventEnded.RemoveDynamic(this, &UUI_MainHUD::HandlePlanetEventEnded);
        }
    }

    if (UGameInstance* GI = GetGameInstance())
    {
        if (UQuestManagerSubsystem* QuestManager = GI->GetSubsystem<UQuestManagerSubsystem>())
        {
            QuestManager->OnTutorialStepChanged.RemoveDynamic(this, &UUI_MainHUD::HandleTutorialStepChangedForSimpleQuest);
            QuestManager->OnTutorialDialogueLogged.RemoveDynamic(this, &UUI_MainHUD::HandleTutorialDialogueLoggedForQuestWindow);
        }
    }

    Super::NativeDestruct();
}

void UUI_MainHUD::RefreshQuestWindowMode()
{
    UGameInstance* GI = GetGameInstance();
    UQuestManagerSubsystem* QuestManager = GI ? GI->GetSubsystem<UQuestManagerSubsystem>() : nullptr;

    const bool bShowFullQuestWindow =
        QuestManager && QuestManager->IsFullQuestWindowUnlocked();

    if (WBP_QuestWindow)
    {
        WBP_QuestWindow->SetVisibility(
            bShowFullQuestWindow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    }

    if (WBP_SimpleQuestWindow)
    {
        WBP_SimpleQuestWindow->SetVisibility(
            bShowFullQuestWindow ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
    }
}

void UUI_MainHUD::RefreshSimpleQuestWindow()
{
    if (!WBP_SimpleQuestWindow)
    {
        return;
    }

    UGameInstance* GI = GetGameInstance();
    UQuestManagerSubsystem* QuestManager = GI ? GI->GetSubsystem<UQuestManagerSubsystem>() : nullptr;
    if (!QuestManager)
    {
        return;
    }

    FTutorialQuestStep CurrentStep;
    if (QuestManager->HasPendingTutorialStartDialogue())
    {
        FString LoggedQuestId;
        FString LoggedTriggerType;
        TArray<FTutorialQuestDialogueLine> LoggedLines;
        QuestManager->GetLastTutorialDialogueLog(LoggedQuestId, LoggedTriggerType, LoggedLines);
        if (LoggedTriggerType == TEXT("on_complete")
            && QuestManager->GetTutorialQuestStepById(LoggedQuestId, CurrentStep))
        {
            WBP_SimpleQuestWindow->UpdateSimpleQuest(
                FText::FromString(CurrentStep.Title),
                FText::FromString(QuestManager->GetTutorialStepDisplayDescription(CurrentStep)));
        }
        return;
    }

    if (!QuestManager->GetCurrentTutorialQuestStep(CurrentStep))
    {
        return;
    }

    WBP_SimpleQuestWindow->UpdateSimpleQuest(
        FText::FromString(CurrentStep.Title),
        FText::FromString(QuestManager->GetTutorialStepDisplayDescription(CurrentStep)));
}

void UUI_MainHUD::HandleTutorialStepChangedForSimpleQuest(const FTutorialQuestStep& Step)
{
    if (!WBP_SimpleQuestWindow)
    {
        return;
    }

    UGameInstance* GI = GetGameInstance();
    UQuestManagerSubsystem* QuestManager = GI ? GI->GetSubsystem<UQuestManagerSubsystem>() : nullptr;
    if (QuestManager && QuestManager->HasPendingTutorialStartDialogue())
    {
        RefreshSimpleQuestWindow();
        RefreshQuestWindowMode();
        return;
    }

    WBP_SimpleQuestWindow->UpdateSimpleQuest(
        FText::FromString(Step.Title),
        FText::FromString(QuestManager ? QuestManager->GetTutorialStepDisplayDescription(Step) : Step.Description));

    RefreshQuestWindowMode();
}

void UUI_MainHUD::HandleTutorialDialogueLoggedForQuestWindow(const FString& QuestId, const FString& TriggerType, const TArray<FTutorialQuestDialogueLine>& Lines)
{
    RefreshQuestWindowMode();
    RefreshSimpleQuestWindow();
}

void UUI_MainHUD::ToggleQuestWindow()
{
    UGameInstance* GI = GetGameInstance();
    UQuestManagerSubsystem* QuestManager = GI ? GI->GetSubsystem<UQuestManagerSubsystem>() : nullptr;

    if (!QuestManager || !QuestManager->IsFullQuestWindowUnlocked())
    {
        return;
    }

    if (WBP_QuestWindow)
    {
        WBP_QuestWindow->ToggleQuestWindow();
    }
}

void UUI_MainHUD::ShowSaveIndicator(float DisplaySeconds)
{
    UWidget* SaveIndicator = ResolveSaveIndicatorWidget();
    if (!SaveIndicator)
    {
        return;
    }

    SaveIndicator->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(SaveIndicatorTimerHandle);
        World->GetTimerManager().SetTimer(
            SaveIndicatorTimerHandle,
            this,
            &UUI_MainHUD::HideSaveIndicator,
            FMath::Max(0.1f, DisplaySeconds),
            false);
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
    const TCHAR* WindLabel = TEXT("고요함");
    UTexture2D* WindTex = WindIcon_Calm;
    FLinearColor WindCol = WindColor_Calm;
    if (WeatherState.WindSpeed >= 0.75f)
    {
        WindLabel = TEXT("폭풍");
        WindTex = WindIcon_Storm;
        WindCol = WindColor_Storm;
    }
    else if (WeatherState.WindSpeed >= 0.5f)
    {
        WindLabel = TEXT("강풍");
        WindTex = WindIcon_Strong;
        WindCol = WindColor_Strong;
    }
    else if (WeatherState.WindSpeed >= 0.25f)
    {
        WindLabel = TEXT("산들바람");
        WindTex = WindIcon_Breeze;
        WindCol = WindColor_Breeze;
    }

    const TCHAR* RainLabel = TEXT("맑음");
    UTexture2D* RainTex = RainfallIcon_Clear;
    FLinearColor RainCol = RainColor_Clear;
    if (WeatherState.Rainfall >= 0.75f)
    {
        RainLabel = TEXT("폭우");
        RainTex = RainfallIcon_Heavy;
        RainCol = RainColor_Heavy;
    }
    else if (WeatherState.Rainfall >= 0.5f)
    {
        RainLabel = TEXT("비");
        RainTex = RainfallIcon_Rain;
        RainCol = RainColor_Rain;
    }
    else if (WeatherState.Rainfall > 0.0f)
    {
        RainLabel = TEXT("이슬비");
        RainTex = RainfallIcon_Drizzle;
        RainCol = RainColor_Drizzle;
    }

    if (TXT_WindSpeed)
    {
        TXT_WindSpeed->SetText(FText::FromString(WindLabel));
    }

    if (TXT_Rainfall)
    {
        TXT_Rainfall->SetText(FText::FromString(RainLabel));
    }

    // [HUD 아이콘] 라벨과 같은 분기에서 고른 텍스처+색을 적용(null이면 숨김·색 무의미). 기존 라벨 로직 무변경.
    ApplyHudIcon(IMG_Wind, WindTex, WindCol);
    ApplyHudIcon(IMG_Rainfall, RainTex, RainCol);

    // [이벤트 캡슐] None 상태면 날씨 변화에 맞춰 위험도 바 갱신. 이벤트 진행 중이면 캡슐은 RefreshPlanetEventUI 소관(PB 숨김 유지).
    if (UWorld* World = GetWorld())
    {
        if (UPlanetEventManagerSubsystem* PlanetManager = World->GetSubsystem<UPlanetEventManagerSubsystem>())
        {
            if (PlanetManager->GetEventState().Type == EPlanetEventType::None)
            {
                RefreshStormRiskBar(WeatherState);
            }
        }
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
    if (!TXT_PlanetEvent && !B_PlanetEvent && !IMG_PlanetEvent && !PB_StormRisk)
    {
        return;
    }

    if (EventType == EPlanetEventType::None)
    {
        // None: 폭풍 아이콘 숨김 + 캡슐 배경 중립 + 위험도 바/텍스트(현재 날씨 기준). (기존엔 None시 캡슐 collapse였으나
        // 이제 위험도 표시를 위해 캡슐 표시 유지 — 토글 캡슐.)
        ApplyHudIcon(IMG_PlanetEvent, nullptr);
        if (B_PlanetEvent)
        {
            B_PlanetEvent->SetBrushColor(FLinearColor(0.15f, 0.15f, 0.15f, 0.75f));
            B_PlanetEvent->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        }

        FPlanetWeatherState Weather;
        if (UWorld* World = GetWorld())
        {
            if (UPlanetEventManagerSubsystem* PlanetManager = World->GetSubsystem<UPlanetEventManagerSubsystem>())
            {
                Weather = PlanetManager->GetWeatherState();
            }
        }
        RefreshStormRiskBar(Weather);
        return;
    }

    // 진행 중 이벤트: 위험도 바 숨김 + 폭풍 아이콘 + severity% + 캡슐색(보라/주황).
    if (PB_StormRisk)
    {
        PB_StormRisk->SetVisibility(ESlateVisibility::Collapsed);
    }

    FString EventText;
    FString SeverityLabel = TEXT("\uC57D\uD568");
    if (Severity >= 0.85f)
    {
        SeverityLabel = TEXT("\uADF9\uC2EC");
    }
    else if (Severity >= 0.6f)
    {
        SeverityLabel = TEXT("\uAC15\uD568");
    }
    else if (Severity >= 0.3f)
    {
        SeverityLabel = TEXT("\uBCF4\uD1B5");
    }
    FLinearColor EventColor(0.15f, 0.15f, 0.15f, 0.9f); // 캡슐 배경(B_PlanetEvent) 색
    UTexture2D* EventTex = nullptr;
    FLinearColor EventIconCol = FLinearColor::White;     // 아이콘(IMG_PlanetEvent) 틴트 — 캡슐색과 별개
    if (EventType == EPlanetEventType::MagneticStorm)
    {
        EventText = FString::Printf(TEXT("자기폭풍 %.0f%%"), Severity * 100.0f);
        EventColor = MagneticStormCapsuleColor;
        EventTex = PlanetEventIcon_Magnetic;
        EventIconCol = PlanetEventColor_Magnetic;
    }
    else if (EventType == EPlanetEventType::SandStorm)
    {
        EventText = FString::Printf(TEXT("모래폭풍 %.0f%%"), Severity * 100.0f);
        EventColor = SandStormCapsuleColor;
        EventTex = PlanetEventIcon_Sand;
        EventIconCol = PlanetEventColor_Sand;
    }

    if (EventType == EPlanetEventType::MagneticStorm)
    {
        EventText = FString::Printf(TEXT("\uC790\uAE30\uD3ED\uD48D %s"), *SeverityLabel);
    }
    else if (EventType == EPlanetEventType::SandStorm)
    {
        EventText = FString::Printf(TEXT("\uBAA8\uB798\uD3ED\uD48D %s"), *SeverityLabel);
    }

    if (TXT_PlanetEvent)
    {
        TXT_PlanetEvent->SetText(FText::FromString(EventText));
    }
    if (B_PlanetEvent)
    {
        B_PlanetEvent->SetBrushColor(EventColor);
        B_PlanetEvent->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }
    ApplyHudIcon(IMG_PlanetEvent, EventTex, EventIconCol);
}

void UUI_MainHUD::RefreshStormRiskBar(const FPlanetWeatherState& WeatherState)
{
    // [이벤트 캡슐] None 상태 전용 — 모래폭풍 발생 확률(매니저 상수)로 위험도 바/텍스트. 시간 누적 없음(날씨 전환 시 점프).
    // chance = Base + (풍속>=Threshold && 강수<=Max ? Bonus : 0). 보너스 충족 = 높음(20%→High) / 아니면 낮음(5%→Low).
    bool bHighRisk = false;
    if (UWorld* World = GetWorld())
    {
        if (UPlanetEventManagerSubsystem* PlanetManager = World->GetSubsystem<UPlanetEventManagerSubsystem>())
        {
            bHighRisk = WeatherState.WindSpeed >= PlanetManager->SandStormWindThreshold
                && WeatherState.Rainfall <= PlanetManager->SandStormRainfallMax;
        }
    }

    if (PB_StormRisk)
    {
        PB_StormRisk->SetPercent(bHighRisk ? StormRiskHighPercent : StormRiskLowPercent);
        PB_StormRisk->SetFillColorAndOpacity(bHighRisk ? StormRiskHighColor : StormRiskLowColor);
        PB_StormRisk->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }
    if (TXT_PlanetEvent)
    {
        TXT_PlanetEvent->SetText(FText::FromString(bHighRisk ? TEXT("모래폭풍 위험: 높음") : TEXT("모래폭풍 위험: 낮음")));
    }
}

UWidget* UUI_MainHUD::ResolveSaveIndicatorWidget()
{
    if (UWidget* CachedWidget = CachedSaveIndicatorWidget.Get())
    {
        return CachedWidget;
    }

    static const FName CandidateNames[] = {
        TEXT("IMG_Save"),
        TEXT("IMG_SaveIcon"),
        TEXT("IMG_Saving"),
        TEXT("SaveIcon"),
        TEXT("SavingIcon"),
        TEXT("WBP_SaveIcon")
    };

    for (const FName CandidateName : CandidateNames)
    {
        if (UWidget* CandidateWidget = GetWidgetFromName(CandidateName))
        {
            CachedSaveIndicatorWidget = CandidateWidget;
            CandidateWidget->SetVisibility(ESlateVisibility::Collapsed);
            return CandidateWidget;
        }
    }

    return nullptr;
}

void UUI_MainHUD::HideSaveIndicator()
{
    if (UWidget* SaveIndicator = ResolveSaveIndicatorWidget())
    {
        SaveIndicator->SetVisibility(ESlateVisibility::Collapsed);
    }
}

FReply UUI_MainHUD::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::U)
    {
        if (WBP_QuestWindow && WBP_QuestWindow->QuestNotifyWidgetClass && GetOwningPlayer())
        {
            UUI_QuestNotify* SubNotify = CreateWidget<UUI_QuestNotify>(GetOwningPlayer(), WBP_QuestWindow->QuestNotifyWidgetClass);
            if (SubNotify)
            {
                SubNotify->AddToViewport(100);
                SubNotify->PlayNotify(TEXT("[Sub] Bring 10 iron ingots"), TEXT("Iron ingot x5, Copper ore x20, Credit +15"));
                UE_LOG(LogTemp, Log, TEXT("[Quest] Forced sub quest notify test succeeded."));
            }
        }

        return FReply::Handled();
    }

    return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UUI_MainHUD::OnRequestQuestsClicked()
{
    if (WBP_QuestWindow)
    {
        WBP_QuestWindow->OnRequestQuestsClicked();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[HUD] WBP_QuestWindow instance is missing."));
    }
}
void UUI_MainHUD::CloseQuestWindow()
{
    if (WBP_QuestWindow)
    {
        WBP_QuestWindow->CloseQuestWindow();
    }
}
