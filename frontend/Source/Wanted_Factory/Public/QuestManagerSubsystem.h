// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Quest/MainQuestTable.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "QuestManagerSubsystem.generated.h"

class UFactoryAgentClientSubsystem;
class UPlayerWarehouseSubsystem;
class UDataTable;

UENUM(BlueprintType)
enum class EQuestKind : uint8
{
	Main,
	Sub
};

UENUM(BlueprintType)
enum class EQuestStatus : uint8
{
	Inactive,
	Active,
	Completed
};

USTRUCT(BlueprintType)
struct FQuestObjective
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	EQuestObjectiveType ObjectiveType = EQuestObjectiveType::WarehouseStoreItem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FString TargetItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FName TargetId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest", meta = (ClampMin = "1"))
	int32 Quantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest", meta = (ClampMin = "0"))
	int32 CurrentCount = 0;
};

USTRUCT(BlueprintType)
struct FQuestState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FString QuestId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	EQuestKind Kind = EQuestKind::Main;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FString QuestType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	TArray<FQuestObjective> Objectives;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	TArray<FQuestRewardItem> Rewards;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	EQuestStatus Status = EQuestStatus::Inactive;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMainQuestChanged, const FQuestState&, Quest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSubQuestRequestStarted, const FString&, RequestId, const FString&, Agent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSubQuestsGenerated, const FString&, RequestId, const TArray<FQuestState>&, Quests);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSubQuestTitlesUpdated, const FString&, RequestId, const TArray<FString>&, Titles);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSubQuestsUpdated, const TArray<FQuestState>&, Quests);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSubQuestRequestFailed, const FString&, RequestId, const FString&, ErrorMessage);

UCLASS()
class WANTED_FACTORY_API UQuestManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UQuestManagerSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Main")
	TArray<FQuestState> MainQuestSequence;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Quest|Main")
	TObjectPtr<UDataTable> MainQuestTable;

	UPROPERTY(BlueprintAssignable, Category = "Quest|Main")
	FOnMainQuestChanged OnMainQuestChanged;

	UPROPERTY(BlueprintAssignable, Category = "Quest|Sub")
	FOnSubQuestRequestStarted OnSubQuestRequestStarted;

	UPROPERTY(BlueprintAssignable, Category = "Quest|Sub")
	FOnSubQuestsGenerated OnSubQuestsGenerated;

	UPROPERTY(BlueprintAssignable, Category = "Quest|Sub")
	FOnSubQuestTitlesUpdated OnSubQuestTitlesUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Quest|Sub")
	FOnSubQuestsUpdated OnSubQuestsUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Quest|Sub")
	FOnSubQuestRequestFailed OnSubQuestRequestFailed;

	UFUNCTION(BlueprintPure, Category = "Quest|Main")
	bool GetCurrentMainQuest(FQuestState& OutQuest) const;

	UFUNCTION(BlueprintPure, Category = "Quest|Main")
	int32 GetCurrentMainQuestIndex() const { return CurrentMainQuestIndex; }

	UFUNCTION(BlueprintCallable, Category = "Quest|Main")
	bool SetCurrentMainQuestIndex(int32 NewIndex);

	UFUNCTION(BlueprintCallable, Category = "Quest|Main")
	bool AdvanceMainQuest();

	UFUNCTION(BlueprintCallable, Category = "Quest|Main")
	void ResetMainQuestProgress();

	UFUNCTION(BlueprintCallable, Category = "Quest|Main")
	void NotifyMainQuestInputAction(FName ActionId);

	UFUNCTION(BlueprintCallable, Category = "Quest|Main")
	void NotifyMainQuestBuildModeEntered();

	UFUNCTION(BlueprintCallable, Category = "Quest|Main")
	void NotifyMainQuestMachinePlaced(FName MachineType);

	UFUNCTION(BlueprintPure, Category = "Quest|Sub")
	void GetSubQuests(TArray<FQuestState>& OutQuests) const;

	UFUNCTION(BlueprintPure, Category = "Quest|Sub")
	void GetSubQuestTitles(TArray<FString>& OutTitles) const;

	UFUNCTION(BlueprintCallable, Category = "Quest|Sub")
	void ClearSubQuests();

	UFUNCTION(BlueprintCallable, Category = "Quest|Sub")
	void ConnectQuestAgent();

	UFUNCTION(BlueprintCallable, Category = "Quest|Sub")
	FString RequestSubQuests();

	UFUNCTION(BlueprintCallable, Category = "Quest|Sub")
	FString RequestProductionSubQuests(const FString& Question);

private:
	int32 CurrentMainQuestIndex = 0;

	UPROPERTY()
	TArray<FQuestState> SubQuests;

	UPROPERTY()
	TArray<FString> SubQuestTitles;

	UPROPERTY()
	TObjectPtr<UFactoryAgentClientSubsystem> AgentClient;

	UPROPERTY()
	TObjectPtr<UPlayerWarehouseSubsystem> WarehouseSubsystem;

	TSet<FString> PendingSubQuestRequestIds;

	void ActivateCurrentMainQuest();
	void BindAgentClient();
	void BindWarehouse();
	void LoadMainQuestSequence();
	void AppendMainQuestObjective(TArray<FQuestObjective>& Objectives, EQuestObjectiveType ObjectiveType, FName TargetId, int32 RequiredCount) const;
	void AppendMainQuestReward(TArray<FQuestRewardItem>& Rewards, FName ItemId, int32 Quantity) const;
	void RefreshMainQuestCompletion();
	void GrantQuestRewards(const FQuestState& Quest);
	bool IsMainQuestCompleted(const FQuestState& Quest) const;
	void ApplyMainQuestObjectiveEvent(EQuestObjectiveType ObjectiveType, FName TargetId, int32 DeltaCount);
	FString SendSubQuestRequest(const FString& PayloadJson);
	void RefreshSubQuestCompletion();
	bool IsQuestCompletedByWarehouse(const FQuestState& Quest) const;

	UFUNCTION()
	void HandleWarehouseItemAdded(FName ItemID, int32 AddedCount, int32 NewTotalCount);

	UFUNCTION()
	void HandleAgentResponse(
		const FString& RequestId,
		const FString& Agent,
		const FString& PayloadJson,
		const FString& RawMessage);

	UFUNCTION()
	void HandleAgentError(
		const FString& RequestId,
		const FString& Agent,
		const FString& ErrorCode,
		const FString& ErrorMessage,
		const FString& RawMessage);
};
