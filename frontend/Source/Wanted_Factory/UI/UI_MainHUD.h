#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/EditableText.h"
#include "UI_MainHUD.generated.h"

UCLASS()
class WANTED_FACTORY_API UUI_MainHUD : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    
    UFUNCTION() void OnToggleGuideClicked();
    
    // 캐릭터 컴포넌트 입력 이벤트와 매칭될 통로 인터페이스 유지
    UFUNCTION(BlueprintCallable, Category = "HUD | Quest")
    void ToggleQuestWindow(); 

    UFUNCTION(BlueprintCallable, Category = "HUD | Quest")
    void ToggleAIGuideWindow();
    
    UFUNCTION(BlueprintPure, Category = "HUD")
    bool IsGuideWindowOpen() const;

    void UpdateMainQuestUI(const FQuestState& MainQuest);
    UFUNCTION()
    void OnRequestQuestsClicked();

protected:
    // 🌟 [핵심 코드 압축] 퀘스트 관련된 모든 처리는 이 자식 컴포넌트 위젯 위임합니다.
    UPROPERTY(meta = (BindWidget))
    class UUI_QuestWindow* WBP_QuestWindow;

    // --- 오퍼레이터 가이드 AI 채팅 UI 위젯 ---
    UPROPERTY(meta = (BindWidget)) class UEditableText* ET_OperatorInput;
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_GuideResponse;
    UPROPERTY(meta = (BindWidget)) class UBorder* B_ChatBackground; 
    UPROPERTY(meta = (BindWidget)) class UButton* BTN_ToggleGuide; 
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_ToggleText; 

    // --- 상단 환경 정보 위젯 ---
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_InGameTime;
    UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_DisasterDay;
    
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

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
private:
    // AI 토글 관련 핸들러만 깔끔하게 보존
    UFUNCTION() void HandleOnTextCommitted(const FText& Text, ETextCommit::Type CommitType);
    UFUNCTION() void HandleOnOperatorGuideResponse(const FString& RequestId, const FString& Agent, const FString& PayloadJson, const FString& RawMessage);
    UFUNCTION() void HandleOnOperatorGuideError(const FString& RequestId, const FString& Agent, const FString& ErrorCode, const FString& ErrorMessage, const FString& RawMessage);
};
