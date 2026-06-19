#include "QuestManagerSubsystem.h"

#include "Engine/DataTable.h"
#include "FactoryAgentJsonUtils.h"
#include "FactoryAgentClientSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PlayerWarehouseSubsystem.h"
#include "Serialization/Csv/CsvParser.h"
#include "Wanted_Factory.h"
#include "Dom/JsonObject.h"

#include "Algo/Sort.h"

namespace
{
constexpr TCHAR QuestManagerQuestGeneratorAgentId[] = TEXT("quest_generator");
constexpr TCHAR ProductionQuestSubAgentId[] = TEXT("quest_generator.production_quest");
constexpr TCHAR QuestManagerQuestSampleRequestId[] = TEXT("request-quest-sample");
constexpr TCHAR QuestManagerQuestSampleSessionId[] = TEXT("smoke-session");
constexpr TCHAR QuestManagerQuestSampleClientId[] = TEXT("smoke-client");
constexpr TCHAR MainQuestCsvRelativePath[] = TEXT("Source/Wanted_Factory/Data/MainQuestTable.csv");
constexpr TCHAR TutorialQuestStepsCsvRelativePath[] = TEXT("Source/Wanted_Factory/Data/tutorial_quest_steps.csv");
constexpr TCHAR TutorialQuestDialogueCsvRelativePath[] = TEXT("Source/Wanted_Factory/Data/tutorial_quest_dialogue.csv");

constexpr TCHAR TutorialEventInputAction[] = TEXT("InputAction");
constexpr TCHAR TutorialEventBuildMode[] = TEXT("BuildMode");
constexpr TCHAR TutorialEventInventoryOpen[] = TEXT("InventoryOpen");
constexpr TCHAR TutorialEventInventoryClose[] = TEXT("InventoryClose");
constexpr TCHAR TutorialEventRotateViewLeft[] = TEXT("RotateViewLeft");
constexpr TCHAR TutorialEventRotateViewRight[] = TEXT("RotateViewRight");
constexpr TCHAR TutorialEventRotatePlacement[] = TEXT("RotatePlacement");
constexpr TCHAR TutorialEventDemolishMode[] = TEXT("DemolishMode");
constexpr TCHAR TutorialEventDemolishRemoved[] = TEXT("DemolishRemoved");
constexpr TCHAR TutorialEventSelectMinerMode[] = TEXT("SelectMinerMode");
constexpr TCHAR TutorialEventValidMinerPlacement[] = TEXT("ValidMinerPlacement");
constexpr TCHAR TutorialEventSelectPowerPlantMode[] = TEXT("SelectPowerPlantMode");
constexpr TCHAR TutorialEventSelectPowerNodeMode[] = TEXT("SelectPowerNodeMode");
constexpr TCHAR TutorialEventSelectSmelterMode[] = TEXT("SelectSmelterMode");
constexpr TCHAR TutorialEventSelectWarehouseMode[] = TEXT("SelectWarehouseMode");
constexpr TCHAR TutorialEventSelectConveyorMode[] = TEXT("SelectConveyorMode");
constexpr TCHAR TutorialEventSelectPowerLineMode[] = TEXT("SelectPowerLineMode");
constexpr TCHAR TutorialEventPowerLineConnected[] = TEXT("PowerLineConnected");
constexpr TCHAR TutorialEventWarehouseOutputItemSet[] = TEXT("WarehouseOutputItemSet");

enum class ETutorialRequirementType : uint8
{
	Unsupported,
	EventCount,
	WarehouseCount
};

struct FTutorialRequirement
{
	ETutorialRequirementType Type = ETutorialRequirementType::Unsupported;
	FName EventId = NAME_None;
	FName TargetId = NAME_None;
	int32 RequiredCount = 1;
};

TSharedPtr<FJsonObject> CreateProductionPayload(const FString& Question)
{
	const TSharedPtr<FJsonObject> PayloadObject = MakeShared<FJsonObject>();
	PayloadObject->SetStringField(TEXT("sub_agent"), ProductionQuestSubAgentId);

	const FString TrimmedQuestion = Question.TrimStartAndEnd();
	if (!TrimmedQuestion.IsEmpty())
	{
		PayloadObject->SetStringField(TEXT("question"), TrimmedQuestion);
	}

	return PayloadObject;
}

bool ReadQuestObjective(const TSharedPtr<FJsonObject>& ObjectiveObject, FQuestObjective& OutObjective)
{
	if (!ObjectiveObject.IsValid())
	{
		return false;
	}

	OutObjective.TargetItemId = FactoryAgentJsonUtils::GetStringField(ObjectiveObject, TEXT("target_item_id"));
	OutObjective.TargetId = FName(*OutObjective.TargetItemId);
	OutObjective.ObjectiveType = EQuestObjectiveType::WarehouseStoreItem;
	OutObjective.Quantity = FactoryAgentJsonUtils::GetIntegerField(ObjectiveObject, TEXT("quantity"), 1);
	OutObjective.CurrentCount = 0;
	return !OutObjective.TargetItemId.IsEmpty() && OutObjective.Quantity > 0;
}

bool ReadQuestState(const TSharedPtr<FJsonObject>& QuestObject, FQuestState& OutQuest)
{
	if (!QuestObject.IsValid())
	{
		return false;
	}

	OutQuest.QuestId = FString::FromInt(FactoryAgentJsonUtils::GetIntegerField(QuestObject, TEXT("id"), 0));
	OutQuest.Kind = EQuestKind::Sub;
	OutQuest.QuestType = FactoryAgentJsonUtils::GetStringField(QuestObject, TEXT("type"));
	OutQuest.Title = FText::FromString(FactoryAgentJsonUtils::GetStringField(QuestObject, TEXT("title")));
	OutQuest.Description = FText::FromString(FactoryAgentJsonUtils::GetStringField(QuestObject, TEXT("description")));
	OutQuest.Status = EQuestStatus::Active;
	OutQuest.Objectives.Empty();

	const TArray<TSharedPtr<FJsonValue>>* ObjectiveValues = nullptr;
	if (QuestObject->TryGetArrayField(TEXT("objectives"), ObjectiveValues) && ObjectiveValues)
	{
		for (const TSharedPtr<FJsonValue>& ObjectiveValue : *ObjectiveValues)
		{
			FQuestObjective Objective;
			if (ReadQuestObjective(ObjectiveValue->AsObject(), Objective))
			{
				OutQuest.Objectives.Add(Objective);
			}
		}
	}

	return !OutQuest.QuestId.IsEmpty() && !OutQuest.Title.IsEmpty() && OutQuest.Objectives.Num() > 0;
}

bool ParseTutorialStepCsv(const FString& CsvContent, TArray<FTutorialQuestStep>& OutSteps)
{
	FCsvParser CsvParser(CsvContent);
	const FCsvParser::FRows& Rows = CsvParser.GetRows();
	if (Rows.Num() <= 1)
	{
		return false;
	}

	for (int32 RowIndex = 1; RowIndex < Rows.Num(); ++RowIndex)
	{
		const TArray<const TCHAR*>& Row = Rows[RowIndex];
		if (Row.Num() < 6)
		{
			continue;
		}

		FTutorialQuestStep Step;
		Step.QuestId = Row[0];
		Step.NextQuestId = Row[1];
		Step.Group = Row[2];
		Step.Title = Row[3];
		Step.Description = Row[4];
		Step.Reward = Row[5];
		if (!Step.QuestId.IsEmpty())
		{
			OutSteps.Add(Step);
		}
	}

	return OutSteps.Num() > 0;
}

bool ParseTutorialDialogueCsv(const FString& CsvContent, TArray<FTutorialQuestDialogueLine>& OutLines)
{
	FCsvParser CsvParser(CsvContent);
	const FCsvParser::FRows& Rows = CsvParser.GetRows();
	if (Rows.Num() <= 1)
	{
		return false;
	}

	for (int32 RowIndex = 1; RowIndex < Rows.Num(); ++RowIndex)
	{
		const TArray<const TCHAR*>& Row = Rows[RowIndex];
		if (Row.Num() < 5)
		{
			continue;
		}

		FTutorialQuestDialogueLine Line;
		Line.QuestId = Row[0];
		Line.TriggerType = Row[1];
		Line.LineOrder = FCString::Atoi(Row[2]);
		Line.Speaker = Row[3];
		Line.Dialogue = Row[4];
		if (!Line.QuestId.IsEmpty() && !Line.TriggerType.IsEmpty())
		{
			OutLines.Add(Line);
		}
	}

	return OutLines.Num() > 0;
}

FTutorialRequirement GetTutorialRequirement(const FString& QuestId)
{
	FTutorialRequirement Requirement;

	auto SetEventRequirement = [&Requirement](const TCHAR* EventName, const TCHAR* TargetName = nullptr, int32 RequiredCount = 1)
	{
		Requirement.Type = ETutorialRequirementType::EventCount;
		Requirement.EventId = FName(EventName);
		Requirement.TargetId = TargetName ? FName(TargetName) : NAME_None;
		Requirement.RequiredCount = RequiredCount;
	};

	auto SetWarehouseRequirement = [&Requirement](const TCHAR* ItemId, int32 RequiredCount)
	{
		Requirement.Type = ETutorialRequirementType::WarehouseCount;
		Requirement.TargetId = FName(ItemId);
		Requirement.RequiredCount = RequiredCount;
	};

	if (QuestId == TEXT("TUT_BASIC_001"))
	{
		SetEventRequirement(TutorialEventInputAction, TEXT("Move"));
	}
	else if (QuestId == TEXT("TUT_BASIC_002"))
	{
		SetEventRequirement(TutorialEventInputAction, TEXT("Jump"));
	}
	else if (QuestId == TEXT("TUT_BASIC_003"))
	{
		SetEventRequirement(TutorialEventInputAction, TEXT("Sprint"));
	}
	else if (QuestId == TEXT("TUT_BASIC_004"))
	{
		SetEventRequirement(TutorialEventInventoryOpen);
	}
	else if (QuestId == TEXT("TUT_BASIC_005"))
	{
		SetEventRequirement(TutorialEventInventoryClose);
	}
	else if (QuestId == TEXT("TUT_BUILD_001"))
	{
		SetEventRequirement(TutorialEventBuildMode);
	}
	else if (QuestId == TEXT("TUT_BUILD_002"))
	{
		SetEventRequirement(TutorialEventRotateViewLeft);
	}
	else if (QuestId == TEXT("TUT_BUILD_003"))
	{
		SetEventRequirement(TutorialEventRotateViewRight);
	}
	else if (QuestId == TEXT("TUT_BUILD_004"))
	{
		SetEventRequirement(TutorialEventRotatePlacement);
	}
	else if (QuestId == TEXT("TUT_BUILD_005"))
	{
		SetEventRequirement(TutorialEventDemolishMode);
	}
	else if (QuestId == TEXT("TUT_BUILD_006"))
	{
		SetEventRequirement(TutorialEventDemolishRemoved);
	}
	else if (QuestId == TEXT("TUT_MINING_001"))
	{
		SetEventRequirement(TutorialEventSelectMinerMode);
	}
	else if (QuestId == TEXT("TUT_MINING_002"))
	{
		SetEventRequirement(TutorialEventValidMinerPlacement);
	}
	else if (QuestId == TEXT("TUT_MINING_003"))
	{
		SetEventRequirement(TEXT("PlaceMachine"), TEXT("MinerMachine"));
	}
	else if (QuestId == TEXT("TUT_POWER_001"))
	{
		SetEventRequirement(TutorialEventSelectPowerPlantMode);
	}
	else if (QuestId == TEXT("TUT_POWER_002"))
	{
		SetEventRequirement(TEXT("PlaceMachine"), TEXT("PowerPlant"));
	}
	else if (QuestId == TEXT("TUT_POWER_003"))
	{
		SetEventRequirement(TutorialEventSelectPowerNodeMode);
	}
	else if (QuestId == TEXT("TUT_POWER_004"))
	{
		SetEventRequirement(TEXT("PlaceMachine"), TEXT("PowerGridNode"));
	}
	else if (QuestId == TEXT("TUT_POWER_005"))
	{
		SetEventRequirement(TutorialEventSelectPowerLineMode);
	}
	else if (QuestId == TEXT("TUT_POWER_006"))
	{
		SetEventRequirement(TutorialEventPowerLineConnected);
	}
	else if (QuestId == TEXT("TUT_LOGI_001"))
	{
		SetEventRequirement(TutorialEventSelectWarehouseMode);
	}
	else if (QuestId == TEXT("TUT_LOGI_002"))
	{
		SetEventRequirement(TEXT("PlaceMachine"), TEXT("WarehousePort"));
	}
	else if (QuestId == TEXT("TUT_LOGI_003"))
	{
		SetEventRequirement(TutorialEventSelectConveyorMode);
	}
	else if (QuestId == TEXT("TUT_LOGI_004"))
	{
		SetEventRequirement(TEXT("PlaceMachine"), TEXT("Conveyor"));
	}
	else if (QuestId == TEXT("TUT_LOGI_005"))
	{
		SetWarehouseRequirement(TEXT("iron_ore"), 1);
	}
	else if (QuestId == TEXT("TUT_SMELT_001"))
	{
		SetWarehouseRequirement(TEXT("iron_ore"), 10);
	}
	else if (QuestId == TEXT("TUT_POWER_007"))
	{
		SetEventRequirement(TutorialEventSelectPowerPlantMode);
	}
	else if (QuestId == TEXT("TUT_POWER_008"))
	{
		SetEventRequirement(TEXT("PlaceMachine"), TEXT("PowerPlant"));
	}
	else if (QuestId == TEXT("TUT_SMELT_002"))
	{
		SetEventRequirement(TEXT("PlaceMachine"), TEXT("Smelter"));
	}
	else if (QuestId == TEXT("TUT_LOGI_006"))
	{
		SetEventRequirement(TutorialEventSelectWarehouseMode);
	}
	else if (QuestId == TEXT("TUT_LOGI_007"))
	{
		SetEventRequirement(TEXT("PlaceMachine"), TEXT("WarehousePort"));
	}
	else if (QuestId == TEXT("TUT_LOGI_008"))
	{
		SetEventRequirement(TutorialEventWarehouseOutputItemSet, TEXT("iron_ore"));
	}
	else if (QuestId == TEXT("TUT_LOGI_009"))
	{
		SetEventRequirement(TutorialEventSelectConveyorMode);
	}
	else if (QuestId == TEXT("TUT_LOGI_010"))
	{
		SetEventRequirement(TEXT("PlaceMachine"), TEXT("Conveyor"));
	}
	else if (QuestId == TEXT("TUT_LOGI_011"))
	{
		SetEventRequirement(TutorialEventSelectWarehouseMode);
	}
	else if (QuestId == TEXT("TUT_LOGI_012"))
	{
		SetEventRequirement(TEXT("PlaceMachine"), TEXT("WarehousePort"));
	}
	else if (QuestId == TEXT("TUT_LOGI_013"))
	{
		SetEventRequirement(TutorialEventSelectConveyorMode);
	}
	else if (QuestId == TEXT("TUT_LOGI_014"))
	{
		SetEventRequirement(TEXT("PlaceMachine"), TEXT("Conveyor"));
	}
	else if (QuestId == TEXT("TUT_LOGI_015"))
	{
		SetWarehouseRequirement(TEXT("iron_ingot"), 1);
	}
	else if (QuestId == TEXT("TUT_EXPAND_001"))
	{
		SetWarehouseRequirement(TEXT("iron_ingot"), 10);
	}
	else if (QuestId == TEXT("TUT_EXPAND_002"))
	{
		SetEventRequirement(TEXT("PlaceMachine"), TEXT("PowerPlant"));
	}
	else if (QuestId == TEXT("TUT_EXPAND_003"))
	{
		SetEventRequirement(TEXT("PlaceMachine"), TEXT("PowerGridNode"));
	}
	else if (QuestId == TEXT("TUT_EXPAND_004"))
	{
		SetEventRequirement(TutorialEventPowerLineConnected);
	}
	else if (QuestId == TEXT("TUT_EXPAND_005"))
	{
		SetEventRequirement(TEXT("PlaceMachine"), TEXT("MinerMachine"));
	}
	else if (QuestId == TEXT("TUT_EXPAND_006"))
	{
		SetEventRequirement(TEXT("PlaceMachine"), TEXT("Smelter"));
	}
	else if (QuestId == TEXT("TUT_EXPAND_007"))
	{
		SetEventRequirement(TEXT("PlaceMachine"), TEXT("Conveyor"));
	}
	else if (QuestId == TEXT("TUT_EXPAND_008"))
	{
		SetWarehouseRequirement(TEXT("iron_ore"), 50);
	}
	else if (QuestId == TEXT("TUT_EXPAND_009"))
	{
		SetWarehouseRequirement(TEXT("iron_ingot"), 30);
	}

	return Requirement;
}

FString DescribeTutorialRequirement(const FTutorialRequirement& Requirement)
{
	switch (Requirement.Type)
	{
	case ETutorialRequirementType::EventCount:
		return Requirement.TargetId.IsNone()
			? FString::Printf(TEXT("event=%s count=%d"), *Requirement.EventId.ToString(), Requirement.RequiredCount)
			: FString::Printf(TEXT("event=%s target=%s count=%d"), *Requirement.EventId.ToString(), *Requirement.TargetId.ToString(), Requirement.RequiredCount);
	case ETutorialRequirementType::WarehouseCount:
		return FString::Printf(TEXT("warehouse item=%s count=%d"), *Requirement.TargetId.ToString(), Requirement.RequiredCount);
	default:
		return TEXT("manual test only");
	}
}
}

UQuestManagerSubsystem::UQuestManagerSubsystem()
{
}

void UQuestManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency(UFactoryAgentClientSubsystem::StaticClass());
	Collection.InitializeDependency(UPlayerWarehouseSubsystem::StaticClass());
	AgentClient = GetGameInstance()->GetSubsystem<UFactoryAgentClientSubsystem>();
	WarehouseSubsystem = GetGameInstance()->GetSubsystem<UPlayerWarehouseSubsystem>();
	LoadMainQuestSequence();
	LoadTutorialQuestTestData();
	BindAgentClient();
	BindWarehouse();
	ActivateCurrentMainQuest();
}

void UQuestManagerSubsystem::Deinitialize()
{
	if (AgentClient)
	{
		AgentClient->OnAgentResponseReceived.RemoveAll(this);
		AgentClient->OnAgentErrorReceived.RemoveAll(this);
	}

	if (WarehouseSubsystem)
	{
		WarehouseSubsystem->OnItemAdded.RemoveAll(this);
	}

	PendingSubQuestRequestIds.Empty();
	AgentClient = nullptr;
	WarehouseSubsystem = nullptr;

	Super::Deinitialize();
}

bool UQuestManagerSubsystem::GetCurrentMainQuest(FQuestState& OutQuest) const
{
	if (!MainQuestSequence.IsValidIndex(CurrentMainQuestIndex))
	{
		return false;
	}

	OutQuest = MainQuestSequence[CurrentMainQuestIndex];
	return true;
}

bool UQuestManagerSubsystem::SetCurrentMainQuestIndex(int32 NewIndex)
{
	if (!MainQuestSequence.IsValidIndex(NewIndex))
	{
		return false;
	}

	if (MainQuestSequence.IsValidIndex(CurrentMainQuestIndex))
	{
		MainQuestSequence[CurrentMainQuestIndex].Status = EQuestStatus::Completed;
	}

	CurrentMainQuestIndex = NewIndex;
	ActivateCurrentMainQuest();
	return true;
}

bool UQuestManagerSubsystem::AdvanceMainQuest()
{
	return SetCurrentMainQuestIndex(CurrentMainQuestIndex + 1);
}

void UQuestManagerSubsystem::SetMainQuestSequenceForSave(
	const TArray<FQuestState>& InMainQuestSequence,
	int32 InCurrentMainQuestIndex)
{
	MainQuestSequence = InMainQuestSequence;
	CurrentMainQuestIndex = FMath::Clamp(InCurrentMainQuestIndex, 0, FMath::Max(0, MainQuestSequence.Num() - 1));
	ActivateCurrentMainQuest();
}

void UQuestManagerSubsystem::ResetMainQuestProgress()
{
	CurrentMainQuestIndex = 0;
	for (FQuestState& Quest : MainQuestSequence)
	{
		Quest.Status = EQuestStatus::Inactive;
		for (FQuestObjective& Objective : Quest.Objectives)
		{
			Objective.CurrentCount = 0;
		}
	}

	ActivateCurrentMainQuest();
	RefreshMainQuestCompletion();
}

void UQuestManagerSubsystem::NotifyMainQuestInputAction(FName ActionId)
{
	ApplyMainQuestObjectiveEvent(EQuestObjectiveType::InputAction, ActionId, 1);
}

void UQuestManagerSubsystem::NotifyMainQuestBuildModeEntered()
{
	ApplyMainQuestObjectiveEvent(EQuestObjectiveType::BuildPlacementMode, TEXT("BuildMode"), 1);
}

void UQuestManagerSubsystem::NotifyMainQuestMachinePlaced(FName MachineType)
{
	ApplyMainQuestObjectiveEvent(EQuestObjectiveType::PlaceMachine, MachineType, 1);
}

void UQuestManagerSubsystem::ClearSubQuests()
{
	SubQuests.Empty();
	SubQuestTitles.Empty();
}

void UQuestManagerSubsystem::GetSubQuests(TArray<FQuestState>& OutQuests) const
{
	OutQuests = SubQuests;
}

void UQuestManagerSubsystem::GetSubQuestTitles(TArray<FString>& OutTitles) const
{
	OutTitles = SubQuestTitles;
}

void UQuestManagerSubsystem::SetSubQuestsForSave(
	const TArray<FQuestState>& InSubQuests,
	const TArray<FString>& InSubQuestTitles)
{
	SubQuests = InSubQuests;
	SubQuestTitles = InSubQuestTitles;
	OnSubQuestsUpdated.Broadcast(SubQuests);
	OnSubQuestTitlesUpdated.Broadcast(FString(), SubQuestTitles);
}

void UQuestManagerSubsystem::ConnectQuestAgent()
{
	if (!AgentClient)
	{
		LOG_LC_W(TEXT("Quest manager could not find FactoryAgentClientSubsystem."));
		return;
	}

	AgentClient->ConnectToDefaultServer();
}

FString UQuestManagerSubsystem::RequestSubQuests()
{
	if (!AgentClient)
	{
		OnSubQuestRequestFailed.Broadcast(FString(), TEXT("FactoryAgentClientSubsystem is not available."));
		return FString();
	}

	if (!AgentClient->SendQuestGeneratorRequest(QuestManagerQuestSampleRequestId, QuestManagerQuestSampleSessionId, QuestManagerQuestSampleClientId))
	{
		OnSubQuestRequestFailed.Broadcast(FString(), TEXT("Failed to send quest generator request. Check the agent connection."));
		return FString();
	}

	const FString RequestId = QuestManagerQuestSampleRequestId;
	ClearSubQuests();
	PendingSubQuestRequestIds.Add(RequestId);
	OnSubQuestRequestStarted.Broadcast(RequestId, QuestManagerQuestGeneratorAgentId);
	return RequestId;
}

FString UQuestManagerSubsystem::RequestProductionSubQuests(const FString& Question)
{
	return SendSubQuestRequest(FactoryAgentJsonUtils::WriteJsonObject(CreateProductionPayload(Question)));
}

void UQuestManagerSubsystem::StartTutorialQuestTest()
{
	if (TutorialQuestSteps.IsEmpty())
	{
		LOG_LC_W(TEXT("Tutorial quest test could not start because no tutorial quest steps were loaded."));
		return;
	}

	bTutorialQuestTestActive = true;
	CurrentTutorialQuestId = TutorialQuestSteps[0].QuestId;
	bPendingTutorialStartDialogueReveal = false;
	BroadcastCurrentTutorialQuestStep();
	LogTutorialDialogue(CurrentTutorialQuestId, TEXT("on_start"));
}

bool UQuestManagerSubsystem::CompleteCurrentTutorialQuestForTest()
{
	if (!bTutorialQuestTestActive)
	{
		StartTutorialQuestTest();
		return !CurrentTutorialQuestId.IsEmpty();
	}

	return AdvanceTutorialQuestStep(true);
}

void UQuestManagerSubsystem::LogCurrentTutorialQuestTestState() const
{
}

bool UQuestManagerSubsystem::GetCurrentTutorialQuestStep(FTutorialQuestStep& OutStep) const
{
	const FTutorialQuestStep* Step = FindCurrentTutorialQuestStep();
	if (!Step)
	{
		return false;
	}

	OutStep = *Step;
	return true;
}

bool UQuestManagerSubsystem::GetTutorialQuestStepById(const FString& QuestId, FTutorialQuestStep& OutStep) const
{
	const int32* StepIndex = TutorialQuestStepIndexById.Find(QuestId);
	if (!StepIndex || !TutorialQuestSteps.IsValidIndex(*StepIndex))
	{
		return false;
	}

	OutStep = TutorialQuestSteps[*StepIndex];
	return true;
}

void UQuestManagerSubsystem::GetTutorialDialogueLines(
	const FString& QuestId,
	const FString& TriggerType,
	TArray<FTutorialQuestDialogueLine>& OutLines) const
{
	OutLines.Reset();

	const TArray<FTutorialQuestDialogueLine>* Lines = TutorialDialogueByQuestId.Find(QuestId);
	if (!Lines)
	{
		return;
	}

	for (const FTutorialQuestDialogueLine& Line : *Lines)
	{
		if (Line.TriggerType == TriggerType)
		{
			OutLines.Add(Line);
		}
	}
}

void UQuestManagerSubsystem::GetLastTutorialDialogueLog(
	FString& OutQuestId,
	FString& OutTriggerType,
	TArray<FTutorialQuestDialogueLine>& OutLines) const
{
	OutQuestId = LastTutorialDialogueQuestId;
	OutTriggerType = LastTutorialDialogueTriggerType;
	OutLines = LastTutorialDialogueLines;
}

void UQuestManagerSubsystem::GetTutorialSaveState(
	bool& bOutTutorialQuestTestActive,
	FString& OutCurrentTutorialQuestId,
	bool& bOutPendingTutorialStartDialogueReveal,
	FString& OutLastTutorialDialogueQuestId,
	FString& OutLastTutorialDialogueTriggerType,
	TArray<FTutorialQuestDialogueLine>& OutLastTutorialDialogueLines) const
{
	bOutTutorialQuestTestActive = bTutorialQuestTestActive;
	OutCurrentTutorialQuestId = CurrentTutorialQuestId;
	bOutPendingTutorialStartDialogueReveal = bPendingTutorialStartDialogueReveal;
	OutLastTutorialDialogueQuestId = LastTutorialDialogueQuestId;
	OutLastTutorialDialogueTriggerType = LastTutorialDialogueTriggerType;
	OutLastTutorialDialogueLines = LastTutorialDialogueLines;
}

void UQuestManagerSubsystem::RestoreTutorialSaveState(
	bool bInTutorialQuestTestActive,
	const FString& InCurrentTutorialQuestId,
	bool bInPendingTutorialStartDialogueReveal,
	const FString& InLastTutorialDialogueQuestId,
	const FString& InLastTutorialDialogueTriggerType,
	const TArray<FTutorialQuestDialogueLine>& InLastTutorialDialogueLines)
{
	bTutorialQuestTestActive = bInTutorialQuestTestActive;
	CurrentTutorialQuestId = InCurrentTutorialQuestId;
	bPendingTutorialStartDialogueReveal = bInPendingTutorialStartDialogueReveal;
	LastTutorialDialogueQuestId = InLastTutorialDialogueQuestId;
	LastTutorialDialogueTriggerType = InLastTutorialDialogueTriggerType;
	LastTutorialDialogueLines = InLastTutorialDialogueLines;

	BroadcastCurrentTutorialQuestStep();
	OnTutorialDialogueLogged.Broadcast(
		LastTutorialDialogueQuestId,
		LastTutorialDialogueTriggerType,
		LastTutorialDialogueLines);
}

bool UQuestManagerSubsystem::HasPendingTutorialStartDialogue() const
{
	return bPendingTutorialStartDialogueReveal;
}

void UQuestManagerSubsystem::RevealPendingTutorialStartDialogue()
{
	if (!bPendingTutorialStartDialogueReveal || CurrentTutorialQuestId.IsEmpty())
	{
		return;
	}

	bPendingTutorialStartDialogueReveal = false;
	LogTutorialDialogue(CurrentTutorialQuestId, TEXT("on_start"));
}

void UQuestManagerSubsystem::NotifyTutorialEvent(FName EventId, FName TargetId, int32 DeltaCount)
{
	if (!bTutorialQuestTestActive || bPendingTutorialStartDialogueReveal || DeltaCount <= 0)
	{
		return;
	}

	const FTutorialQuestStep* Step = FindCurrentTutorialQuestStep();
	if (!Step)
	{
		return;
	}

	const FTutorialRequirement Requirement = GetTutorialRequirement(Step->QuestId);
	if (Requirement.Type != ETutorialRequirementType::EventCount)
	{
		return;
	}

	if (Requirement.EventId != EventId)
	{
		return;
	}

	if (!Requirement.TargetId.IsNone() && Requirement.TargetId != TargetId)
	{
		return;
	}

	AdvanceTutorialQuestStep(false);
}

void UQuestManagerSubsystem::ActivateCurrentMainQuest()
{
	for (int32 Index = 0; Index < MainQuestSequence.Num(); ++Index)
	{
		if (Index < CurrentMainQuestIndex)
		{
			MainQuestSequence[Index].Status = EQuestStatus::Completed;
		}
		else
		{
			MainQuestSequence[Index].Status = Index == CurrentMainQuestIndex
				? EQuestStatus::Active
				: EQuestStatus::Inactive;
		}
	}

	if (MainQuestSequence.IsValidIndex(CurrentMainQuestIndex))
	{
		OnMainQuestChanged.Broadcast(MainQuestSequence[CurrentMainQuestIndex]);
	}
}

void UQuestManagerSubsystem::BindAgentClient()
{
	if (!AgentClient)
	{
		LOG_LC_W(TEXT("Quest manager could not bind FactoryAgentClientSubsystem."));
		return;
	}

	AgentClient->OnAgentResponseReceived.AddDynamic(this, &UQuestManagerSubsystem::HandleAgentResponse);
	AgentClient->OnAgentErrorReceived.AddDynamic(this, &UQuestManagerSubsystem::HandleAgentError);
}

void UQuestManagerSubsystem::BindWarehouse()
{
	if (!WarehouseSubsystem)
	{
		LOG_LC_W(TEXT("Quest manager could not bind PlayerWarehouseSubsystem."));
		return;
	}

	WarehouseSubsystem->OnItemAdded.AddDynamic(this, &UQuestManagerSubsystem::HandleWarehouseItemAdded);
}

void UQuestManagerSubsystem::LoadTutorialQuestTestData()
{
	TutorialQuestSteps.Empty();
	TutorialDialogueLines.Empty();
	TutorialQuestStepIndexById.Empty();
	TutorialDialogueByQuestId.Empty();
	CurrentTutorialQuestId.Empty();
	bTutorialQuestTestActive = false;
	bPendingTutorialStartDialogueReveal = false;
	LastTutorialDialogueQuestId.Empty();
	LastTutorialDialogueTriggerType.Empty();
	LastTutorialDialogueLines.Reset();

	FString StepsCsvContent;
	const FString StepsCsvPath = FPaths::Combine(FPaths::ProjectDir(), TutorialQuestStepsCsvRelativePath);
	if (!FFileHelper::LoadFileToString(StepsCsvContent, *StepsCsvPath))
	{
		LOG_LC_W(TEXT("Quest manager could not load tutorial quest steps CSV: %s"), *StepsCsvPath);
		return;
	}

	if (!ParseTutorialStepCsv(StepsCsvContent, TutorialQuestSteps))
	{
		LOG_LC_W(TEXT("Quest manager could not parse tutorial quest steps CSV: %s"), *StepsCsvPath);
		return;
	}

	for (int32 Index = 0; Index < TutorialQuestSteps.Num(); ++Index)
	{
		TutorialQuestStepIndexById.Add(TutorialQuestSteps[Index].QuestId, Index);
	}

	FString DialogueCsvContent;
	const FString DialogueCsvPath = FPaths::Combine(FPaths::ProjectDir(), TutorialQuestDialogueCsvRelativePath);
	if (!FFileHelper::LoadFileToString(DialogueCsvContent, *DialogueCsvPath))
	{
		LOG_LC_W(TEXT("Quest manager could not load tutorial quest dialogue CSV: %s"), *DialogueCsvPath);
		return;
	}

	if (!ParseTutorialDialogueCsv(DialogueCsvContent, TutorialDialogueLines))
	{
		LOG_LC_W(TEXT("Quest manager could not parse tutorial quest dialogue CSV: %s"), *DialogueCsvPath);
		return;
	}

	for (const FTutorialQuestDialogueLine& Line : TutorialDialogueLines)
	{
		TutorialDialogueByQuestId.FindOrAdd(Line.QuestId).Add(Line);
	}

	for (TPair<FString, TArray<FTutorialQuestDialogueLine>>& Pair : TutorialDialogueByQuestId)
	{
		Pair.Value.Sort(
			[](const FTutorialQuestDialogueLine& Left, const FTutorialQuestDialogueLine& Right)
			{
				return Left.LineOrder < Right.LineOrder;
			});
	}

}

void UQuestManagerSubsystem::LoadMainQuestSequence()
{
	MainQuestSequence.Empty();
	CurrentMainQuestIndex = 0;

	if (!MainQuestTable)
	{
		MainQuestTable = NewObject<UDataTable>(this, TEXT("MainQuestTable"));
		if (MainQuestTable)
		{
			MainQuestTable->RowStruct = FMainQuestTableRow::StaticStruct();
		}
	}

	if (!MainQuestTable)
	{
		LOG_LC_W(TEXT("Quest manager could not create main quest table."));
		return;
	}

	const FString CsvPath = FPaths::Combine(FPaths::ProjectDir(), MainQuestCsvRelativePath);
	FString CsvContent;
	if (!FFileHelper::LoadFileToString(CsvContent, *CsvPath))
	{
		LOG_LC_W(TEXT("Quest manager could not load main quest CSV: %s"), *CsvPath);
		return;
	}

	MainQuestTable->EmptyTable();
	const TArray<FString> ImportProblems = MainQuestTable->CreateTableFromCSVString(CsvContent);
	for (const FString& Problem : ImportProblems)
	{
		LOG_LC_W(TEXT("Main quest CSV import warning: %s"), *Problem);
	}

	TArray<FMainQuestTableRow*> Rows;
	MainQuestTable->GetAllRows(TEXT("MainQuestTable"), Rows);
	Algo::Sort(
		Rows,
		[](const FMainQuestTableRow* Left, const FMainQuestTableRow* Right)
		{
			return Left && Right ? Left->Order < Right->Order : Left != nullptr;
		});

	for (const FMainQuestTableRow* Row : Rows)
	{
		if (!Row)
		{
			continue;
		}

		FQuestState Quest;
		Quest.QuestId = Row->QuestId;
		Quest.Kind = EQuestKind::Main;
		Quest.QuestType = TEXT("main");
		Quest.Title = Row->Title;
		Quest.Description = Row->Description;
		Quest.Status = EQuestStatus::Inactive;

		AppendMainQuestObjective(Quest.Objectives, Row->ObjectiveType1, Row->TargetId1, Row->RequiredCount1);
		AppendMainQuestObjective(Quest.Objectives, Row->ObjectiveType2, Row->TargetId2, Row->RequiredCount2);
		AppendMainQuestReward(Quest.Rewards, Row->RewardItemId1, Row->RewardQuantity1);

		if (!Quest.QuestId.IsEmpty() && !Quest.Objectives.IsEmpty())
		{
			MainQuestSequence.Add(Quest);
		}
	}
}

void UQuestManagerSubsystem::AppendMainQuestObjective(
	TArray<FQuestObjective>& Objectives,
	EQuestObjectiveType ObjectiveType,
	FName TargetId,
	int32 RequiredCount) const
{
	if (ObjectiveType == EQuestObjectiveType::None || TargetId.IsNone() || RequiredCount <= 0)
	{
		return;
	}

	FQuestObjective Objective;
	Objective.ObjectiveType = ObjectiveType;
	Objective.TargetId = TargetId;
	Objective.TargetItemId = TargetId.ToString();
	Objective.Quantity = RequiredCount;
	Objective.CurrentCount = 0;
	Objectives.Add(Objective);
}

void UQuestManagerSubsystem::AppendMainQuestReward(TArray<FQuestRewardItem>& Rewards, FName ItemId, int32 Quantity) const
{
	if (ItemId.IsNone() || Quantity <= 0)
	{
		return;
	}

	FQuestRewardItem Reward;
	Reward.ItemId = ItemId;
	Reward.Quantity = Quantity;
	Rewards.Add(Reward);
}

FString UQuestManagerSubsystem::SendSubQuestRequest(const FString& PayloadJson)
{
	if (!AgentClient)
	{
		OnSubQuestRequestFailed.Broadcast(FString(), TEXT("FactoryAgentClientSubsystem is not available."));
		return FString();
	}

	const FString RequestId = AgentClient->SendAgentRequest(QuestManagerQuestGeneratorAgentId, PayloadJson);
	if (RequestId.IsEmpty())
	{
		OnSubQuestRequestFailed.Broadcast(FString(), TEXT("Failed to send sub quest request. Check the agent connection."));
		return FString();
	}

	ClearSubQuests();
	PendingSubQuestRequestIds.Add(RequestId);
	OnSubQuestRequestStarted.Broadcast(RequestId, QuestManagerQuestGeneratorAgentId);
	return RequestId;
}

void UQuestManagerSubsystem::RefreshSubQuestCompletion()
{
	bool bAnyQuestUpdated = false;

	for (FQuestState& Quest : SubQuests)
	{
		if (Quest.Status == EQuestStatus::Completed)
		{
			continue;
		}

		if (IsQuestCompletedByWarehouse(Quest))
		{
			Quest.Status = EQuestStatus::Completed;
			bAnyQuestUpdated = true;
		}
	}

	if (bAnyQuestUpdated)
	{
		OnSubQuestsUpdated.Broadcast(SubQuests);
	}
}

void UQuestManagerSubsystem::RefreshMainQuestCompletion()
{
	if (!MainQuestSequence.IsValidIndex(CurrentMainQuestIndex))
	{
		return;
	}

	FQuestState& CurrentQuest = MainQuestSequence[CurrentMainQuestIndex];
	if (CurrentQuest.Status != EQuestStatus::Active || !IsMainQuestCompleted(CurrentQuest))
	{
		return;
	}

	CurrentQuest.Status = EQuestStatus::Completed;
	GrantQuestRewards(CurrentQuest);
	AdvanceMainQuest();
}

void UQuestManagerSubsystem::GrantQuestRewards(const FQuestState& Quest)
{
	if (!WarehouseSubsystem)
	{
		return;
	}

	for (const FQuestRewardItem& Reward : Quest.Rewards)
	{
		if (!Reward.ItemId.IsNone() && Reward.Quantity > 0)
		{
			WarehouseSubsystem->AddItem(Reward.ItemId, Reward.Quantity);
		}
	}
}

bool UQuestManagerSubsystem::IsMainQuestCompleted(const FQuestState& Quest) const
{
	if (Quest.Objectives.IsEmpty())
	{
		return false;
	}

	for (const FQuestObjective& Objective : Quest.Objectives)
	{
		if (Objective.ObjectiveType == EQuestObjectiveType::WarehouseStoreItem)
		{
			if (!WarehouseSubsystem || WarehouseSubsystem->GetItemCount(Objective.TargetId) < Objective.Quantity)
			{
				return false;
			}
			continue;
		}

		if (Objective.CurrentCount < Objective.Quantity)
		{
			return false;
		}
	}

	return true;
}

void UQuestManagerSubsystem::ApplyMainQuestObjectiveEvent(EQuestObjectiveType ObjectiveType, FName TargetId, int32 DeltaCount)
{
	if (!MainQuestSequence.IsValidIndex(CurrentMainQuestIndex) || DeltaCount <= 0)
	{
		return;
	}

	FQuestState& CurrentQuest = MainQuestSequence[CurrentMainQuestIndex];
	if (CurrentQuest.Status != EQuestStatus::Active)
	{
		return;
	}

	bool bQuestUpdated = false;
	for (FQuestObjective& Objective : CurrentQuest.Objectives)
	{
		if (Objective.ObjectiveType != ObjectiveType || Objective.TargetId != TargetId)
		{
			continue;
		}

		const int32 NewCount = FMath::Min(Objective.Quantity, Objective.CurrentCount + DeltaCount);
		if (NewCount != Objective.CurrentCount)
		{
			Objective.CurrentCount = NewCount;
			bQuestUpdated = true;
		}
	}

	if (!bQuestUpdated)
	{
		return;
	}

	RefreshMainQuestCompletion();
	if (MainQuestSequence.IsValidIndex(CurrentMainQuestIndex))
	{
		OnMainQuestChanged.Broadcast(MainQuestSequence[CurrentMainQuestIndex]);
		RefreshMainQuestCompletion();
	}
}

bool UQuestManagerSubsystem::IsQuestCompletedByWarehouse(const FQuestState& Quest) const
{
	if (!WarehouseSubsystem || Quest.Objectives.IsEmpty())
	{
		return false;
	}

	for (const FQuestObjective& Objective : Quest.Objectives)
	{
		const FName ItemId(*Objective.TargetItemId);
		if (ItemId.IsNone() || WarehouseSubsystem->GetItemCount(ItemId) < Objective.Quantity)
		{
			return false;
		}
	}

	return true;
}

bool UQuestManagerSubsystem::AdvanceTutorialQuestStep(bool bFromManualTest)
{
	const FTutorialQuestStep* CurrentStep = FindCurrentTutorialQuestStep();
	if (!CurrentStep)
	{
		LOG_LC_W(TEXT("Tutorial quest test advance failed because there is no active step."));
		return false;
	}

	LogTutorialDialogue(CurrentStep->QuestId, TEXT("on_complete"));

	if (CurrentStep->NextQuestId.IsEmpty())
	{
		CurrentTutorialQuestId.Empty();
		bTutorialQuestTestActive = false;
		bPendingTutorialStartDialogueReveal = false;
		return true;
	}

	const int32* NextIndex = TutorialQuestStepIndexById.Find(CurrentStep->NextQuestId);
	if (!NextIndex || !TutorialQuestSteps.IsValidIndex(*NextIndex))
	{
		LOG_LC_W(
			TEXT("Tutorial quest test could not find next step [%s] after [%s]."),
			*CurrentStep->NextQuestId,
			*CurrentStep->QuestId);
		CurrentTutorialQuestId.Empty();
		bTutorialQuestTestActive = false;
		bPendingTutorialStartDialogueReveal = false;
		return false;
	}

	CurrentTutorialQuestId = CurrentStep->NextQuestId;
	bPendingTutorialStartDialogueReveal = true;
	BroadcastCurrentTutorialQuestStep();
	return true;
}

void UQuestManagerSubsystem::BroadcastCurrentTutorialQuestStep()
{
	const FTutorialQuestStep* Step = FindCurrentTutorialQuestStep();
	if (!Step)
	{
		return;
	}

	OnTutorialStepChanged.Broadcast(*Step);
}

void UQuestManagerSubsystem::LogTutorialDialogue(const FString& QuestId, const FString& TriggerType)
{
	TArray<FTutorialQuestDialogueLine> MatchingLines;
	GetTutorialDialogueLines(QuestId, TriggerType, MatchingLines);
	if (MatchingLines.IsEmpty())
	{
		LastTutorialDialogueQuestId = QuestId;
		LastTutorialDialogueTriggerType = TriggerType;
		LastTutorialDialogueLines.Reset();
		OnTutorialDialogueLogged.Broadcast(QuestId, TriggerType, LastTutorialDialogueLines);
		return;
	}

	LastTutorialDialogueQuestId = QuestId;
	LastTutorialDialogueTriggerType = TriggerType;
	LastTutorialDialogueLines = MatchingLines;

	OnTutorialDialogueLogged.Broadcast(QuestId, TriggerType, LastTutorialDialogueLines);
}

const FTutorialQuestStep* UQuestManagerSubsystem::FindCurrentTutorialQuestStep() const
{
	if (CurrentTutorialQuestId.IsEmpty())
	{
		return nullptr;
	}

	const int32* StepIndex = TutorialQuestStepIndexById.Find(CurrentTutorialQuestId);
	return StepIndex && TutorialQuestSteps.IsValidIndex(*StepIndex)
		? &TutorialQuestSteps[*StepIndex]
		: nullptr;
}

void UQuestManagerSubsystem::HandleWarehouseItemAdded(FName ItemID, int32 AddedCount, int32 NewTotalCount)
{
	if (bTutorialQuestTestActive && !bPendingTutorialStartDialogueReveal)
	{
		const FTutorialQuestStep* Step = FindCurrentTutorialQuestStep();
		if (Step)
		{
			const FTutorialRequirement Requirement = GetTutorialRequirement(Step->QuestId);
			if (Requirement.Type == ETutorialRequirementType::WarehouseCount
				&& Requirement.TargetId == ItemID
				&& NewTotalCount >= Requirement.RequiredCount)
			{
				AdvanceTutorialQuestStep(false);
			}
		}
	}

	RefreshSubQuestCompletion();
	RefreshMainQuestCompletion();
}

void UQuestManagerSubsystem::HandleAgentResponse(
	const FString& RequestId,
	const FString& Agent,
	const FString& PayloadJson,
	const FString& RawMessage)
{
	if (Agent != QuestManagerQuestGeneratorAgentId || !PendingSubQuestRequestIds.Contains(RequestId))
	{
		return;
	}

	PendingSubQuestRequestIds.Remove(RequestId);

	TSharedPtr<FJsonObject> PayloadObject;
	if (!FactoryAgentJsonUtils::ParseJsonObject(PayloadJson, PayloadObject))
	{
		OnSubQuestRequestFailed.Broadcast(RequestId, TEXT("Sub quest response payload was not valid JSON."));
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* QuestValues = nullptr;
	if (!PayloadObject->TryGetArrayField(TEXT("quests"), QuestValues) || !QuestValues)
	{
		OnSubQuestRequestFailed.Broadcast(RequestId, TEXT("Sub quest response did not include a quests array."));
		return;
	}

	TArray<FQuestState> GeneratedQuests;
	TArray<FString> GeneratedTitles;
	for (const TSharedPtr<FJsonValue>& QuestValue : *QuestValues)
	{
		FQuestState Quest;
		if (ReadQuestState(QuestValue->AsObject(), Quest))
		{
			GeneratedQuests.Add(Quest);
			GeneratedTitles.Add(Quest.Title.ToString());
		}
	}

	if (GeneratedQuests.IsEmpty())
	{
		OnSubQuestRequestFailed.Broadcast(RequestId, TEXT("Sub quest response did not include any valid quests."));
		return;
	}

	SubQuests = GeneratedQuests;
	SubQuestTitles = GeneratedTitles;
	RefreshSubQuestCompletion();
	OnSubQuestsGenerated.Broadcast(RequestId, SubQuests);
	OnSubQuestTitlesUpdated.Broadcast(RequestId, SubQuestTitles);
}

void UQuestManagerSubsystem::HandleAgentError(
	const FString& RequestId,
	const FString& Agent,
	const FString& ErrorCode,
	const FString& ErrorMessage,
	const FString& RawMessage)
{
	if (Agent != QuestManagerQuestGeneratorAgentId || !PendingSubQuestRequestIds.Contains(RequestId))
	{
		return;
	}

	PendingSubQuestRequestIds.Remove(RequestId);
	OnSubQuestRequestFailed.Broadcast(RequestId, FString::Printf(TEXT("%s: %s"), *ErrorCode, *ErrorMessage));
}
