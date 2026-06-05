#include "FactoryAgentClientSubsystem.h"

#include "FactoryAgentJsonUtils.h"
#include "IWebSocket.h"
#include "Wanted_Factory.h"
#include "WebSocketsModule.h"
#include "Misc/Guid.h"
#include "Dom/JsonObject.h"

namespace
{
constexpr TCHAR AgentRequestType[] = TEXT("agent.request");
constexpr TCHAR AgentResponseType[] = TEXT("agent.response");
constexpr TCHAR AgentErrorType[] = TEXT("agent.error");
constexpr TCHAR QuestGeneratorAgentId[] = TEXT("quest_generator");
constexpr TCHAR OperatorGuideAgentId[] = TEXT("operator_guide");
constexpr TCHAR QuestSampleRequestId[] = TEXT("request-quest-sample");
constexpr TCHAR QuestSampleSessionId[] = TEXT("smoke-session");
constexpr TCHAR QuestSampleClientId[] = TEXT("smoke-client");
constexpr TCHAR OperatorGuideClientId[] = TEXT("unreal-ui-001");
constexpr TCHAR DefaultSessionId[] = TEXT("dev-session");
constexpr TCHAR DefaultClientId[] = TEXT("unreal-client");
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

	const TSharedPtr<FJsonObject> RequestObject = MakeShared<FJsonObject>();
	RequestObject->SetStringField(TEXT("type"), AgentRequestType);
	RequestObject->SetStringField(TEXT("client_id"), ClientId.IsEmpty() ? OperatorGuideClientId : ClientId);
	RequestObject->SetStringField(TEXT("agent"), OperatorGuideAgentId);
	RequestObject->SetObjectField(TEXT("payload"), PayloadObject.ToSharedRef());

	return SendRawMessage(FactoryAgentJsonUtils::WriteJsonObject(RequestObject));
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
