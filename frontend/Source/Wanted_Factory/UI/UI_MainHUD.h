#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/EditableText.h"
#include "QuestManagerSubsystem.h"
#include "UI_MainHUD.generated.h"

UCLASS()
class WANTED_FACTORY_API UUI_MainHUD : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

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

    virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
    UFUNCTION()
    void HandleOnTextCommitted(const FText& Text, ETextCommit::Type CommitType);

    UFUNCTION()
    void HandleOnOperatorGuideResponse(const FString& RequestId, const FString& Agent, const FString& PayloadJson, const FString& RawMessage);

    UFUNCTION()
    void HandleOnOperatorGuideError(const FString& RequestId, const FString& Agent, const FString& ErrorCode, const FString& ErrorMessage, const FString& RawMessage);
};
