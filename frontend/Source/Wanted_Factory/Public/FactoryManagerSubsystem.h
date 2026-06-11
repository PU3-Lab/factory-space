// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FactoryManagerSubsystem.generated.h"

class AConveyor;
class AMachineBase;
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

private:
	TArray<TWeakObjectPtr<AMachineBase>> RegisteredMachines;
	TArray<TWeakObjectPtr<AConveyor>> RegisteredConveyors;
	TArray<TWeakObjectPtr<APowerGridNode>> RegisteredPowerGridNodes;
	TArray<TWeakObjectPtr<APowerLine>> RegisteredPowerLines;

	TMap<FName, FMachineNode> Machines;
	TMap<FName, FConnectionEdge> Connections;
	TMap<FName, FPowerConnectionEdge> PowerConnections;
	bool bGraphDirty = true;

	float LastTotalGeneratedPower = 0.0f;
	float LastTotalDemandPower = 0.0f;

	void EnsureCachedData();
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
};
