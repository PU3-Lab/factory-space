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
	
	UFUNCTION() void OnRequestQuestsClicked();
	UFUNCTION() void OnToggleGuideClicked();
protected:
	// --- 오퍼레이터 가이드 AI 채팅 UI 위젯 ---
	UPROPERTY(meta = (BindWidget))
	class UEditableText* ET_OperatorInput;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* TXT_GuideResponse;
	
	// --- 오퍼레이터 가이드 토글 제어 위젯 ---
	UPROPERTY(meta = (BindWidget))
	class UBorder* B_ChatBackground; // 열고 닫힐 채팅창 전체 배경 상자

	UPROPERTY(meta = (BindWidget))
	class UButton* BTN_ToggleGuide; // 항상 화면에 남아있을 접이식 버튼

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* TXT_ToggleText; // 버튼의 글자 ("열기" / "닫기" 변경용)

	// --- 퀘스트 정보 위젯 ---
	UPROPERTY(meta = (BindWidget)) class UButton* BTN_RequestQuests;
	UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_Quest_1;
	UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_Quest_2;
	UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_Quest_3;
	UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_Quest_4;
	UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_Quest_5;
    
	// --- 상단 환경 정보 위젯 ---
	UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_InGameTime;
	UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_DisasterDay;
	
private:
	// 성공 시 호출될 함수
	UFUNCTION()
	void HandleOnSubQuestsGenerated(const FString& RequestId, const TArray<FQuestState>& Quests);
	
	// 서버 연결 실패나 에러 발생 시 텍스트를 원복할 실패 함수
	UFUNCTION()
	void HandleOnSubQuestRequestFailed(const FString& RequestId, const FString& ErrorMessage);
	
	// 엔터키를 쳤을 때 실행될 입력 바인딩 함수
	UFUNCTION()
	void HandleOnTextCommitted(const FText& Text, ETextCommit::Type CommitType);

	//AI 에이전트로부터 오퍼레이터 가이드 답변 패킷이 수신되었을 때 처리할 함수
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
