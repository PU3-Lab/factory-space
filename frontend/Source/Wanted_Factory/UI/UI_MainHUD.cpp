#include "UI/UI_MainHUD.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "PlanetEventManagerSubsystem.h"
#include "UI_QuestNotify.h"
#include "UI_QuestWindow.h"

void UUI_MainHUD::NativeConstruct()
{
    Super::NativeConstruct();

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
    const TCHAR* WindLabel = TEXT("Calm");
    if (WeatherState.WindSpeed >= 0.75f)
    {
        WindLabel = TEXT("Storm");
    }
    else if (WeatherState.WindSpeed >= 0.5f)
    {
        WindLabel = TEXT("Strong");
    }
    else if (WeatherState.WindSpeed >= 0.25f)
    {
        WindLabel = TEXT("Breeze");
    }

    const TCHAR* RainLabel = TEXT("Clear");
    if (WeatherState.Rainfall >= 0.75f)
    {
        RainLabel = TEXT("Heavy rain");
    }
    else if (WeatherState.Rainfall >= 0.5f)
    {
        RainLabel = TEXT("Rain");
    }
    else if (WeatherState.Rainfall > 0.0f)
    {
        RainLabel = TEXT("Drizzle");
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
        EventText = FString::Printf(TEXT("Magnetic storm %.0f%%"), Severity * 100.0f);
        EventColor = FLinearColor(0.1f, 0.45f, 0.55f, 0.9f);
    }
    else if (EventType == EPlanetEventType::SandStorm)
    {
        EventText = FString::Printf(TEXT("Sand storm %.0f%%"), Severity * 100.0f);
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