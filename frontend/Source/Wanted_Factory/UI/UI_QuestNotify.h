#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI_QuestNotify.generated.h"

UCLASS()
class WANTED_FACTORY_API UUI_QuestNotify : public UUserWidget
{
	GENERATED_BODY()

public:
	// 외부에서 알림 텍스트를 주입해 주며 호출할 함수
	void PlayNotify(FString QuestName, FString RewardText);

protected:
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* TXT_NotifyQuestName;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* TXT_NotifyReward;

	// 애니메이션
	UFUNCTION(BlueprintImplementableEvent, Category = "QuestNotify")
	void K2_PlayNotifyAnimation();

private:
	UFUNCTION()
	void CloseNotify();

	FTimerHandle DestroyTimerHandle;
};