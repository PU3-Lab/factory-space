#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QuestManagerSubsystem.h"
#include "Components/EditableText.h"
#include "UI_MainHUD.generated.h"

UCLASS()
class WANTED_FACTORY_API UUI_MainHUD : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    
    UFUNCTION() 
    void OnRequestQuestsClicked();
    
    UFUNCTION() 
    void OnToggleGuideClicked();
    
    UFUNCTION(BlueprintCallable, Category = "HUD | Quest")
    void ToggleQuestWindow(); // J키를 눌렀을 때 호출해 줄 외부 함수

    UFUNCTION(BlueprintCallable, Category = "HUD | Quest")
    void ToggleAIGuideWindow(); // 캐릭터가 Tab키를 눌렀을 때 호출해 줄 외부 함수
    
    void UpdateMainQuestUI(const FQuestState& MainQuest);

protected:
    // --- 퀘스트 창 전체 레이아웃 바인딩 ---
    UPROPERTY(meta = (BindWidget))
    class UVerticalBox* VB_QuestLayout;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
    bool bIsQuestWindowOpen = true;

    UFUNCTION(BlueprintImplementableEvent, Category = "HUD | Anim")
    void K2_PlayQuestAnimation(bool bOpen);
    
    // 메인 퀘스트 전용 구역 상자
    UPROPERTY(meta = (BindWidget))
    class UVerticalBox* VB_MainQuestZone;

    // 서브 퀘스트 전용 구역 상자
    UPROPERTY(meta = (BindWidget))
    class UVerticalBox* VB_SubQuestZone;
    
    // --- 오퍼레이터 가이드 AI 채팅 UI 위젯 ---
    UPROPERTY(meta = (BindWidget))
    class UEditableText* ET_OperatorInput;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* TXT_GuideResponse;
    
    // --- 오퍼레이터 가이드 토글 제어 위젯 ---
    UPROPERTY(meta = (BindWidget))
    class UBorder* B_ChatBackground; 

    UPROPERTY(meta = (BindWidget))
    class UButton* BTN_ToggleGuide; 
    
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* TXT_ToggleText; 

    // --- 퀘스트 정보 위젯 ---
    UPROPERTY(meta = (BindWidget)) 
    class UButton* BTN_RequestQuests;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* TXT_MainQuestTitle;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* TXT_MainQuestDesc;

    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_SubQuest_1;
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_SubQuest_2;
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_SubQuest_3;
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_SubQuest_4;
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_SubQuest_5;
    
    // --- 상단 환경 정보 위젯 ---
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_InGameTime;
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_DisasterDay;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Quest")
    TSubclassOf<class UUI_QuestNotify> QuestNotifyWidgetClass;
private:
    // QuestManagerSubsystem의 OnMainQuestChanged 신호를 받아올 동적 수신기 함수
    UFUNCTION()
    void HandleOnMainQuestChanged(const FQuestState& NewQuest);
    
    UFUNCTION()
    void HandleOnSubQuestsGenerated(const FString& RequestId, const TArray<FQuestState>& Quests);
    
    UFUNCTION()
    void HandleOnSubQuestRequestFailed(const FString& RequestId, const FString& ErrorMessage);
    
    UFUNCTION()
    void HandleOnTextCommitted(const FText& Text, ETextCommit::Type CommitType);

    UFUNCTION()
    void HandleOnOperatorGuideResponse(const FString& RequestId, const FString& Agent, const FString& PayloadJson, const FString& RawMessage);
    
    UFUNCTION()
    void HandleOnOperatorGuideError(
       const FString& RequestId,
       const FString& Agent,
       const FString& ErrorCode,
       const FString& ErrorMessage,
       const FString& RawMessage);
};