#include "UI_MainHUD.h"
#include "Components/TextBlock.h"
#include "QuestManagerSubsystem.h" // 🌟 동료분의 퀘스트 매니저 헤더 포함
#include "Kismet/GameplayStatics.h"

void UUI_MainHUD::NativeConstruct()
{
    Super::NativeConstruct();

    // 1. 게임 인스턴스로부터 퀘스트 매니저 서브시스템을 안전하게 찾아옵니다.
    UGameInstance* GameInstance = GetGameInstance();
    if (GameInstance)
    {
        UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>();
        if (QuestManager)
        {
            // 제목 5개가 업데이트되면 실행할 이벤트 함수 바인딩
            QuestManager->OnSubQuestTitlesUpdated.AddDynamic(this, &UUI_MainHUD::HandleOnSubQuestTitlesUpdated);
        }
    }

    //  UI가 생성되자마자 서버(에이전트)에 첫 서브퀘스트 목록을 요청합니다.
    RefreshSubQuests();
}

void UUI_MainHUD::RefreshSubQuests()
{
    UGameInstance* GameInstance = GetGameInstance();
    if (GameInstance)
    {
        UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>();
        if (QuestManager)
        {
            QuestManager->RequestSubQuests();
        }
    }
}

// 동료의 서브시스템이 5개를 파싱해서 브로드캐스트하면 일로 쇽 들어옵니다.
void UUI_MainHUD::HandleOnSubQuestTitlesUpdated(const FString& RequestId, const TArray<FString>& Titles)
{
    // 안전장치: 화면에 배치한 텍스트 위젯들을 제어하기 편하게 배열로 묶어둡니다.
    TArray<UTextBlock*> QuestTextBoxes;
    if (TXT_Quest_1) QuestTextBoxes.Add(TXT_Quest_1);
    if (TXT_Quest_2) QuestTextBoxes.Add(TXT_Quest_2);
    if (TXT_Quest_3) QuestTextBoxes.Add(TXT_Quest_3);
    if (TXT_Quest_4) QuestTextBoxes.Add(TXT_Quest_4);
    if (TXT_Quest_5) QuestTextBoxes.Add(TXT_Quest_5);

    // 넘어온 Titles 리스트를 돌면서 텍스트창에 하나씩 꽂아줍니다.
    for (int32 i = 0; i < QuestTextBoxes.Num(); ++i)
    {
        if (i < Titles.Num())
        {
            // 서버에서 정상적으로 제목이 넘어온 경우 화면에 출력
            QuestTextBoxes[i]->SetText(FText::FromString(Titles[i]));
            QuestTextBoxes[i]->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            // 만약 넘어온 퀘스트가 5개보다 적다면 남은 칸은 깔끔하게 숨깁니다.
            QuestTextBoxes[i]->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}