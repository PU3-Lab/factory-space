#include "UI/UI_DialogueBalloon.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/GameInstance.h"

void UUI_DialogueBalloon::NativeConstruct()
{
    Super::NativeConstruct();

    if (UGameInstance* GI = GetGameInstance())
    {
        QuestSubsystem = GI->GetSubsystem<UQuestManagerSubsystem>();
    }

    // 초기 생성 시 최근 대사 상태를 반영합니다.
    RefreshDialogueUI();
}

void UUI_DialogueBalloon::RefreshDialogueUI()
{
    if (!QuestSubsystem) return;

    FString OutQuestId;
    FString OutTriggerType;
    
    // 최근 로그된 대사 배열을 CachedLines에 쏙 받아옵니다.
    QuestSubsystem->GetLastTutorialDialogueLog(OutQuestId, OutTriggerType, CachedLines);

    // 대사 줄 인덱스 초기화 후 첫 줄 출력
    CurrentLineIndex = 0;
    DisplayCurrentLine();
}

void UUI_DialogueBalloon::DisplayCurrentLine()
{
    if (CachedLines.IsValidIndex(CurrentLineIndex))
    {
        // 켜야 할 대사가 있다면 말풍선 활성화
        if (DialogueContainer) DialogueContainer->SetVisibility(ESlateVisibility::Visible);

        const FTutorialQuestDialogueLine& CurrentLine = CachedLines[CurrentLineIndex];
        
        if (TXT_Dialogue) TXT_Dialogue->SetText(FText::FromString(CurrentLine.Dialogue));
    }
    else
    {
        // 모든 줄을 다 읽었다면 말풍선 숨기기
        if (DialogueContainer) DialogueContainer->SetVisibility(ESlateVisibility::Collapsed);
        CachedLines.Empty();
        CurrentLineIndex = 0;
    }
}

// 버튼이 없으므로 말풍선 창을 클릭할 때마다 대사가 넘어갑니다.
FReply UUI_DialogueBalloon::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    FReply Reply = Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);

    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        // 대사 인덱스를 올리고 다음 줄을 그립니다.
        CurrentLineIndex++;
        DisplayCurrentLine();
        return FReply::Handled();
    }

    return Reply;
}