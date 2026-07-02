#include "UI/UI_SimpleQuestWindow.h"
#include "Components/TextBlock.h"

void UUI_SimpleQuestWindow::UpdateSimpleQuest(const FText& Title, const FText& Objective)
{
	if (TXT_MainQuestTitle)     TXT_MainQuestTitle->SetText(Title);
	if (TXT_MainQuestObjective) TXT_MainQuestObjective->SetText(Objective);
}