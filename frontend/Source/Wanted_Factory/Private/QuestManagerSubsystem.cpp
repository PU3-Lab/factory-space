#include "QuestManagerSubsystem.h"

#include "FactoryAgentJsonUtils.h"
#include "FactoryAgentClientSubsystem.h"
#include "Wanted_Factory.h"
#include "Dom/JsonObject.h"

namespace
{
constexpr TCHAR QuestManagerQuestGeneratorAgentId[] = TEXT("quest_generator");
constexpr TCHAR ProductionQuestSubAgentId[] = TEXT("quest_generator.production_quest");
constexpr TCHAR QuestManagerQuestSampleRequestId[] = TEXT("request-quest-sample");
constexpr TCHAR QuestManagerQuestSampleSessionId[] = TEXT("smoke-session");
constexpr TCHAR QuestManagerQuestSampleClientId[] = TEXT("smoke-client");

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
	OutObjective.Quantity = FactoryAgentJsonUtils::GetIntegerField(ObjectiveObject, TEXT("quantity"), 1);
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
	FQuestState FirstQuest;
	FirstQuest.QuestId = TEXT("main-001");
	FirstQuest.Kind = EQuestKind::Main;
	FirstQuest.QuestType = TEXT("main");
	FirstQuest.Title = FText::FromString(TEXT("Factory Startup"));
	FirstQuest.Description = FText::FromString(TEXT("Build the first production line."));
	FirstQuest.Status = EQuestStatus::Active;

	FQuestObjective FirstObjective;
	FirstObjective.TargetItemId = TEXT("iron_ore");
	FirstObjective.Quantity = 10;
	FirstQuest.Objectives.Add(FirstObjective);

	FQuestState SecondQuest;
	SecondQuest.QuestId = TEXT("main-002");
	SecondQuest.Kind = EQuestKind::Main;
	SecondQuest.QuestType = TEXT("main");
	SecondQuest.Title = FText::FromString(TEXT("Expand Production"));
	SecondQuest.Description = FText::FromString(TEXT("Increase resource throughput for the next factory stage."));
	SecondQuest.Status = EQuestStatus::Inactive;

	FQuestObjective SecondObjective;
	SecondObjective.TargetItemId = TEXT("iron_ingot");
	SecondObjective.Quantity = 10;
	SecondQuest.Objectives.Add(SecondObjective);

	MainQuestSequence.Add(FirstQuest);
	MainQuestSequence.Add(SecondQuest);
}

void UQuestManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency(UFactoryAgentClientSubsystem::StaticClass());
	AgentClient = GetGameInstance()->GetSubsystem<UFactoryAgentClientSubsystem>();
	BindAgentClient();
	ActivateCurrentMainQuest();
}

void UQuestManagerSubsystem::Deinitialize()
{
	if (AgentClient)
	{
		AgentClient->OnAgentResponseReceived.RemoveAll(this);
		AgentClient->OnAgentErrorReceived.RemoveAll(this);
	}

	PendingSubQuestRequestIds.Empty();
	AgentClient = nullptr;

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
	}

	ActivateCurrentMainQuest();
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
	OnSubQuestsGenerated.Broadcast(RequestId, GeneratedQuests);
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
