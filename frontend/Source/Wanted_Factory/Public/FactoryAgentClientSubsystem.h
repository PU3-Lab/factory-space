// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FactoryAgentClientSubsystem.generated.h"

class IWebSocket;

USTRUCT(BlueprintType)
struct FFactoryMaterialRequestInput
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory Agent|Material Generation")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory Agent|Material Generation", meta = (ClampMin = "1"))
	int32 Quantity = 1;
};

USTRUCT(BlueprintType)
struct FFactoryMaterialProcessConditions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory Agent|Material Generation")
	FString Temperature = TEXT("default");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory Agent|Material Generation")
	FString Pressure = TEXT("default");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory Agent|Material Generation")
	FString Catalyst;
};

USTRUCT(BlueprintType)
struct FFactoryMaterialResponseOutput
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Agent|Material Generation")
	FName ItemId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Agent|Material Generation")
	int32 Quantity = 0;
};

USTRUCT()
struct FFactoryPendingMaterialGenerationRequest
{
	GENERATED_BODY()

	UPROPERTY()
	FString RequestId;

	UPROPERTY()
	FString SessionId;

	UPROPERTY()
	FString ClientId;

	UPROPERTY()
	TArray<FFactoryMaterialRequestInput> Inputs;

	UPROPERTY()
	FFactoryMaterialProcessConditions ProcessConditions;

	UPROPERTY()
	bool bGenerateVisualAsset = false;

	UPROPERTY()
	int32 ContextTemperature = 0;
};

USTRUCT(BlueprintType)
struct FFactoryMaterialGenerationResponse
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Agent|Material Generation")
	FString RequestId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Agent|Material Generation")
	FString Agent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Agent|Material Generation")
	FString ResultType;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Agent|Material Generation")
	FString ExperimentHash;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Agent|Material Generation")
	FString RecipeName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Agent|Material Generation")
	FString MaterialId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Agent|Material Generation")
	FString Name;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Agent|Material Generation")
	FString State;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Agent|Material Generation")
	FString GenerationStatus;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Agent|Material Generation")
	FString VisualStatus;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Agent|Material Generation")
	FString VisualAssetKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Agent|Material Generation")
	FString TextureAssetKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Agent|Material Generation")
	FString ThumbnailAssetKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Agent|Material Generation")
	FString FallbackIcon;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Agent|Material Generation")
	FString RowName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Agent|Material Generation")
	FString Form;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Agent|Material Generation")
	FString Substance;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Agent|Material Generation")
	FString MaterialType;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Agent|Material Generation")
	FString Shape;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Agent|Material Generation")
	FString DisplayName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Agent|Material Generation")
	FString VisualColor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Agent|Material Generation")
	FString Message;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Agent|Material Generation")
	FString FailureReason;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Agent|Material Generation")
	bool bCached = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Agent|Material Generation")
	TArray<FFactoryMaterialResponseOutput> Outputs;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Agent|Material Generation")
	FString MetadataJson;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Agent|Material Generation")
	FString RawPayloadJson;
};

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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnFactoryAgentProgress, const FString&, RequestId, const FString&, Agent, const FString&, Stage, const FString&, Message, const FString&, RawMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFactoryMaterialGenerationResponse, const FFactoryMaterialGenerationResponse&, Response);

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

	UPROPERTY(BlueprintAssignable, Category = "Factory Agent|Messages")
	FOnFactoryAgentProgress OnAgentProgressReceived;

	UPROPERTY(BlueprintAssignable, Category = "Factory Agent|Material Generation")
	FOnFactoryMaterialGenerationResponse OnMaterialGenerationResponseReceived;

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

	UFUNCTION(BlueprintCallable, Category = "Factory Agent|Request")
	bool SendMaterialGenerationRequest(
		const TArray<FFactoryMaterialRequestInput>& Inputs,
		const FString& PlayerId,
		bool bGenerateVisualAsset,
		const FString& ClientId);

	UFUNCTION(BlueprintCallable, Category = "Factory Agent|Request")
	bool SendMaterialGenerationRequestAdvanced(
		const TArray<FFactoryMaterialRequestInput>& Inputs,
		const FFactoryMaterialProcessConditions& ProcessConditions,
		bool bGenerateVisualAsset,
		const FString& RequestId,
		const FString& SessionId,
		const FString& ClientId,
		int32 ContextTemperature);

	UFUNCTION(BlueprintCallable, Category = "Factory Agent|Request")
	FString BuildMaterialGenerationRequestJson(
		const TArray<FFactoryMaterialRequestInput>& Inputs,
		const FFactoryMaterialProcessConditions& ProcessConditions,
		bool bGenerateVisualAsset,
		const FString& RequestId,
		const FString& SessionId,
		const FString& ClientId,
		int32 ContextTemperature) const;

	bool ConsumePendingMaterialGenerationRequest(
		const FString& RequestId,
		FFactoryPendingMaterialGenerationRequest& OutRequest);

	UFUNCTION(BlueprintCallable, Category = "Factory Agent|Request")
	bool SendProcessOptimizerStateUpdate(int32 FactoryRevision, const FString& SessionId, const FString& ClientId);

	UFUNCTION(BlueprintCallable, Category = "Factory Agent|Request")
	FString BuildProcessOptimizerStateUpdateJson(int32 FactoryRevision, const FString& SessionId, const FString& ClientId);

	UFUNCTION(BlueprintCallable, Category = "Factory Agent|Request")
	bool SendProcessOptimizerAnalyzeRequest(
		int32 FactoryRevision,
		const FString& SessionId,
		const FString& ClientId);

	UFUNCTION(BlueprintCallable, Category = "Factory Agent|Request")
	FString BuildProcessOptimizerAnalyzeRequestJson(
		int32 FactoryRevision,
		const FString& SessionId,
		const FString& ClientId);

	UFUNCTION(BlueprintCallable, Category = "Factory Agent|Request")
	void LogProcessOptimizerStateUpdateJson(int32 FactoryRevision, const FString& SessionId, const FString& ClientId);

	UFUNCTION(BlueprintCallable, Category = "Factory Agent|Request")
	bool SaveProcessOptimizerStateUpdateJsonToDesktop(
		int32 FactoryRevision,
		const FString& SessionId,
		const FString& ClientId,
		FString& OutSavedFilePath);

	UFUNCTION(BlueprintCallable, Category = "Factory Agent|Request")
	bool SaveProcessOptimizerAnalyzeRequestJsonToDesktop(
		int32 FactoryRevision,
		const FString& SessionId,
		const FString& ClientId,
		FString& OutSavedFilePath);

	UFUNCTION(BlueprintCallable, Category = "Factory Agent|Messages")
	bool SendRawMessage(const FString& RawMessage);

private:
	TSharedPtr<IWebSocket> Socket;
	EFactoryAgentConnectionState ConnectionState = EFactoryAgentConnectionState::Disconnected;
	TMap<FString, FFactoryPendingMaterialGenerationRequest> PendingMaterialGenerationRequests;

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
	bool TryBuildMaterialGenerationResponse(
		const FString& RequestId,
		const FString& Agent,
		const TSharedPtr<FJsonObject>& PayloadObject,
		FFactoryMaterialGenerationResponse& OutResponse) const;
	TSharedPtr<FJsonObject> BuildMaterialGenerationRequestObject(
		const TArray<FFactoryMaterialRequestInput>& Inputs,
		const FFactoryMaterialProcessConditions& ProcessConditions,
		bool bGenerateVisualAsset,
		const FString& RequestId,
		const FString& SessionId,
		const FString& ClientId,
		int32 ContextTemperature) const;
	FString BuildProcessOptimizerRequestJson(
		int32 FactoryRevision,
		const FString& SessionId,
		const FString& ClientId,
		const FString& Operation,
		const FString& RequestSource,
		const FString& RequestIdPrefix);
};
