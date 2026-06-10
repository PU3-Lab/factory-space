#include "QuestManagerSubsystem.h"

#include "Engine/DataTable.h"
#include "FactoryAgentJsonUtils.h"
#include "FactoryAgentClientSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PlayerWarehouseSubsystem.h"
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
constexpr TCHAR MainQuestCsvRelativePath[] = TEXT("Source/Wanted_Factory/Quest/MainQuestTable.csv");

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

void UQuestManagerSubsystem::HandleWarehouseItemAdded(FName ItemID, int32 AddedCount, int32 NewTotalCount)
{
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
