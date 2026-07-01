#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QuestManagerSubsystem.h"
#include "UI_QuestWindow.generated.h"

UCLASS()
class WANTED_FACTORY_API UUI_QuestWindow : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    // J키를 누르거나 HUD에서 제어할 때 호출될 전용 토글 셔터
    void ToggleQuestWindow();
    
    void UpdateMainQuestUI(const FQuestState& MainQuest);
    
    UFUNCTION() 
    void OnRequestQuestsClicked();
    void CloseQuestWindow();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    TSubclassOf<class UUI_QuestNotify> QuestNotifyWidgetClass;

protected:
    // --- 퀘스트 레이아웃 컴포넌트 바인딩 ---
    UPROPERTY(meta = (BindWidget)) class UVerticalBox* VB_MainQuestZone;
    UPROPERTY(meta = (BindWidget)) class UVerticalBox* VB_SubQuestZone;
    UPROPERTY(meta = (BindWidget)) class UBorder* B_QuestBg;

    // --- 텍스트 컴포넌트 바인딩 ---
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_MainQuestTitle;
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_MainQuestDesc;
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_NoSubQuestsPlaceholder;
    UPROPERTY(meta = (BindWidget)) class URichTextBlock* TXT_SubQuest_1;
    UPROPERTY(meta = (BindWidget)) class URichTextBlock* TXT_SubQuest_2;
    UPROPERTY(meta = (BindWidget)) class URichTextBlock* TXT_SubQuest_3;
    UPROPERTY(meta = (BindWidget)) class URichTextBlock* TXT_SubQuest_4;
    UPROPERTY(meta = (BindWidget)) class URichTextBlock* TXT_SubQuest_5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    bool bIsQuestWindowOpen = true;

    // 블루프린트에서 연출할 애니메이션 이벤트 인터페이스
    UFUNCTION(BlueprintImplementableEvent, Category = "Quest | Anim")
    void K2_PlayQuestAnimation(bool bOpen);
private:
    void DisplayTutorialStep(const FTutorialQuestStep& Step);
    void UpdateQuestZoneVisibility();
    void UpdateSubQuestZoneVisibility();
    void UpdateSubQuestTexts(const TArray<FQuestState>& Quests);
    void CacheSubQuestStatuses(const TArray<FQuestState>& Quests);
    void ShowSubQuestCompletedNotify(const FQuestState& Quest);

    UFUNCTION()
    void HandleOnTutorialStepChanged(const FTutorialQuestStep& NewStep);

    UFUNCTION()
    void HandleOnTutorialDialogueLogged(const FString& QuestId, const FString& TriggerType, const TArray<FTutorialQuestDialogueLine>& Lines);

    UFUNCTION() void HandleOnMainQuestChanged(const FQuestState& NewQuest);
    UFUNCTION() void HandleOnSubQuestsGenerated(const FString& RequestId, const TArray<FQuestState>& Quests);
    UFUNCTION() void HandleOnSubQuestsUpdated(const TArray<FQuestState>& Quests);
    UFUNCTION() void HandleOnSubQuestRequestFailed(const FString& RequestId, const FString& ErrorMessage);

    TMap<FString, EQuestStatus> CachedSubQuestStatuses;
};
