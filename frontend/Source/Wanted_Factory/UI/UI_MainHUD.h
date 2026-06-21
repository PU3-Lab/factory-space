#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/EditableText.h"
#include "PlanetEventManagerSubsystem.h"
#include "QuestManagerSubsystem.h"
#include "UI_MainHUD.generated.h"

UCLASS()
class WANTED_FACTORY_API UUI_MainHUD : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    UFUNCTION()
    void OnToggleGuideClicked();

    UFUNCTION(BlueprintCallable, Category = "HUD | Quest")
    void ToggleQuestWindow();

    UFUNCTION(BlueprintCallable, Category = "HUD | Quest")
    void ToggleAIGuideWindow();

    UFUNCTION(BlueprintPure, Category = "HUD")
    bool IsGuideWindowOpen() const;

    UFUNCTION()
    void OnRequestQuestsClicked();

protected:
    UPROPERTY(meta = (BindWidget))
    class UUI_QuestWindow* WBP_QuestWindow;

    UPROPERTY(meta = (BindWidget))
    class UEditableText* ET_OperatorInput;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* TXT_GuideResponse;

    UPROPERTY(meta = (BindWidget))
    class UBorder* B_ChatBackground;

    UPROPERTY(meta = (BindWidget))
    class UButton* BTN_ToggleGuide;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* TXT_ToggleText;

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
    class UUI_DialogueBalloon* FindDialogueBalloon() const;
    void ShowGuideResponseInDialogueBalloon(const FString& Message) const;
    void ClearGuideDialogueBalloon() const;

    UFUNCTION()
    void HandleWeatherChanged(const FPlanetWeatherState& WeatherState);

    void RefreshWeatherText(const FPlanetWeatherState& WeatherState);

    UFUNCTION()
    void HandlePlanetEventStarted(EPlanetEventType EventType, float Severity);

    UFUNCTION()
    void HandlePlanetEventEnded(EPlanetEventType EventType);

    void RefreshPlanetEventUI(EPlanetEventType EventType, float Severity);

    UFUNCTION()
    void HandleOnTextCommitted(const FText& Text, ETextCommit::Type CommitType);

    UFUNCTION()
    void HandleOnOperatorGuideResponse(const FString& RequestId, const FString& Agent, const FString& PayloadJson, const FString& RawMessage);

    UFUNCTION()
    void HandleOnOperatorGuideProgress(const FString& RequestId, const FString& Agent, const FString& Stage, const FString& Message, const FString& RawMessage);

    UFUNCTION()
    void HandleOnOperatorGuideError(const FString& RequestId, const FString& Agent, const FString& ErrorCode, const FString& ErrorMessage, const FString& RawMessage);
};
