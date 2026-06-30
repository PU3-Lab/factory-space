#include "FactoryAgentClientSubsystem.h"

#include "Conveyor.h"
#include "FactoryManagerSubsystem.h"
#include "FactoryAgentJsonUtils.h"
#include "IWebSocket.h"
#include "Machines/PowerGridNode.h"
#include "Machines/PowerPlant.h"
#include "Wanted_Factory.h"
#include "WebSocketsModule.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Guid.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"

namespace
{
constexpr TCHAR AgentRequestType[] = TEXT("agent.request");
constexpr TCHAR AgentResponseType[] = TEXT("agent.response");
constexpr TCHAR AgentErrorType[] = TEXT("agent.error");
constexpr TCHAR AgentProgressType[] = TEXT("agent.progress");
constexpr TCHAR QuestGeneratorAgentId[] = TEXT("quest_generator");
constexpr TCHAR OperatorGuideAgentId[] = TEXT("operator_guide");
constexpr TCHAR ProcessOptimizerAgentId[] = TEXT("process_optimizer");
constexpr TCHAR QuestSampleRequestId[] = TEXT("request-quest-sample");
constexpr TCHAR QuestSampleSessionId[] = TEXT("smoke-session");
constexpr TCHAR QuestSampleClientId[] = TEXT("smoke-client");
constexpr TCHAR OperatorGuideRequestId[] = TEXT("operator-guide-demo-multi-001");
constexpr TCHAR OperatorGuideSessionId[] = TEXT("operator-guide-demo-session");
constexpr TCHAR OperatorGuideClientId[] = TEXT("unreal-client");
constexpr TCHAR ProcessOptimizerSessionId[] = TEXT("player-session-001");
constexpr TCHAR ProcessOptimizerClientId[] = TEXT("unreal-client");
constexpr TCHAR DefaultSessionId[] = TEXT("dev-session");
constexpr TCHAR DefaultClientId[] = TEXT("unreal-client");

FString MachineStateToStatusString(const EMachineState MachineState)
{
	switch (MachineState)
	{
	case EMachineState::Idle:
		return TEXT("idle");
	case EMachineState::Working:
		return TEXT("operating");
	case EMachineState::NoPower:
		return TEXT("no_power");
	case EMachineState::Blocked:
		return TEXT("blocked");
	case EMachineState::Disabled:
		return TEXT("disabled");
	default:
		return TEXT("unknown");
	}
}

FString SanitizeObjectName(FString ObjectName)
{
	ObjectName.RemoveFromStart(TEXT("BP_"));

	constexpr TCHAR GeneratedClassSeparator[] = TEXT("_C_");
	const FString SeparatorString = GeneratedClassSeparator;
	const int32 ClassSeparatorIndex = ObjectName.Find(SeparatorString, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	if (ClassSeparatorIndex != INDEX_NONE)
	{
		const FString InstanceSuffix = ObjectName.Mid(ClassSeparatorIndex + SeparatorString.Len());
		if (!InstanceSuffix.IsEmpty() && InstanceSuffix.IsNumeric())
		{
			ObjectName = ObjectName.Left(ClassSeparatorIndex) + TEXT("_") + InstanceSuffix;
		}
	}

	return ObjectName;
}

FString NormalizeTypeName(const FString& SourceType)
{
	FString Result;
	Result.Reserve(SourceType.Len() + 8);

	for (int32 Index = 0; Index < SourceType.Len(); ++Index)
	{
		const TCHAR Character = SourceType[Index];
		const bool bIsUpper = FChar::IsUpper(Character);
		if (bIsUpper && Index > 0)
		{
			const TCHAR PreviousCharacter = SourceType[Index - 1];
			const bool bPreviousIsLowerOrDigit = FChar::IsLower(PreviousCharacter) || FChar::IsDigit(PreviousCharacter);
			const bool bNextIsLower =
				Index + 1 < SourceType.Len() && FChar::IsLower(SourceType[Index + 1]);
			if (bPreviousIsLowerOrDigit || bNextIsLower)
			{
				Result.AppendChar(TEXT('_'));
			}
		}

		Result.AppendChar(FChar::ToLower(Character));
	}

	return Result;
}

FString GetNormalizedMachineType(const AMachineBase* Machine)
{
	if (!Machine)
	{
		return TEXT("unknown");
	}

	if (Machine->IsA<APowerGridNode>())
	{
		return TEXT("power_pole");
	}

	if (Machine->IsA<APowerPlant>())
	{
		return TEXT("generator");
	}

	return NormalizeTypeName(Machine->GetMachineType().ToString());
}

FString MakeObjectIdString(const UObject* Object)
{
	return Object ? SanitizeObjectName(Object->GetName()) : FString();
}

TArray<TSharedPtr<FJsonValue>> MakeStringArray(const TArray<FString>& Values)
{
	TArray<TSharedPtr<FJsonValue>> JsonValues;
	JsonValues.Reserve(Values.Num());
	for (const FString& Value : Values)
	{
		JsonValues.Add(MakeShared<FJsonValueString>(Value));
	}

	return JsonValues;
}

FString MakeMachineIdString(const FMachineNode& MachineNode)
{
	const AMachineBase* Machine = MachineNode.MachineActor.Get();
	if (Machine)
	{
		return MakeObjectIdString(Machine);
	}

	return MachineNode.ID.IsNone() ? FString() : SanitizeObjectName(MachineNode.ID.ToString());
}

void SetStringMapField(
	const TSharedPtr<FJsonObject>& TargetObject,
	const FString& FieldName,
	const TMap<FName, int32>& SourceMap)
{
	const TSharedPtr<FJsonObject> MapObject = MakeShared<FJsonObject>();
	for (const TPair<FName, int32>& Pair : SourceMap)
	{
		if (!Pair.Key.IsNone())
		{
			MapObject->SetNumberField(Pair.Key.ToString(), Pair.Value);
		}
	}

	TargetObject->SetObjectField(FieldName, MapObject.ToSharedRef());
}
}

void UFactoryAgentClientSubsystem::Deinitialize()
{
	Disconnect();
	Super::Deinitialize();
}

void UFactoryAgentClientSubsystem::ConnectToDefaultServer()
{
	Connect(DefaultWebSocketUrl);
}

void UFactoryAgentClientSubsystem::Connect(const FString& WebSocketUrl)
{
	const FString ResolvedUrl = WebSocketUrl.TrimStartAndEnd().IsEmpty()
		? DefaultWebSocketUrl
		: WebSocketUrl.TrimStartAndEnd();

	if (Socket.IsValid())
	{
		if (Socket->IsConnected() && ResolvedUrl == DefaultWebSocketUrl)
		{
			return;
		}

		Disconnect();
	}

	DefaultWebSocketUrl = ResolvedUrl;
	ConnectionState = EFactoryAgentConnectionState::Connecting;

#if WITH_WEBSOCKETS
	Socket = FWebSocketsModule::Get().CreateWebSocket(ResolvedUrl);
	BindSocketEvents();
	Socket->Connect();
	LOG_LC(TEXT("Factory agent WebSocket connecting: %s"), *ResolvedUrl);
#else
	ConnectionState = EFactoryAgentConnectionState::Disconnected;
	const FString Error = TEXT("WebSockets are not available on this platform.");
	LOG_LC_E(TEXT("%s"), *Error);
	OnConnectionError.Broadcast(Error);
#endif
}

void UFactoryAgentClientSubsystem::Disconnect()
{
	if (Socket.IsValid())
	{
		Socket->Close();
		ResetSocket();
	}

	ConnectionState = EFactoryAgentConnectionState::Disconnected;
}

bool UFactoryAgentClientSubsystem::IsConnected() const
{
	return Socket.IsValid() && Socket->IsConnected();
}

EFactoryAgentConnectionState UFactoryAgentClientSubsystem::GetConnectionState() const
{
	return ConnectionState;
}

FString UFactoryAgentClientSubsystem::SendAgentRequest(const FString& Agent, const FString& PayloadJson)
{
	return SendAgentRequestInternal(
		Agent,
		PayloadJson,
		TEXT("{}"),
		DefaultSessionId,
		DefaultClientId);
}

FString UFactoryAgentClientSubsystem::SendAgentRequestWithContext(
	const FString& Agent,
	const FString& PayloadJson,
	const FString& ContextJson,
	const FString& SessionId,
	const FString& ClientId)
{
	return SendAgentRequestInternal(Agent, PayloadJson, ContextJson, SessionId, ClientId);
}

bool UFactoryAgentClientSubsystem::SendJsonMessage(const FString& JsonMessage)
{
	TSharedPtr<FJsonObject> MessageObject;
	if (!FactoryAgentJsonUtils::ParseJsonObject(JsonMessage, MessageObject))
	{
		LOG_LC_W(TEXT("Factory agent message must be a JSON object."));
		return false;
	}

	return SendRawMessage(FactoryAgentJsonUtils::WriteJsonObject(MessageObject));
}

bool UFactoryAgentClientSubsystem::SendQuestGeneratorRequest(
	const FString& RequestId,
	const FString& SessionId,
	const FString& ClientId)
{
	const TSharedPtr<FJsonObject> RequestObject = MakeShared<FJsonObject>();
	RequestObject->SetStringField(TEXT("type"), AgentRequestType);
	RequestObject->SetStringField(TEXT("request_id"), RequestId.IsEmpty() ? QuestSampleRequestId : RequestId);
	RequestObject->SetStringField(TEXT("session_id"), SessionId.IsEmpty() ? QuestSampleSessionId : SessionId);
	RequestObject->SetStringField(TEXT("client_id"), ClientId.IsEmpty() ? QuestSampleClientId : ClientId);
	RequestObject->SetStringField(TEXT("agent"), QuestGeneratorAgentId);

	return SendRawMessage(FactoryAgentJsonUtils::WriteJsonObject(RequestObject));
}

bool UFactoryAgentClientSubsystem::SendOperatorGuideQuestion(const FString& Question, const FString& ClientId)
{
	const FString TrimmedQuestion = Question.TrimStartAndEnd();
	if (TrimmedQuestion.IsEmpty())
	{
		LOG_LC_W(TEXT("Factory agent operator guide question is empty."));
		return false;
	}

	const TSharedPtr<FJsonObject> PayloadObject = MakeShared<FJsonObject>();
	PayloadObject->SetStringField(TEXT("question"), TrimmedQuestion);

	const TSharedPtr<FJsonObject> ContextObject = MakeShared<FJsonObject>();
	ContextObject->SetStringField(TEXT("language"), TEXT("ko"));
	ContextObject->SetStringField(TEXT("mode"), TEXT("gameplay"));

	const TSharedPtr<FJsonObject> RequestObject = MakeShared<FJsonObject>();
	RequestObject->SetStringField(TEXT("type"), AgentRequestType);
	RequestObject->SetStringField(TEXT("request_id"), OperatorGuideRequestId);
	RequestObject->SetStringField(TEXT("session_id"), OperatorGuideSessionId);
	RequestObject->SetStringField(TEXT("client_id"), ClientId.IsEmpty() ? OperatorGuideClientId : ClientId);
	RequestObject->SetStringField(TEXT("agent"), OperatorGuideAgentId);
	RequestObject->SetObjectField(TEXT("payload"), PayloadObject.ToSharedRef());
	RequestObject->SetObjectField(TEXT("context"), ContextObject.ToSharedRef());

	return SendRawMessage(FactoryAgentJsonUtils::WriteJsonObject(RequestObject));
}

bool UFactoryAgentClientSubsystem::SendProcessOptimizerStateUpdate(
	int32 FactoryRevision,
	const FString& SessionId,
	const FString& ClientId)
{
	return SendRawMessage(BuildProcessOptimizerStateUpdateJson(FactoryRevision, SessionId, ClientId));
}

void UFactoryAgentClientSubsystem::LogProcessOptimizerStateUpdateJson(
	int32 FactoryRevision,
	const FString& SessionId,
	const FString& ClientId)
{
	const FString PreviewJson = BuildProcessOptimizerStateUpdateJson(FactoryRevision, SessionId, ClientId);
	LOG_LC(TEXT("Process optimizer state update preview: %s"), *PreviewJson);
}

bool UFactoryAgentClientSubsystem::SaveProcessOptimizerStateUpdateJsonToDesktop(
	int32 FactoryRevision,
	const FString& SessionId,
	const FString& ClientId,
	FString& OutSavedFilePath)
{
	const FString PreviewJson = BuildProcessOptimizerStateUpdateJson(FactoryRevision, SessionId, ClientId);
	const FString UserProfilePath = FPlatformMisc::GetEnvironmentVariable(TEXT("USERPROFILE"));
	const FString DesktopDirectory = UserProfilePath.IsEmpty()
		? FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir())
		: FPaths::Combine(UserProfilePath, TEXT("Desktop"));

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*DesktopDirectory) && !PlatformFile.CreateDirectoryTree(*DesktopDirectory))
	{
		OutSavedFilePath.Reset();
		LOG_LC_W(TEXT("Failed to create directory for factory state preview: %s"), *DesktopDirectory);
		return false;
	}

	const FString FileName = FString::Printf(TEXT("process_optimizer_factory_state_%03d.json"), FMath::Max(0, FactoryRevision));
	const FString SavedFilePath = FPaths::Combine(DesktopDirectory, FileName);
	if (!FFileHelper::SaveStringToFile(PreviewJson, *SavedFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutSavedFilePath.Reset();
		LOG_LC_W(TEXT("Failed to save factory state preview file: %s"), *SavedFilePath);
		return false;
	}

	OutSavedFilePath = SavedFilePath;
	LOG_LC(TEXT("Process optimizer state update preview saved: %s"), *SavedFilePath);
	return true;
}

FString UFactoryAgentClientSubsystem::BuildProcessOptimizerStateUpdateJson(
	int32 FactoryRevision,
	const FString& SessionId,
	const FString& ClientId)
{
	const int32 SafeFactoryRevision = FMath::Max(0, FactoryRevision);
	const FString ResolvedSessionId = SessionId.IsEmpty() ? ProcessOptimizerSessionId : SessionId;
	const FString ResolvedClientId = ClientId.IsEmpty() ? ProcessOptimizerClientId : ClientId;

	TArray<FMachineNode> MachineNodes;
	TArray<FConnectionEdge> ConnectionEdges;
	TArray<FPowerConnectionEdge> PowerConnectionEdges;
	float TotalProducedPower = 0.0f;
	float TotalConsumedPower = 0.0f;

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFactoryManagerSubsystem* FactoryManager = GameInstance->GetSubsystem<UFactoryManagerSubsystem>())
		{
			FactoryManager->UpdatePowerGrid();
			MachineNodes = FactoryManager->GetMachineNodes();
			ConnectionEdges = FactoryManager->GetConnectionEdges();
			PowerConnectionEdges = FactoryManager->GetPowerConnectionEdges();
			TotalProducedPower = FactoryManager->GetLastTotalGeneratedPower();
			TotalConsumedPower = FactoryManager->GetLastTotalDemandPower();
		}
	}

	MachineNodes.Sort([](const FMachineNode& Left, const FMachineNode& Right)
	{
		return Left.ID.LexicalLess(Right.ID);
	});

	ConnectionEdges.Sort([](const FConnectionEdge& Left, const FConnectionEdge& Right)
	{
		return Left.ID.LexicalLess(Right.ID);
	});

	PowerConnectionEdges.Sort([](const FPowerConnectionEdge& Left, const FPowerConnectionEdge& Right)
	{
		return Left.ID.LexicalLess(Right.ID);
	});

	TMap<FString, FString> RawMachineIdToSanitizedId;
	TMap<FString, APowerGridNode*> PowerNodesById;
	for (const FMachineNode& MachineNode : MachineNodes)
	{
		const FString SanitizedMachineId = MakeMachineIdString(MachineNode);
		RawMachineIdToSanitizedId.Add(MachineNode.ID.ToString(), SanitizedMachineId);

		if (APowerGridNode* PowerNode = Cast<APowerGridNode>(MachineNode.MachineActor.Get()))
		{
			PowerNodesById.Add(SanitizedMachineId, PowerNode);
		}
	}

	TMap<FString, TArray<FString>> NodeToConnectedNodeIds;
	TMap<FString, TArray<FString>> NodeToConnectedMachineIds;
	TMap<FString, TArray<FString>> GeneratorToConnectedNodeIds;

	for (const FPowerConnectionEdge& PowerEdge : PowerConnectionEdges)
	{
		const FString SourceId = RawMachineIdToSanitizedId.FindRef(PowerEdge.SourceMachine.ToString());
		const FString TargetId = RawMachineIdToSanitizedId.FindRef(PowerEdge.TargetMachine.ToString());
		if (SourceId.IsEmpty() || TargetId.IsEmpty())
		{
			continue;
		}

		const bool bSourceIsNode = PowerNodesById.Contains(SourceId);
		const bool bTargetIsNode = PowerNodesById.Contains(TargetId);

		if (bSourceIsNode && bTargetIsNode)
		{
			NodeToConnectedNodeIds.FindOrAdd(SourceId).AddUnique(TargetId);
			NodeToConnectedNodeIds.FindOrAdd(TargetId).AddUnique(SourceId);
			continue;
		}

		if (bSourceIsNode && !bTargetIsNode)
		{
			NodeToConnectedMachineIds.FindOrAdd(SourceId).AddUnique(TargetId);
			GeneratorToConnectedNodeIds.FindOrAdd(TargetId).AddUnique(SourceId);
			continue;
		}

		if (!bSourceIsNode && bTargetIsNode)
		{
			NodeToConnectedMachineIds.FindOrAdd(TargetId).AddUnique(SourceId);
			GeneratorToConnectedNodeIds.FindOrAdd(SourceId).AddUnique(TargetId);
		}
	}

	TMap<FString, TArray<FString>> MachineToNearbyNodeIds;
	for (const FMachineNode& MachineNode : MachineNodes)
	{
		AMachineBase* Machine = MachineNode.MachineActor.Get();
		if (!Machine)
		{
			continue;
		}

		TArray<FString> NearbyNodeIds;
		for (const TPair<FString, APowerGridNode*>& NodePair : PowerNodesById)
		{
			const APowerGridNode* PowerNode = NodePair.Value;
			if (!PowerNode)
			{
				continue;
			}

			const float SupplyRadius = PowerNode->GetSupplyRadius();
			if (SupplyRadius <= 0.0f)
			{
				continue;
			}

			if (FVector::DistSquared(Machine->GetActorLocation(), PowerNode->GetActorLocation()) <=
				FMath::Square(SupplyRadius))
			{
				NearbyNodeIds.Add(NodePair.Key);
				NodeToConnectedMachineIds.FindOrAdd(NodePair.Key).AddUnique(MakeMachineIdString(MachineNode));
			}
		}

		NearbyNodeIds.Sort();
		MachineToNearbyNodeIds.Add(MakeMachineIdString(MachineNode), NearbyNodeIds);
	}

	for (TPair<FString, TArray<FString>>& Pair : NodeToConnectedNodeIds)
	{
		Pair.Value.Sort();
	}

	for (TPair<FString, TArray<FString>>& Pair : NodeToConnectedMachineIds)
	{
		Pair.Value.Sort();
	}

	for (TPair<FString, TArray<FString>>& Pair : GeneratorToConnectedNodeIds)
	{
		Pair.Value.Sort();
	}

	TArray<TSharedPtr<FJsonValue>> MachineArray;
	MachineArray.Reserve(MachineNodes.Num());
	for (const FMachineNode& MachineNode : MachineNodes)
	{
		AMachineBase* Machine = MachineNode.MachineActor.Get();
		if (!Machine)
		{
			continue;
		}

		const FString MachineId = MakeMachineIdString(MachineNode);
		const TSharedPtr<FJsonObject> MachineObject = MakeShared<FJsonObject>();
		MachineObject->SetStringField(TEXT("id"), MachineId);
		MachineObject->SetStringField(TEXT("type"), GetNormalizedMachineType(Machine));
		MachineObject->SetStringField(TEXT("status"), MachineStateToStatusString(Machine->GetMachineState()));
		MachineObject->SetArrayField(
			TEXT("connected_power_node_ids"),
			MakeStringArray(MachineToNearbyNodeIds.FindRef(MachineId)));

		if (const FRecipeTable& CurrentRecipe = Machine->GetCurrentRecipe(); !CurrentRecipe.OutputItem1.IsNone())
		{
			MachineObject->SetStringField(TEXT("recipe_id"), CurrentRecipe.OutputItem1.ToString());
		}

		if (Machine->NeedsPower())
		{
			MachineObject->SetNumberField(TEXT("power_consumption"), Machine->GetPowerConsumption());
		}

		if (APowerPlant* PowerPlant = Cast<APowerPlant>(Machine))
		{
			MachineObject->SetNumberField(TEXT("power_output"), PowerPlant->GetCurrentPowerOutput());
		}
		else
		{
			MachineObject->SetNumberField(TEXT("operating_rate"), Machine->GetMachineState() == EMachineState::Working ? 1.0 : 0.0);
		}

		SetStringMapField(MachineObject, TEXT("input_inventory"), Machine->GetInputInventory());
		SetStringMapField(MachineObject, TEXT("output_buffer"), Machine->GetOutputBuffer());

		MachineArray.Add(MakeShared<FJsonValueObject>(MachineObject));
	}

	TArray<TSharedPtr<FJsonValue>> ConveyorArray;
	ConveyorArray.Reserve(ConnectionEdges.Num());
	for (const FConnectionEdge& ConnectionEdge : ConnectionEdges)
	{
		const TSharedPtr<FJsonObject> ConveyorObject = MakeShared<FJsonObject>();
		ConveyorObject->SetStringField(
			TEXT("source_machine_id"),
			RawMachineIdToSanitizedId.FindRef(ConnectionEdge.SourceMachine.ToString()));
		ConveyorObject->SetStringField(
			TEXT("target_machine_id"),
			RawMachineIdToSanitizedId.FindRef(ConnectionEdge.TargetMachine.ToString()));

		if (AConveyor* Conveyor = ConnectionEdge.ConveyorActor.Get())
		{
			ConveyorObject->SetStringField(TEXT("id"), MakeObjectIdString(Conveyor));
			ConveyorObject->SetNumberField(TEXT("path_cell_count"), Conveyor->GetPathCells().Num());

			TArray<FString> ItemSlotStrings;
			const TArray<FName>& ItemSlots = Conveyor->GetItemSlotsForSave();
			ItemSlotStrings.Reserve(ItemSlots.Num());
			for (const FName& ItemSlot : ItemSlots)
			{
				ItemSlotStrings.Add(ItemSlot.IsNone() ? FString() : ItemSlot.ToString());
			}

			ConveyorObject->SetArrayField(TEXT("item_slots"), MakeStringArray(ItemSlotStrings));
			ConveyorObject->SetBoolField(TEXT("is_blocked"), Conveyor->IsOutputBlocked());
		}
		else
		{
			ConveyorObject->SetStringField(TEXT("id"), SanitizeObjectName(ConnectionEdge.ID.ToString()));
		}

		ConveyorArray.Add(MakeShared<FJsonValueObject>(ConveyorObject));
	}

	TArray<TSharedPtr<FJsonValue>> PowerNodeArray;
	for (const TPair<FString, APowerGridNode*>& NodePair : PowerNodesById)
	{
		const TSharedPtr<FJsonObject> NodeObject = MakeShared<FJsonObject>();
		NodeObject->SetStringField(TEXT("id"), NodePair.Key);
		NodeObject->SetStringField(TEXT("type"), TEXT("power_pole"));
		NodeObject->SetArrayField(TEXT("connected_node_ids"), MakeStringArray(NodeToConnectedNodeIds.FindRef(NodePair.Key)));
		NodeObject->SetArrayField(TEXT("connected_machine_ids"), MakeStringArray(NodeToConnectedMachineIds.FindRef(NodePair.Key)));
		PowerNodeArray.Add(MakeShared<FJsonValueObject>(NodeObject));
	}

	TArray<TSharedPtr<FJsonValue>> GeneratorArray;
	for (const FMachineNode& MachineNode : MachineNodes)
	{
		APowerPlant* PowerPlant = Cast<APowerPlant>(MachineNode.MachineActor.Get());
		if (!PowerPlant)
		{
			continue;
		}

		const FString GeneratorId = MakeMachineIdString(MachineNode);
		const TArray<FString> ConnectedNodeIds = GeneratorToConnectedNodeIds.FindRef(GeneratorId);
		const TSharedPtr<FJsonObject> GeneratorObject = MakeShared<FJsonObject>();
		GeneratorObject->SetStringField(TEXT("id"), GeneratorId);
		GeneratorObject->SetNumberField(TEXT("produced"), PowerPlant->GetCurrentPowerOutput());
		GeneratorObject->SetBoolField(TEXT("connected"), ConnectedNodeIds.Num() > 0);
		GeneratorObject->SetArrayField(TEXT("connected_power_node_ids"), MakeStringArray(ConnectedNodeIds));
		GeneratorArray.Add(MakeShared<FJsonValueObject>(GeneratorObject));
	}

	const TSharedPtr<FJsonObject> SummaryObject = MakeShared<FJsonObject>();
	SummaryObject->SetNumberField(TEXT("machine_count"), MachineArray.Num());
	SummaryObject->SetNumberField(TEXT("conveyor_count"), ConveyorArray.Num());
	SummaryObject->SetNumberField(TEXT("power_node_count"), PowerNodeArray.Num());
	SummaryObject->SetNumberField(TEXT("power_line_count"), PowerConnectionEdges.Num());
	SummaryObject->SetNumberField(TEXT("factory_revision"), SafeFactoryRevision);

	const TSharedPtr<FJsonObject> PowerGridObject = MakeShared<FJsonObject>();
	PowerGridObject->SetNumberField(TEXT("produced"), TotalProducedPower);
	PowerGridObject->SetNumberField(TEXT("consumed"), TotalConsumedPower);
	PowerGridObject->SetArrayField(TEXT("nodes"), PowerNodeArray);
	PowerGridObject->SetArrayField(TEXT("generators"), GeneratorArray);

	const TSharedPtr<FJsonObject> FactoryStateObject = MakeShared<FJsonObject>();
	FactoryStateObject->SetObjectField(TEXT("summary"), SummaryObject.ToSharedRef());
	FactoryStateObject->SetArrayField(TEXT("machines"), MachineArray);
	FactoryStateObject->SetArrayField(TEXT("conveyors"), ConveyorArray);
	FactoryStateObject->SetObjectField(TEXT("power_grid"), PowerGridObject.ToSharedRef());

	const TSharedPtr<FJsonObject> PayloadObject = MakeShared<FJsonObject>();
	PayloadObject->SetStringField(TEXT("operation"), TEXT("state_update"));
	PayloadObject->SetStringField(TEXT("goal"), TEXT("balance"));
	PayloadObject->SetNumberField(TEXT("factoryRevision"), SafeFactoryRevision);
	PayloadObject->SetObjectField(TEXT("factory_state"), FactoryStateObject.ToSharedRef());

	const TSharedPtr<FJsonObject> ContextObject = MakeShared<FJsonObject>();
	ContextObject->SetStringField(TEXT("language"), TEXT("ko"));
	ContextObject->SetStringField(TEXT("mode"), TEXT("gameplay"));

	const TSharedPtr<FJsonObject> RequestObject = MakeShared<FJsonObject>();
	RequestObject->SetStringField(TEXT("type"), AgentRequestType);
	RequestObject->SetStringField(
		TEXT("request_id"),
		FString::Printf(TEXT("unreal-optimizer-state-%03d"), SafeFactoryRevision));
	RequestObject->SetStringField(TEXT("session_id"), ResolvedSessionId);
	RequestObject->SetStringField(TEXT("client_id"), ResolvedClientId);
	RequestObject->SetStringField(TEXT("agent"), ProcessOptimizerAgentId);
	RequestObject->SetObjectField(TEXT("payload"), PayloadObject.ToSharedRef());
	RequestObject->SetObjectField(TEXT("context"), ContextObject.ToSharedRef());

	return FactoryAgentJsonUtils::WriteJsonObject(RequestObject);
}

bool UFactoryAgentClientSubsystem::SendRawMessage(const FString& RawMessage)
{
	if (!IsConnected())
	{
		LOG_LC_W(TEXT("Factory agent WebSocket is not connected."));
		return false;
	}

	Socket->Send(RawMessage);
	return true;
}

FString UFactoryAgentClientSubsystem::SendAgentRequestInternal(
	const FString& Agent,
	const FString& PayloadJson,
	const FString& ContextJson,
	const FString& SessionId,
	const FString& ClientId)
{
	TSharedPtr<FJsonObject> PayloadObject;
	if (!FactoryAgentJsonUtils::ParseJsonObject(PayloadJson, PayloadObject))
	{
		LOG_LC_W(TEXT("Factory agent payload must be a JSON object."));
		return FString();
	}

	TSharedPtr<FJsonObject> ContextObject;
	if (!FactoryAgentJsonUtils::ParseJsonObject(ContextJson, ContextObject))
	{
		LOG_LC_W(TEXT("Factory agent context must be a JSON object."));
		return FString();
	}

	const FString RequestId = FString::Printf(
		TEXT("ue-%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));

	const TSharedPtr<FJsonObject> RequestObject = MakeShared<FJsonObject>();
	RequestObject->SetStringField(TEXT("type"), AgentRequestType);
	RequestObject->SetStringField(TEXT("request_id"), RequestId);
	RequestObject->SetStringField(TEXT("session_id"), SessionId.IsEmpty() ? DefaultSessionId : SessionId);
	RequestObject->SetStringField(TEXT("client_id"), ClientId.IsEmpty() ? DefaultClientId : ClientId);
	RequestObject->SetStringField(TEXT("agent"), Agent);
	RequestObject->SetObjectField(TEXT("payload"), PayloadObject.ToSharedRef());
	RequestObject->SetObjectField(TEXT("context"), ContextObject.ToSharedRef());

	if (!SendRawMessage(FactoryAgentJsonUtils::WriteJsonObject(RequestObject)))
	{
		return FString();
	}

	return RequestId;
}

void UFactoryAgentClientSubsystem::BindSocketEvents()
{
	if (!Socket.IsValid())
	{
		return;
	}

	Socket->OnConnected().AddUObject(this, &UFactoryAgentClientSubsystem::HandleSocketConnected);
	Socket->OnConnectionError().AddUObject(this, &UFactoryAgentClientSubsystem::HandleSocketConnectionError);
	Socket->OnClosed().AddUObject(this, &UFactoryAgentClientSubsystem::HandleSocketClosed);
	Socket->OnMessage().AddUObject(this, &UFactoryAgentClientSubsystem::HandleSocketMessage);
}

void UFactoryAgentClientSubsystem::HandleSocketConnected()
{
	ConnectionState = EFactoryAgentConnectionState::Connected;
	LOG_LC(TEXT("Factory agent WebSocket connected."));
	OnConnected.Broadcast();
}

void UFactoryAgentClientSubsystem::HandleSocketConnectionError(const FString& Error)
{
	ConnectionState = EFactoryAgentConnectionState::Disconnected;
	LOG_LC_E(TEXT("Factory agent WebSocket connection error: %s"), *Error);
	OnConnectionError.Broadcast(Error);
	ResetSocket();
}

void UFactoryAgentClientSubsystem::HandleSocketClosed(int32 StatusCode, const FString& Reason, bool bWasClean)
{
	ConnectionState = EFactoryAgentConnectionState::Disconnected;
	LOG_LC_W(
		TEXT("Factory agent WebSocket closed. Code=%d Clean=%s Reason=%s"),
		StatusCode,
		bWasClean ? TEXT("true") : TEXT("false"),
		*Reason);
	OnClosed.Broadcast(StatusCode, Reason, bWasClean);
	ResetSocket();
}

void UFactoryAgentClientSubsystem::HandleSocketMessage(const FString& Message)
{
	LOG_LC(TEXT("Factory agent WebSocket received: %s"), *Message);
	OnRawMessageReceived.Broadcast(Message);

	TSharedPtr<FJsonObject> RootObject;
	if (!FactoryAgentJsonUtils::ParseJsonObject(Message, RootObject))
	{
		LOG_LC_W(TEXT("Factory agent response was not valid JSON."));
		return;
	}

	const FString Type = FactoryAgentJsonUtils::GetStringField(RootObject, TEXT("type"));
	const FString RequestId = FactoryAgentJsonUtils::GetStringField(RootObject, TEXT("request_id"));
	const FString Agent = FactoryAgentJsonUtils::GetStringField(RootObject, TEXT("agent"));

	if (Type == AgentResponseType)
	{
		OnAgentResponseReceived.Broadcast(
			RequestId,
			Agent,
			FactoryAgentJsonUtils::WriteJsonObject(FactoryAgentJsonUtils::GetObjectField(RootObject, TEXT("payload"))),
			Message);
		return;
	}

	if (Type == AgentErrorType)
	{
		const TSharedPtr<FJsonObject> ErrorObject = FactoryAgentJsonUtils::GetObjectField(RootObject, TEXT("error"));
		OnAgentErrorReceived.Broadcast(
			RequestId,
			Agent,
			FactoryAgentJsonUtils::GetStringField(ErrorObject, TEXT("code")),
			FactoryAgentJsonUtils::GetStringField(ErrorObject, TEXT("message")),
			Message);
		return;
	}

	if (Type == AgentProgressType)
	{
		const TSharedPtr<FJsonObject> PayloadObject = FactoryAgentJsonUtils::GetObjectField(RootObject, TEXT("payload"));
		OnAgentProgressReceived.Broadcast(
			RequestId,
			Agent,
			FactoryAgentJsonUtils::GetStringField(PayloadObject, TEXT("stage")),
			FactoryAgentJsonUtils::GetStringField(PayloadObject, TEXT("message")),
			Message);
		return;
	}

	LOG_LC_W(TEXT("Unknown factory agent message type: %s"), *Type);
}

void UFactoryAgentClientSubsystem::ResetSocket()
{
	if (Socket.IsValid())
	{
		Socket->OnConnected().RemoveAll(this);
		Socket->OnConnectionError().RemoveAll(this);
		Socket->OnClosed().RemoveAll(this);
		Socket->OnMessage().RemoveAll(this);
		Socket.Reset();
	}
}
