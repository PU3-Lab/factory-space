// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FactoryAgentClientSubsystem.generated.h"

class IWebSocket;

UENUM(BlueprintType)
enum class EFactoryAgentConnectionState : uint8
{
	Disconnected,
	Connecting,
	Connected
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFactoryAgentConnected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFactoryAgentConnectionError, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnFactoryAgentClosed, int32, StatusCode, const FString&, Reason, bool, bWasClean);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFactoryAgentRawMessage, const FString&, RawMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnFactoryAgentResponse, const FString&, RequestId, const FString&, Agent, const FString&, PayloadJson, const FString&, RawMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnFactoryAgentError, const FString&, RequestId, const FString&, Agent, const FString&, ErrorCode, const FString&, ErrorMessage, const FString&, RawMessage);

UCLASS()
class WANTED_FACTORY_API UFactoryAgentClientSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory Agent|Connection")
	FString DefaultWebSocketUrl = TEXT("ws://127.0.0.1:18000/ws/agent");

	UPROPERTY(BlueprintAssignable, Category = "Factory Agent|Connection")
	FOnFactoryAgentConnected OnConnected;

	UPROPERTY(BlueprintAssignable, Category = "Factory Agent|Connection")
	FOnFactoryAgentConnectionError OnConnectionError;

	UPROPERTY(BlueprintAssignable, Category = "Factory Agent|Connection")
	FOnFactoryAgentClosed OnClosed;

	UPROPERTY(BlueprintAssignable, Category = "Factory Agent|Messages")
	FOnFactoryAgentRawMessage OnRawMessageReceived;

	UPROPERTY(BlueprintAssignable, Category = "Factory Agent|Messages")
	FOnFactoryAgentResponse OnAgentResponseReceived;

	UPROPERTY(BlueprintAssignable, Category = "Factory Agent|Messages")
	FOnFactoryAgentError OnAgentErrorReceived;

	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Factory Agent|Connection")
	void ConnectToDefaultServer();

	UFUNCTION(BlueprintCallable, Category = "Factory Agent|Connection")
	void Connect(const FString& WebSocketUrl);

	UFUNCTION(BlueprintCallable, Category = "Factory Agent|Connection")
	void Disconnect();

	UFUNCTION(BlueprintPure, Category = "Factory Agent|Connection")
	bool IsConnected() const;

	UFUNCTION(BlueprintPure, Category = "Factory Agent|Connection")
	EFactoryAgentConnectionState GetConnectionState() const;

	UFUNCTION(BlueprintCallable, Category = "Factory Agent|Request")
	FString SendAgentRequest(const FString& Agent, const FString& PayloadJson);

	UFUNCTION(BlueprintCallable, Category = "Factory Agent|Request")
	FString SendAgentRequestWithContext(
		const FString& Agent,
		const FString& PayloadJson,
		const FString& ContextJson,
		const FString& SessionId,
		const FString& ClientId);

	UFUNCTION(BlueprintCallable, Category = "Factory Agent|Messages")
	bool SendJsonMessage(const FString& JsonMessage);

	UFUNCTION(BlueprintCallable, Category = "Factory Agent|Request")
	bool SendQuestGeneratorRequest(const FString& RequestId, const FString& SessionId, const FString& ClientId);

	UFUNCTION(BlueprintCallable, Category = "Factory Agent|Request")
	bool SendOperatorGuideQuestion(const FString& Question, const FString& ClientId);

	UFUNCTION(BlueprintCallable, Category = "Factory Agent|Messages")
	bool SendRawMessage(const FString& RawMessage);

private:
	TSharedPtr<IWebSocket> Socket;
	EFactoryAgentConnectionState ConnectionState = EFactoryAgentConnectionState::Disconnected;

	FString SendAgentRequestInternal(
		const FString& Agent,
		const FString& PayloadJson,
		const FString& ContextJson,
		const FString& SessionId,
		const FString& ClientId);

	void BindSocketEvents();
	void HandleSocketConnected();
	void HandleSocketConnectionError(const FString& Error);
	void HandleSocketClosed(int32 StatusCode, const FString& Reason, bool bWasClean);
	void HandleSocketMessage(const FString& Message);
	void ResetSocket();
};
