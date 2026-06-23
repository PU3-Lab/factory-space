#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlanetEventManagerSubsystem.h"
#include "UI_MainHUD.generated.h"

UCLASS()
class WANTED_FACTORY_API UUI_MainHUD : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UFUNCTION(BlueprintCallable, Category = "HUD | Quest")
    void ToggleQuestWindow();

    UFUNCTION()
    void OnRequestQuestsClicked();

protected:
    UPROPERTY(meta = (BindWidget))
    class UUI_QuestWindow* WBP_QuestWindow;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* TXT_InGameTime;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* TXT_DisasterDay;

    UPROPERTY(meta = (BindWidgetOptional))
    class UTextBlock* TXT_WindSpeed;

    UPROPERTY(meta = (BindWidgetOptional))
    class UTextBlock* TXT_Rainfall;

    UPROPERTY(meta = (BindWidgetOptional))
    class UBorder* B_PlanetEvent;

    UPROPERTY(meta = (BindWidgetOptional))
    class UTextBlock* TXT_PlanetEvent;

    virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
    UFUNCTION()
    void HandleWeatherChanged(const FPlanetWeatherState& WeatherState);

    void RefreshWeatherText(const FPlanetWeatherState& WeatherState);

    UFUNCTION()
    void HandlePlanetEventStarted(EPlanetEventType EventType, float Severity);

    UFUNCTION()
    void HandlePlanetEventEnded(EPlanetEventType EventType);

    void RefreshPlanetEventUI(EPlanetEventType EventType, float Severity);
};
