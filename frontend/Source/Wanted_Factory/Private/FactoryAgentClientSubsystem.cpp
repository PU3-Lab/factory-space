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
constexpr TCHAR MaterialGenerationAgentId[] = TEXT("material_generation");
constexpr TCHAR ProcessOptimizerAgentId[] = TEXT("process_optimizer");
constexpr TCHAR QuestSampleRequestId[] = TEXT("request-quest-sample");
constexpr TCHAR QuestSampleSessionId[] = TEXT("smoke-session");
constexpr TCHAR QuestSampleClientId[] = TEXT("smoke-client");
constexpr TCHAR OperatorGuideRequestId[] = TEXT("operator-guide-demo-multi-001");
constexpr TCHAR OperatorGuideSessionId[] = TEXT("operator-guide-demo-session");
constexpr TCHAR OperatorGuideClientId[] = TEXT("unreal-client");
constexpr TCHAR MaterialGenerationSessionId[] = TEXT("player-session-001");
constexpr TCHAR MaterialGenerationClientId[] = TEXT("unreal-client");
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
	if (ObjectName.IsEmpty())
	{
		return ObjectName;
	}

	int32 ArrowIndex = INDEX_NONE;
	if (ObjectName.FindLastChar(TEXT('>'), ArrowIndex))
	{
		ObjectName = ObjectName.Mid(ArrowIndex + 1);
	}

	int32 LastSeparatorIndex = INDEX_NONE;
	for (const TCHAR Separator : { TEXT('/'), TEXT('\\'), TEXT(':'), TEXT('.') })
	{
		int32 SeparatorIndex = INDEX_NONE;
		if (ObjectName.FindLastChar(Separator, SeparatorIndex))
		{
			LastSeparatorIndex = FMath::Max(LastSeparatorIndex, SeparatorIndex);
		}
	}

	if (LastSeparatorIndex != INDEX_NONE)
	{
		ObjectName = ObjectName.Mid(LastSeparatorIndex + 1);
	}

	while (ObjectName.RemoveFromStart(TEXT("BP_")))
	{
	}

	int32 NumericSuffixSeparatorIndex = INDEX_NONE;
	if (ObjectName.FindLastChar(TEXT('_'), NumericSuffixSeparatorIndex))
	{
		const FString InstanceSuffix = ObjectName.Mid(NumericSuffixSeparatorIndex + 1);
		if (!InstanceSuffix.IsEmpty() && InstanceSuffix.IsNumeric())
		{
			FString BaseName = ObjectName.Left(NumericSuffixSeparatorIndex);
			if (BaseName.EndsWith(TEXT("_C"), ESearchCase::CaseSensitive))
			{
				BaseName.LeftChopInline(2, EAllowShrinking::No);
			}

			ObjectName = BaseName + TEXT("_") + InstanceSuffix;
		}
	}

	ObjectName.ReplaceInline(TEXT("_C_"), TEXT("_"), ESearchCase::CaseSensitive);
	if (ObjectName.EndsWith(TEXT("_C"), ESearchCase::CaseSensitive))
	{
		ObjectName.LeftChopInline(2, EAllowShrinking::No);
	}

	while (ObjectName.Contains(TEXT("__")))
	{
		ObjectName.ReplaceInline(TEXT("__"), TEXT("_"), ESearchCase::CaseSensitive);
	}

	ObjectName.TrimStartAndEndInline();

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

FString SanitizeFileToken(FString Value)
{
	Value.TrimStartAndEndInline();
	if (Value.IsEmpty())
	{
		return TEXT("unknown");
	}

	for (const TCHAR InvalidCharacter : { TEXT('<'), TEXT('>'), TEXT(':'), TEXT('"'), TEXT('/'), TEXT('\\'), TEXT('|'), TEXT('?'), TEXT('*') })
	{
		const TCHAR SearchText[] = { InvalidCharacter, TEXT('\0') };
		Value.ReplaceInline(SearchText, TEXT("_"), ESearchCase::CaseSensitive);
	}

	while (Value.Contains(TEXT("__")))
	{
		Value.ReplaceInline(TEXT("__"), TEXT("_"), ESearchCase::CaseSensitive);
	}

	return Value;
}

bool SaveJsonToDesktop(const FString& FileName, const FString& JsonText, FString& OutSavedFilePath)
{
	const FString UserProfilePath = FPlatformMisc::GetEnvironmentVariable(TEXT("USERPROFILE"));
	const FString DesktopDirectory = UserProfilePath.IsEmpty()
		? FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir())
		: FPaths::Combine(UserProfilePath, TEXT("Desktop"));

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*DesktopDirectory) && !PlatformFile.CreateDirectoryTree(*DesktopDirectory))
	{
		OutSavedFilePath.Reset();
		return false;
	}

	const FString SavedFilePath = FPaths::Combine(DesktopDirectory, FileName);
	if (!FFileHelper::SaveStringToFile(JsonText, *SavedFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutSavedFilePath.Reset();
		return false;
	}

	OutSavedFilePath = SavedFilePath;
	return true;
}

bool TryGetBoolField(const TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName, bool DefaultValue = false)
{
	bool Value = DefaultValue;
	if (JsonObject.IsValid())
	{
		JsonObject->TryGetBoolField(FieldName, Value);
	}

	return Value;
}

FString GetFirstNonEmptyStringField(const TSharedPtr<FJsonObject>& JsonObject, std::initializer_list<const TCHAR*> FieldNames)
{
	for (const TCHAR* FieldName : FieldNames)
	{
		const FString Value = FactoryAgentJsonUtils::GetStringField(JsonObject, FieldName);
		if (!Value.IsEmpty())
		{
			return Value;
		}
	}

	return FString();
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

bool ShouldSanitizeIdentifierField(const FString& FieldName)
{
	return FieldName.Equals(TEXT("id"), ESearchCase::IgnoreCase) ||
		FieldName.EndsWith(TEXT("_id"), ESearchCase::IgnoreCase) ||
		FieldName.EndsWith(TEXT("_ids"), ESearchCase::IgnoreCase);
}

void SanitizeJsonIdentifierArray(const FString& FieldName, const TArray<TSharedPtr<FJsonValue>>& SourceValues, TArray<TSharedPtr<FJsonValue>>& OutValues);

void SanitizeJsonIdentifierObject(const TSharedPtr<FJsonObject>& JsonObject)
{
	if (!JsonObject.IsValid())
	{
		return;
	}

	for (TPair<FString, TSharedPtr<FJsonValue>>& Pair : JsonObject->Values)
	{
		if (!Pair.Value.IsValid())
		{
			continue;
		}

		if (ShouldSanitizeIdentifierField(Pair.Key))
		{
			if (Pair.Value->Type == EJson::String)
			{
				Pair.Value = MakeShared<FJsonValueString>(SanitizeObjectName(Pair.Value->AsString()));
			}
			else if (Pair.Value->Type == EJson::Array)
			{
				TArray<TSharedPtr<FJsonValue>> SanitizedValues;
				SanitizeJsonIdentifierArray(Pair.Key, Pair.Value->AsArray(), SanitizedValues);
				Pair.Value = MakeShared<FJsonValueArray>(SanitizedValues);
			}
			else if (Pair.Value->Type == EJson::Object)
			{
				SanitizeJsonIdentifierObject(Pair.Value->AsObject());
			}
			continue;
		}

		if (Pair.Value->Type == EJson::Object)
		{
			SanitizeJsonIdentifierObject(Pair.Value->AsObject());
		}
		else if (Pair.Value->Type == EJson::Array)
		{
			TArray<TSharedPtr<FJsonValue>> SanitizedValues;
			SanitizeJsonIdentifierArray(Pair.Key, Pair.Value->AsArray(), SanitizedValues);
			Pair.Value = MakeShared<FJsonValueArray>(SanitizedValues);
		}
	}
}

void SanitizeJsonIdentifierArray(const FString& FieldName, const TArray<TSharedPtr<FJsonValue>>& SourceValues, TArray<TSharedPtr<FJsonValue>>& OutValues)
{
	OutValues.Reset();
	OutValues.Reserve(SourceValues.Num());

	for (const TSharedPtr<FJsonValue>& Value : SourceValues)
	{
		if (!Value.IsValid())
		{
			continue;
		}

		if (ShouldSanitizeIdentifierField(FieldName) && Value->Type == EJson::String)
		{
			OutValues.Add(MakeShared<FJsonValueString>(SanitizeObjectName(Value->AsString())));
		}
		else if (Value->Type == EJson::Object)
		{
			TSharedPtr<FJsonObject> ObjectValue = Value->AsObject();
			SanitizeJsonIdentifierObject(ObjectValue);
			OutValues.Add(MakeShared<FJsonValueObject>(ObjectValue));
		}
		else if (Value->Type == EJson::Array)
		{
			TArray<TSharedPtr<FJsonValue>> NestedValues;
			SanitizeJsonIdentifierArray(FieldName, Value->AsArray(), NestedValues);
			OutValues.Add(MakeShared<FJsonValueArray>(NestedValues));
		}
		else
		{
			OutValues.Add(Value);
		}
	}
}

TSharedPtr<FJsonObject> BuildDurabilityObject(const AMachineBase* Machine)
{
	const TSharedPtr<FJsonObject> DurabilityObject = MakeShared<FJsonObject>();
	if (!Machine)
	{
		DurabilityObject->SetNumberField(TEXT("current"), 0.0);
		DurabilityObject->SetNumberField(TEXT("max"), 0.0);
		DurabilityObject->SetNumberField(TEXT("ratio"), 0.0);
		DurabilityObject->SetBoolField(TEXT("is_broken"), false);
		DurabilityObject->SetBoolField(TEXT("is_infinite"), false);
		return DurabilityObject;
	}

	const double MaxDurability = Machine->GetMaxDurability();
	const double CurrentDurability = Machine->GetCurrentDurability();
	DurabilityObject->SetNumberField(TEXT("current"), CurrentDurability);
	DurabilityObject->SetNumberField(TEXT("max"), MaxDurability);
	DurabilityObject->SetNumberField(
		TEXT("ratio"),
		MaxDurability > 0.0 ? CurrentDurability / MaxDurability : 0.0);
	DurabilityObject->SetBoolField(TEXT("is_broken"), Machine->isBroken());
	DurabilityObject->SetBoolField(TEXT("is_infinite"), Machine->IsInfiniteDurability());
	return DurabilityObject;
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

bool UFactoryAgentClientSubsystem::SendMaterialGenerationRequest(
	const TArray<FFactoryMaterialRequestInput>& Inputs,
	const FString& PlayerId,
	bool bGenerateVisualAsset,
	const FString& ClientId)
{
	static_cast<void>(PlayerId);

	const FFactoryMaterialProcessConditions ProcessConditions;
	return SendMaterialGenerationRequestAdvanced(
		Inputs,
		ProcessConditions,
		bGenerateVisualAsset,
		FString::Printf(TEXT("material-exp-%s"), *FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower)),
		MaterialGenerationSessionId,
		ClientId,
		1);
}

bool UFactoryAgentClientSubsystem::SendMaterialGenerationRequestAdvanced(
	const TArray<FFactoryMaterialRequestInput>& Inputs,
	const FFactoryMaterialProcessConditions& ProcessConditions,
	bool bGenerateVisualAsset,
	const FString& RequestId,
	const FString& SessionId,
	const FString& ClientId,
	int32 ContextTemperature)
{
	const TSharedPtr<FJsonObject> RequestObject = BuildMaterialGenerationRequestObject(
		Inputs,
		ProcessConditions,
		bGenerateVisualAsset,
		RequestId,
		SessionId,
		ClientId,
		ContextTemperature);
	if (!RequestObject.IsValid())
	{
		return false;
	}

	const FString RawMessage = FactoryAgentJsonUtils::WriteJsonObject(RequestObject);
	if (!SendRawMessage(RawMessage))
	{
		return false;
	}

	FFactoryPendingMaterialGenerationRequest& PendingRequest =
		PendingMaterialGenerationRequests.FindOrAdd(
			FactoryAgentJsonUtils::GetStringField(RequestObject, TEXT("request_id")));
	PendingRequest.RequestId = FactoryAgentJsonUtils::GetStringField(RequestObject, TEXT("request_id"));
	PendingRequest.SessionId = FactoryAgentJsonUtils::GetStringField(RequestObject, TEXT("session_id"));
	PendingRequest.ClientId = FactoryAgentJsonUtils::GetStringField(RequestObject, TEXT("client_id"));
	PendingRequest.ProcessConditions = ProcessConditions;
	PendingRequest.bGenerateVisualAsset = bGenerateVisualAsset;
	PendingRequest.ContextTemperature = ContextTemperature;
	PendingRequest.Inputs.Reset();
	for (const FFactoryMaterialRequestInput& Input : Inputs)
	{
		if (Input.ItemId.IsNone() || Input.Quantity <= 0)
		{
			continue;
		}

		PendingRequest.Inputs.Add(Input);
	}

	return true;
}

FString UFactoryAgentClientSubsystem::BuildMaterialGenerationRequestJson(
	const TArray<FFactoryMaterialRequestInput>& Inputs,
	const FFactoryMaterialProcessConditions& ProcessConditions,
	bool bGenerateVisualAsset,
	const FString& RequestId,
	const FString& SessionId,
	const FString& ClientId,
	int32 ContextTemperature) const
{
	const TSharedPtr<FJsonObject> RequestObject = BuildMaterialGenerationRequestObject(
		Inputs,
		ProcessConditions,
		bGenerateVisualAsset,
		RequestId,
		SessionId,
		ClientId,
		ContextTemperature);
	return FactoryAgentJsonUtils::WriteJsonObject(RequestObject);
}

bool UFactoryAgentClientSubsystem::ConsumePendingMaterialGenerationRequest(
	const FString& RequestId,
	FFactoryPendingMaterialGenerationRequest& OutRequest)
{
	if (FFactoryPendingMaterialGenerationRequest* PendingRequest =
		PendingMaterialGenerationRequests.Find(RequestId))
	{
		OutRequest = *PendingRequest;
		PendingMaterialGenerationRequests.Remove(RequestId);
		return true;
	}

	return false;
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
	TSharedPtr<FJsonObject> PreviewObject;
	const FString JsonToSave = FactoryAgentJsonUtils::ParseJsonObject(PreviewJson, PreviewObject)
		? FactoryAgentJsonUtils::WritePrettyJsonObject(PreviewObject)
		: PreviewJson;
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
	if (!FFileHelper::SaveStringToFile(JsonToSave, *SavedFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
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
	return BuildProcessOptimizerRequestJson(
		FactoryRevision,
		SessionId,
		ClientId,
		TEXT("state_update"),
		TEXT("periodic"),
		TEXT("optimizer-state"));
}

bool UFactoryAgentClientSubsystem::SendProcessOptimizerAnalyzeRequest(
	int32 FactoryRevision,
	const FString& SessionId,
	const FString& ClientId)
{
	return SendRawMessage(
		BuildProcessOptimizerAnalyzeRequestJson(
			FactoryRevision,
			SessionId,
			ClientId));
}

FString UFactoryAgentClientSubsystem::BuildProcessOptimizerAnalyzeRequestJson(
	int32 FactoryRevision,
	const FString& SessionId,
	const FString& ClientId)
{
	return BuildProcessOptimizerRequestJson(
		FactoryRevision,
		SessionId,
		ClientId,
		TEXT("analyze"),
		TEXT("player"),
		TEXT("optimizer-analyze"));
}

FString UFactoryAgentClientSubsystem::BuildProcessOptimizerRequestJson(
	int32 FactoryRevision,
	const FString& SessionId,
	const FString& ClientId,
	const FString& Operation,
	const FString& RequestSource,
	const FString& RequestIdPrefix)
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
		if (SourceId == TargetId)
		{
			continue;
		}

		if (bSourceIsNode && bTargetIsNode)
		{
			NodeToConnectedNodeIds.FindOrAdd(SourceId).AddUnique(TargetId);
			NodeToConnectedNodeIds.FindOrAdd(TargetId).AddUnique(SourceId);
			continue;
		}

		if (bSourceIsNode && !bTargetIsNode)
		{
			GeneratorToConnectedNodeIds.FindOrAdd(TargetId).AddUnique(SourceId);
			continue;
		}

		if (!bSourceIsNode && bTargetIsNode)
		{
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
		const bool bCanReceivePowerFromNodeRadius = Machine->NeedsPower();
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

			if (Machine == PowerNode)
			{
				continue;
			}

			if (FVector::DistSquared(Machine->GetActorLocation(), PowerNode->GetActorLocation()) <=
				FMath::Square(SupplyRadius))
			{
				if (bCanReceivePowerFromNodeRadius)
				{
					NearbyNodeIds.Add(NodePair.Key);
					NodeToConnectedMachineIds.FindOrAdd(NodePair.Key).AddUnique(MakeMachineIdString(MachineNode));
				}
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
		const FString MachineStatus = MachineStateToStatusString(Machine->GetMachineState());
		const TSharedPtr<FJsonObject> MachineObject = MakeShared<FJsonObject>();
		MachineObject->SetStringField(TEXT("id"), MachineId);
		MachineObject->SetStringField(TEXT("type"), GetNormalizedMachineType(Machine));
		MachineObject->SetStringField(TEXT("status"), MachineStatus);
		MachineObject->SetStringField(TEXT("equipment_status"), MachineStatus);
		MachineObject->SetObjectField(TEXT("durability"), BuildDurabilityObject(Machine).ToSharedRef());
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
		const FString GeneratorStatus = MachineStateToStatusString(PowerPlant->GetMachineState());
		const TSharedPtr<FJsonObject> GeneratorObject = MakeShared<FJsonObject>();
		GeneratorObject->SetStringField(TEXT("id"), GeneratorId);
		GeneratorObject->SetStringField(TEXT("equipment_status"), GeneratorStatus);
		GeneratorObject->SetObjectField(TEXT("durability"), BuildDurabilityObject(PowerPlant).ToSharedRef());
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
	SanitizeJsonIdentifierObject(FactoryStateObject);

	const TSharedPtr<FJsonObject> PayloadObject = MakeShared<FJsonObject>();
	PayloadObject->SetStringField(TEXT("operation"), Operation);
	PayloadObject->SetStringField(TEXT("request_source"), RequestSource);
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
		FString::Printf(TEXT("%s-%03d"), *RequestIdPrefix, SafeFactoryRevision));
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

	LOG_LC(TEXT("Factory agent WebSocket sending: %s"), *RawMessage);
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
		const TSharedPtr<FJsonObject> PayloadObject = FactoryAgentJsonUtils::GetObjectField(RootObject, TEXT("payload"));
		if (Agent == MaterialGenerationAgentId)
		{
			FString SavedFilePath;
			const FString RequestToken = SanitizeFileToken(RequestId);
			if (SaveJsonToDesktop(
				FString::Printf(TEXT("material_generation_response_%s.json"), *RequestToken),
				Message,
				SavedFilePath))
			{
				LOG_LC(TEXT("Material generation response saved: %s"), *SavedFilePath);
			}

			FFactoryMaterialGenerationResponse MaterialResponse;
			if (TryBuildMaterialGenerationResponse(RequestId, Agent, PayloadObject, MaterialResponse))
			{
				OnMaterialGenerationResponseReceived.Broadcast(MaterialResponse);
			}
		}

		OnAgentResponseReceived.Broadcast(
			RequestId,
			Agent,
			FactoryAgentJsonUtils::WriteJsonObject(PayloadObject),
			Message);
		return;
	}

	if (Type == AgentErrorType)
	{
		const TSharedPtr<FJsonObject> ErrorObject = FactoryAgentJsonUtils::GetObjectField(RootObject, TEXT("error"));
		if (Agent == MaterialGenerationAgentId)
		{
			FString SavedFilePath;
			const FString RequestToken = SanitizeFileToken(RequestId);
			if (SaveJsonToDesktop(
				FString::Printf(TEXT("material_generation_error_%s.json"), *RequestToken),
				Message,
				SavedFilePath))
			{
				LOG_LC(TEXT("Material generation error saved: %s"), *SavedFilePath);
			}

			PendingMaterialGenerationRequests.Remove(RequestId);
		}
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

bool UFactoryAgentClientSubsystem::TryBuildMaterialGenerationResponse(
	const FString& RequestId,
	const FString& Agent,
	const TSharedPtr<FJsonObject>& PayloadObject,
	FFactoryMaterialGenerationResponse& OutResponse) const
{
	if (!PayloadObject.IsValid())
	{
		return false;
	}

	OutResponse.RequestId = RequestId;
	OutResponse.Agent = Agent;
	OutResponse.ResultType = FactoryAgentJsonUtils::GetStringField(PayloadObject, TEXT("result_type"));
	OutResponse.ExperimentHash = FactoryAgentJsonUtils::GetStringField(PayloadObject, TEXT("experiment_hash"));
	OutResponse.RecipeName = FactoryAgentJsonUtils::GetStringField(PayloadObject, TEXT("recipe_name"));
	OutResponse.MaterialId = FactoryAgentJsonUtils::GetStringField(PayloadObject, TEXT("material_id"));
	OutResponse.Name = FactoryAgentJsonUtils::GetStringField(PayloadObject, TEXT("name"));
	OutResponse.State = FactoryAgentJsonUtils::GetStringField(PayloadObject, TEXT("state"));
	OutResponse.GenerationStatus = FactoryAgentJsonUtils::GetStringField(PayloadObject, TEXT("generation_status"));
	OutResponse.VisualStatus = FactoryAgentJsonUtils::GetStringField(PayloadObject, TEXT("visual_status"));
	OutResponse.VisualAssetKey = FactoryAgentJsonUtils::GetStringField(PayloadObject, TEXT("visual_asset_key"));
	OutResponse.TextureAssetKey = FactoryAgentJsonUtils::GetStringField(PayloadObject, TEXT("texture_asset_key"));
	OutResponse.ThumbnailAssetKey = FactoryAgentJsonUtils::GetStringField(PayloadObject, TEXT("thumbnail_asset_key"));
	OutResponse.FallbackIcon = FactoryAgentJsonUtils::GetStringField(PayloadObject, TEXT("fallback_icon"));
	OutResponse.RowName = GetFirstNonEmptyStringField(PayloadObject, { TEXT("rowname"), TEXT("row_name") });
	OutResponse.Form = FactoryAgentJsonUtils::GetStringField(PayloadObject, TEXT("form"));
	OutResponse.Substance = FactoryAgentJsonUtils::GetStringField(PayloadObject, TEXT("substance"));
	OutResponse.MaterialType = FactoryAgentJsonUtils::GetStringField(PayloadObject, TEXT("type"));
	OutResponse.Shape = FactoryAgentJsonUtils::GetStringField(PayloadObject, TEXT("shape"));
	OutResponse.DisplayName = GetFirstNonEmptyStringField(PayloadObject, { TEXT("display_name"), TEXT("DisplayName"), TEXT("DIsplayName") });
	OutResponse.VisualColor = GetFirstNonEmptyStringField(PayloadObject, { TEXT("visual_color"), TEXT("VisualColor") });
	OutResponse.Message = FactoryAgentJsonUtils::GetStringField(PayloadObject, TEXT("message"));
	OutResponse.FailureReason = FactoryAgentJsonUtils::GetStringField(PayloadObject, TEXT("failure_reason"));
	OutResponse.bCached = TryGetBoolField(PayloadObject, TEXT("cached"));
	OutResponse.MetadataJson = FactoryAgentJsonUtils::WriteJsonObject(FactoryAgentJsonUtils::GetObjectField(PayloadObject, TEXT("metadata")));
	OutResponse.RawPayloadJson = FactoryAgentJsonUtils::WriteJsonObject(PayloadObject);

	const TArray<TSharedPtr<FJsonValue>>* OutputValues = nullptr;
	if (PayloadObject->TryGetArrayField(TEXT("outputs"), OutputValues) && OutputValues != nullptr)
	{
		for (const TSharedPtr<FJsonValue>& OutputValue : *OutputValues)
		{
			if (!OutputValue.IsValid() || OutputValue->Type != EJson::Object)
			{
				continue;
			}

			const TSharedPtr<FJsonObject> OutputObject = OutputValue->AsObject();
			if (!OutputObject.IsValid())
			{
				continue;
			}

			FFactoryMaterialResponseOutput Output;
			Output.ItemId = FName(FactoryAgentJsonUtils::GetStringField(OutputObject, TEXT("item_id")));
			Output.Quantity = FactoryAgentJsonUtils::GetIntegerField(OutputObject, TEXT("qty"), 0);
			OutResponse.Outputs.Add(Output);
		}
	}

	return true;
}

TSharedPtr<FJsonObject> UFactoryAgentClientSubsystem::BuildMaterialGenerationRequestObject(
	const TArray<FFactoryMaterialRequestInput>& Inputs,
	const FFactoryMaterialProcessConditions& ProcessConditions,
	bool bGenerateVisualAsset,
	const FString& RequestId,
	const FString& SessionId,
	const FString& ClientId,
	int32 ContextTemperature) const
{
	TArray<FFactoryMaterialRequestInput> NormalizedInputs;
	NormalizedInputs.Reserve(Inputs.Num());
	for (const FFactoryMaterialRequestInput& Input : Inputs)
	{
		if (Input.ItemId.IsNone() || Input.Quantity <= 0)
		{
			continue;
		}

		NormalizedInputs.Add(Input);
	}

	NormalizedInputs.Sort([](const FFactoryMaterialRequestInput& Left, const FFactoryMaterialRequestInput& Right)
	{
		if (Left.ItemId != Right.ItemId)
		{
			return Left.ItemId.LexicalLess(Right.ItemId);
		}

		return Left.Quantity < Right.Quantity;
	});

	if (NormalizedInputs.Num() == 0)
	{
		LOG_LC_W(TEXT("Material generation request has no valid inputs."));
		return nullptr;
	}

	TArray<TSharedPtr<FJsonValue>> InputArray;
	InputArray.Reserve(NormalizedInputs.Num());
	for (const FFactoryMaterialRequestInput& Input : NormalizedInputs)
	{
		const TSharedPtr<FJsonObject> InputObject = MakeShared<FJsonObject>();
		InputObject->SetStringField(TEXT("item_id"), Input.ItemId.ToString());
		InputObject->SetNumberField(TEXT("qty"), Input.Quantity);
		InputArray.Add(MakeShared<FJsonValueObject>(InputObject));
	}

	const TSharedPtr<FJsonObject> ProcessConditionsObject = MakeShared<FJsonObject>();
	ProcessConditionsObject->SetStringField(
		TEXT("temperature"),
		ProcessConditions.Temperature.TrimStartAndEnd().IsEmpty() ? TEXT("default") : ProcessConditions.Temperature.TrimStartAndEnd());
	ProcessConditionsObject->SetStringField(
		TEXT("pressure"),
		ProcessConditions.Pressure.TrimStartAndEnd().IsEmpty() ? TEXT("default") : ProcessConditions.Pressure.TrimStartAndEnd());

	const FString TrimmedCatalyst = ProcessConditions.Catalyst.TrimStartAndEnd();
	if (TrimmedCatalyst.IsEmpty())
	{
		ProcessConditionsObject->SetField(TEXT("catalyst"), MakeShared<FJsonValueNull>());
	}
	else
	{
		ProcessConditionsObject->SetStringField(TEXT("catalyst"), TrimmedCatalyst);
	}

	const TSharedPtr<FJsonObject> PayloadObject = MakeShared<FJsonObject>();
	PayloadObject->SetStringField(TEXT("machine_type"), TEXT("Synthesizer"));
	PayloadObject->SetArrayField(TEXT("inputs"), InputArray);
	PayloadObject->SetObjectField(TEXT("process_conditions"), ProcessConditionsObject.ToSharedRef());
	PayloadObject->SetBoolField(TEXT("generate_visual_asset"), bGenerateVisualAsset);

	const TSharedPtr<FJsonObject> ContextObject = MakeShared<FJsonObject>();
	ContextObject->SetNumberField(TEXT("temperature"), ContextTemperature);

	const TSharedPtr<FJsonObject> RequestObject = MakeShared<FJsonObject>();
	RequestObject->SetStringField(TEXT("type"), AgentRequestType);
	RequestObject->SetStringField(
		TEXT("request_id"),
		RequestId.TrimStartAndEnd().IsEmpty()
			? FString::Printf(TEXT("material-exp-%s"), *FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower))
			: RequestId.TrimStartAndEnd());
	RequestObject->SetStringField(TEXT("session_id"), SessionId.TrimStartAndEnd().IsEmpty() ? MaterialGenerationSessionId : SessionId.TrimStartAndEnd());
	RequestObject->SetStringField(TEXT("client_id"), ClientId.TrimStartAndEnd().IsEmpty() ? MaterialGenerationClientId : ClientId.TrimStartAndEnd());
	RequestObject->SetStringField(TEXT("agent"), MaterialGenerationAgentId);
	RequestObject->SetObjectField(TEXT("payload"), PayloadObject.ToSharedRef());
	RequestObject->SetObjectField(TEXT("context"), ContextObject.ToSharedRef());
	return RequestObject;
}
