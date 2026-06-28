// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MachineBase.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FactoryManagerSubsystem.generated.h"

class AConveyor;
class APowerLine;
class APowerGridNode;

USTRUCT(BlueprintType)
struct FMachineNode
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager")
	FName ID = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager")
	TWeakObjectPtr<AMachineBase> MachineActor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager")
	TArray<FName> InputConnections;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager")
	TArray<FName> OutputConnections;
};

USTRUCT(BlueprintType)
struct FConnectionEdge
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager")
	FName ID = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager")
	FName SourceMachine = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager")
	FName TargetMachine = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager")
	int32 SourceOutputPortIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager")
	int32 TargetInputPortIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager")
	TWeakObjectPtr<AConveyor> ConveyorActor;
};

USTRUCT(BlueprintType)
struct FPowerConnectionEdge
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager|Power")
	FName ID = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager|Power")
	FName SourceMachine = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager|Power")
	FName TargetMachine = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager|Power")
	TWeakObjectPtr<APowerLine> PowerLineActor;
};

USTRUCT(BlueprintType)
struct FFactorySectorSnapshot
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager|Sector")
	FName SectorID = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager|Sector")
	int32 Revision = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager|Sector")
	TArray<FName> CellIDs;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager|Sector")
	TArray<FName> EdgeIDs;
};

USTRUCT(BlueprintType)
struct FFactoryPowerOverview
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager|Dashboard")
	int32 InstalledGeneratorCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager|Dashboard")
	int32 ActiveGeneratorCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager|Dashboard")
	int32 RunningConsumerCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager|Dashboard")
	float CurrentDemandPower = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager|Dashboard")
	float CurrentAvailablePower = 0.0f;
};

USTRUCT(BlueprintType)
struct FFactoryMachineProductionState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager|Dashboard")
	FName MachineID = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager|Dashboard")
	FName MachineType = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager|Dashboard")
	FName SectorID = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager|Dashboard")
	EMachineState MachineState = EMachineState::Idle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager|Dashboard")
	FName PrimaryOutputItemID = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager|Dashboard")
	float PrimaryOutputPerSecond = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager|Dashboard")
	FName SecondaryOutputItemID = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager|Dashboard")
	float SecondaryOutputPerSecond = 0.0f;
};

USTRUCT(BlueprintType)
struct FFactoryItemProductionStat
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager|Dashboard")
	FName ItemID = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager|Dashboard")
	int32 ProducingMachineCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager|Dashboard")
	float ActualProductionPerSecond = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory Manager|Dashboard")
	float TheoreticalProductionPerSecond = 0.0f;
};

struct FFactoryObservedItemSample
{
	double TimestampSeconds = 0.0;
	int32 Count = 0;
};

UCLASS()
class WANTED_FACTORY_API UFactoryManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Factory Manager")
	void RegisterMachine(AMachineBase* Machine);

	UFUNCTION(BlueprintCallable, Category = "Factory Manager")
	void UnregisterMachine(AMachineBase* Machine);

	UFUNCTION(BlueprintCallable, Category = "Factory Manager")
	void NotifyMachineChanged(AMachineBase* Machine);

	UFUNCTION(BlueprintCallable, Category = "Factory Manager")
	void RegisterConveyor(AConveyor* Conveyor);

	UFUNCTION(BlueprintCallable, Category = "Factory Manager")
	void UnregisterConveyor(AConveyor* Conveyor);

	UFUNCTION(BlueprintCallable, Category = "Factory Manager")
	void NotifyConveyorChanged(AConveyor* Conveyor);

	UFUNCTION(BlueprintCallable, Category = "Factory Manager|Power")
	void RegisterPowerGridNode(APowerGridNode* PowerGridNode);

	UFUNCTION(BlueprintCallable, Category = "Factory Manager|Power")
	void UnregisterPowerGridNode(APowerGridNode* PowerGridNode);

	UFUNCTION(BlueprintCallable, Category = "Factory Manager|Power")
	void RegisterPowerLine(APowerLine* PowerLine);

	UFUNCTION(BlueprintCallable, Category = "Factory Manager|Power")
	void UnregisterPowerLine(APowerLine* PowerLine);

	UFUNCTION(BlueprintCallable, Category = "Factory Manager|Power")
	bool AddPowerConnection(AMachineBase* SourceMachine, AMachineBase* TargetMachine, APowerLine* PowerLine);

	UFUNCTION(BlueprintCallable, Category = "Factory Manager|Power")
	void RemovePowerConnection(FName ConnectionID);

	UFUNCTION(BlueprintPure, Category = "Factory Manager|Power")
	bool CanConnectPowerGridNodes(APowerGridNode* SourceNode, APowerGridNode* TargetNode) const;

	UFUNCTION(BlueprintPure, Category = "Factory Manager|Power")
	bool CanConnectPowerLineEndpoints(AMachineBase* SourceMachine, AMachineBase* TargetMachine) const;

	UFUNCTION(BlueprintCallable, Category = "Factory Manager")
	void AddConnection(AMachineBase* SourceMachine, AMachineBase* TargetMachine, AConveyor* Conveyor);

	UFUNCTION(BlueprintCallable, Category = "Factory Manager")
	void RemoveConnection(FName ConnectionID);

	UFUNCTION(BlueprintCallable, Category = "Factory Manager")
	void MarkGraphDirty();

	UFUNCTION(BlueprintCallable, Category = "Factory Manager")
	void RebuildCachedData();

	UFUNCTION(BlueprintCallable, Category = "Factory Manager|Power")
	void UpdatePowerGrid();

	UFUNCTION(BlueprintCallable, Category = "Factory Manager")
	TArray<AMachineBase*> GetConnectedMachines(AMachineBase* Machine);

	UFUNCTION(BlueprintCallable, Category = "Factory Manager")
	TArray<FMachineNode> GetMachineNodes();

	UFUNCTION(BlueprintCallable, Category = "Factory Manager")
	TArray<FConnectionEdge> GetConnectionEdges();

	UFUNCTION(BlueprintCallable, Category = "Factory Manager|Sector")
	TArray<FFactorySectorSnapshot> GetSectorSnapshots();

	UFUNCTION(BlueprintCallable, Category = "Factory Manager|Sector")
	TArray<FFactorySectorSnapshot> GetDirtySectorSnapshots();

	UFUNCTION(BlueprintCallable, Category = "Factory Manager|Sector")
	TArray<FName> GetRemovedSectorIDs();

	UFUNCTION(BlueprintCallable, Category = "Factory Manager|Sector")
	void ClearSectorDirtyState();

	UFUNCTION(BlueprintPure, Category = "Factory Manager|Sector")
	FName GetSectorIDForMachine(const AMachineBase* Machine);

	UFUNCTION(BlueprintCallable, Category = "Factory Manager|Power")
	TArray<FPowerConnectionEdge> GetPowerConnectionEdges();

	UFUNCTION(BlueprintPure, Category = "Factory Manager|Power")
	bool IsPowerLineEnergized(const APowerLine* PowerLine);

	UFUNCTION(BlueprintPure, Category = "Factory Manager")
	bool IsGraphDirty() const { return bGraphDirty; }

	UFUNCTION(BlueprintPure, Category = "Factory Manager|Power")
	float GetLastTotalGeneratedPower() const { return LastTotalGeneratedPower; }

	UFUNCTION(BlueprintPure, Category = "Factory Manager|Power")
	float GetLastTotalDemandPower() const { return LastTotalDemandPower; }

	UFUNCTION(BlueprintCallable, Category = "Factory Manager|Dashboard")
	FFactoryPowerOverview GetFactoryPowerOverview();

	UFUNCTION(BlueprintCallable, Category = "Factory Manager|Dashboard")
	TArray<FFactoryMachineProductionState> GetMachineProductionStates();

	UFUNCTION(BlueprintCallable, Category = "Factory Manager|Dashboard")
	TArray<FFactoryItemProductionStat> GetItemProductionStats();

	UFUNCTION(BlueprintCallable, Category = "Factory Manager|Dashboard")
	void RecordObservedItemProduction(FName ItemID, int32 Count);

private:
	TArray<TWeakObjectPtr<AMachineBase>> RegisteredMachines;
	TArray<TWeakObjectPtr<AConveyor>> RegisteredConveyors;
	TArray<TWeakObjectPtr<APowerGridNode>> RegisteredPowerGridNodes;
	TArray<TWeakObjectPtr<APowerLine>> RegisteredPowerLines;

	TMap<FName, FMachineNode> Machines;
	TMap<FName, FConnectionEdge> Connections;
	TMap<FName, FPowerConnectionEdge> PowerConnections;
	TMap<FName, FFactorySectorSnapshot> SectorSnapshots;
	TMap<FName, TArray<FFactoryObservedItemSample>> ObservedItemSamples;
	TMap<FName, FName> MachineToSector;
	TSet<FName> DirtySectorIDs;
	TSet<FName> RemovedSectorIDs;
	bool bGraphDirty = true;
	bool bPowerDirty = true;
	float ObservedProductionWindowSeconds = 10.0f;

	float LastTotalGeneratedPower = 0.0f;
	float LastTotalDemandPower = 0.0f;

	void EnsureCachedData();
	void RebuildSectorData();
	void RemoveConnectionsForMachine(FName MachineID);
	void RemoveConnectionsForConveyor(AConveyor* Conveyor);
	void RemovePowerConnectionsForNode(FName NodeID);
	void RemovePowerConnectionsForLine(APowerLine* PowerLine);
	void ResetConsumerPower();
	void BuildPowerGridComponent(
		APowerGridNode* StartNode,
		TSet<APowerGridNode*>& VisitedNodes,
		TArray<APowerGridNode*>& OutComponent) const;

	void SupplyPowerToComponent(
		const TArray<APowerGridNode*>& ComponentNodes,
		TMap<AMachineBase*, float>& DesiredConsumerPower,
		TSet<AMachineBase*>& SuppliedConsumers,
		TSet<AMachineBase*>& UsedGenerators);

	FName MakeMachineID(const AMachineBase* Machine) const;
	FName MakeConnectionID(const AMachineBase* SourceMachine, const AMachineBase* TargetMachine, const AConveyor* Conveyor) const;
	FName MakePowerConnectionID(const AMachineBase* SourceMachine, const AMachineBase* TargetMachine, const APowerLine* PowerLine) const;
	bool IsPowerGeneratorMachine(const AMachineBase* Machine) const;
	bool ArePowerEndpointsConnected(const AMachineBase* First, const AMachineBase* Second) const;
	bool ArePowerGridNodesConnected(const APowerGridNode* First, const APowerGridNode* Second) const;
	bool IsMachineInNodeRadius(const AMachineBase* Machine, const APowerGridNode* Node, float Radius) const;
	bool IsMachineConnectedToComponent(const AMachineBase* Machine, const TArray<APowerGridNode*>& ComponentNodes) const;
	bool IsMachineSuppliedByComponent(const AMachineBase* Machine, const TArray<APowerGridNode*>& ComponentNodes) const;
	void SetMachinePowerIfChanged(AMachineBase* Machine, float NewPower) const;
	FName MakeSectorID(const TArray<FName>& CellIDs) const;
	bool BuildMachineProductionState(AMachineBase* Machine, FFactoryMachineProductionState& OutState);
	void PruneObservedItemSamples(double CurrentTimeSeconds);
	float CalculateObservedProductionRate(FName ItemID, double CurrentTimeSeconds);
};
