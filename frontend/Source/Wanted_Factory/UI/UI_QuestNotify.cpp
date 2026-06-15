#include "UI/UI_QuestNotify.h"
#include "Components/TextBlock.h"

void UUI_QuestNotify::PlayNotify(FString QuestName, FString RewardText)
{
	if (!TXT_NotifyQuestName || !TXT_NotifyReward) return;

	// 1. 텍스트 채우기
	TXT_NotifyQuestName->SetText(FText::FromString(FString::Printf(TEXT("퀘스트 완료: %s "), *QuestName)));
	TXT_NotifyReward->SetText(FText::FromString(FString::Printf(TEXT("보상: %s"), *RewardText)));

	// 2. 블루프린트 애니메이션 실행
	K2_PlayNotifyAnimation();

	// 2초 뒤에 화면에서 사라지도록 타이머 설정
	GetWorld()->GetTimerManager().SetTimer(
		DestroyTimerHandle,
		this,
		&UUI_QuestNotify::CloseNotify,
		2.0f,
		false
	);
}

void UUI_QuestNotify::CloseNotify()
{
	RemoveFromParent();
}