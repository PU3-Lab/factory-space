// Fill out your copyright notice in the Description page of Project Settings.

#include "FactoryManagerSubsystem.h"

#include "Conveyor.h"
#include "MachineBase.h"
#include "Machines/PowerLine.h"
#include "Machines/PowerGridNode.h"
#include "Machines/PowerPlant.h"

void UFactoryManagerSubsystem::Deinitialize()
{
	ResetConsumerPower();
	RegisteredMachines.Reset();
	RegisteredConveyors.Reset();
	RegisteredPowerGridNodes.Reset();
	RegisteredPowerLines.Reset();
	Machines.Reset();
	Connections.Reset();
	PowerConnections.Reset();
	bGraphDirty = true;
	Super::Deinitialize();
}

void UFactoryManagerSubsystem::RegisterMachine(AMachineBase* Machine)
{
	if (!Machine)
	{
		return;
	}

	RegisteredMachines.RemoveAllSwap(
		[Machine](const TWeakObjectPtr<AMachineBase>& Registered)
		{
			return !Registered.IsValid() || Registered.Get() == Machine;
		});
	RegisteredMachines.Add(Machine);
	MarkGraphDirty();
}

void UFactoryManagerSubsystem::UnregisterMachine(AMachineBase* Machine)
{
	if (!Machine)
	{
		return;
	}

	SetMachinePowerIfChanged(Machine, 0.0f);
	RegisteredMachines.RemoveAllSwap(
		[Machine](const TWeakObjectPtr<AMachineBase>& Registered)
		{
			return !Registered.IsValid() || Registered.Get() == Machine;
		});
	RemoveConnectionsForMachine(MakeMachineID(Machine));
	MarkGraphDirty();
}

void UFactoryManagerSubsystem::NotifyMachineChanged(AMachineBase* Machine)
{
	RegisterMachine(Machine);
}

void UFactoryManagerSubsystem::RegisterConveyor(AConveyor* Conveyor)
{
	if (!Conveyor)
	{
		return;
	}

	RegisteredConveyors.RemoveAllSwap(
		[Conveyor](const TWeakObjectPtr<AConveyor>& Registered)
		{
			return !Registered.IsValid() || Registered.Get() == Conveyor;
		});
	RegisteredConveyors.Add(Conveyor);
	MarkGraphDirty();
}

void UFactoryManagerSubsystem::UnregisterConveyor(AConveyor* Conveyor)
{
	RegisteredConveyors.RemoveAllSwap(
		[Conveyor](const TWeakObjectPtr<AConveyor>& Registered)
		{
			return !Registered.IsValid() || Registered.Get() == Conveyor;
		});
	RemoveConnectionsForConveyor(Conveyor);
	MarkGraphDirty();
}

void UFactoryManagerSubsystem::NotifyConveyorChanged(AConveyor* Conveyor)
{
	RegisterConveyor(Conveyor);
}

void UFactoryManagerSubsystem::RegisterPowerGridNode(APowerGridNode* PowerGridNode)
{
	if (!PowerGridNode)
	{
		return;
	}

	RegisteredPowerGridNodes.RemoveAllSwap(
		[PowerGridNode](const TWeakObjectPtr<APowerGridNode>& Registered)
		{
			return !Registered.IsValid() || Registered.Get() == PowerGridNode;
		});
	RegisteredPowerGridNodes.Add(PowerGridNode);
	RegisterMachine(PowerGridNode);
}

void UFactoryManagerSubsystem::UnregisterPowerGridNode(APowerGridNode* PowerGridNode)
{
	RegisteredPowerGridNodes.RemoveAllSwap(
		[PowerGridNode](const TWeakObjectPtr<APowerGridNode>& Registered)
		{
			return !Registered.IsValid() || Registered.Get() == PowerGridNode;
		});
	RemovePowerConnectionsForNode(MakeMachineID(PowerGridNode));
	MarkGraphDirty();
}

void UFactoryManagerSubsystem::RegisterPowerLine(APowerLine* PowerLine)
{
	if (!PowerLine)
	{
		return;
	}

	RegisteredPowerLines.RemoveAllSwap(
		[PowerLine](const TWeakObjectPtr<APowerLine>& Registered)
		{
			return !Registered.IsValid() || Registered.Get() == PowerLine;
		});
	RegisteredPowerLines.Add(PowerLine);
	MarkGraphDirty();
}

void UFactoryManagerSubsystem::UnregisterPowerLine(APowerLine* PowerLine)
{
	RegisteredPowerLines.RemoveAllSwap(
		[PowerLine](const TWeakObjectPtr<APowerLine>& Registered)
		{
			return !Registered.IsValid() || Registered.Get() == PowerLine;
		});
	RemovePowerConnectionsForLine(PowerLine);
	MarkGraphDirty();
}

bool UFactoryManagerSubsystem::AddPowerConnection(
	APowerGridNode* SourceNode,
	APowerGridNode* TargetNode,
	APowerLine* PowerLine)
{
	if (!SourceNode || !TargetNode || !PowerLine || !CanConnectPowerGridNodes(SourceNode, TargetNode))
	{
		return false;
	}

	const FName SourceID = MakeMachineID(SourceNode);
	const FName TargetID = MakeMachineID(TargetNode);
	const FName ConnectionID = MakePowerConnectionID(SourceNode, TargetNode, PowerLine);

	FPowerConnectionEdge& Edge = PowerConnections.FindOrAdd(ConnectionID);
	Edge.ID = ConnectionID;
	Edge.SourceNode = SourceID;
	Edge.TargetNode = TargetID;
	Edge.PowerLineActor = PowerLine;

	MarkGraphDirty();
	return true;
}

void UFactoryManagerSubsystem::RemovePowerConnection(FName ConnectionID)
{
	if (ConnectionID.IsNone())
	{
		return;
	}

	PowerConnections.Remove(ConnectionID);
	MarkGraphDirty();
}

bool UFactoryManagerSubsystem::CanConnectPowerGridNodes(APowerGridNode* SourceNode, APowerGridNode* TargetNode) const
{
	if (!SourceNode || !TargetNode || SourceNode == TargetNode)
	{
		return false;
	}

	const float MaxDistance = FMath::Min(SourceNode->GetConnectionRadius(), TargetNode->GetConnectionRadius());
	if (MaxDistance <= 0.0f)
	{
		return false;
	}

	if (ArePowerGridNodesConnected(SourceNode, TargetNode))
	{
		return false;
	}

	return FVector::DistSquared(SourceNode->GetActorLocation(), TargetNode->GetActorLocation()) <= FMath::Square(MaxDistance);
}

void UFactoryManagerSubsystem::AddConnection(
	AMachineBase* SourceMachine,
	AMachineBase* TargetMachine,
	AConveyor* Conveyor)
{
	if (!SourceMachine || !TargetMachine || !Conveyor)
	{
		return;
	}

	const FName SourceID = MakeMachineID(SourceMachine);
	const FName TargetID = MakeMachineID(TargetMachine);
	const FName ConnectionID = MakeConnectionID(SourceMachine, TargetMachine, Conveyor);

	FConnectionEdge& Edge = Connections.FindOrAdd(ConnectionID);
	Edge.ID = ConnectionID;
	Edge.SourceMachine = SourceID;
	Edge.TargetMachine = TargetID;
	Edge.SourceOutputPortIndex = 0;
	Edge.TargetInputPortIndex = 0;
	Edge.ConveyorActor = Conveyor;

	FMachineNode& SourceNode = Machines.FindOrAdd(SourceID);
	SourceNode.ID = SourceID;
	SourceNode.MachineActor = SourceMachine;
	SourceNode.OutputConnections.AddUnique(ConnectionID);

	FMachineNode& TargetNode = Machines.FindOrAdd(TargetID);
	TargetNode.ID = TargetID;
	TargetNode.MachineActor = TargetMachine;
	TargetNode.InputConnections.AddUnique(ConnectionID);

	MarkGraphDirty();
}

void UFactoryManagerSubsystem::RemoveConnection(FName ConnectionID)
{
	if (ConnectionID.IsNone())
	{
		return;
	}

	Connections.Remove(ConnectionID);
	for (TPair<FName, FMachineNode>& MachinePair : Machines)
	{
		MachinePair.Value.InputConnections.Remove(ConnectionID);
		MachinePair.Value.OutputConnections.Remove(ConnectionID);
	}
	MarkGraphDirty();
}

void UFactoryManagerSubsystem::MarkGraphDirty()
{
	bGraphDirty = true;
}

void UFactoryManagerSubsystem::RebuildCachedData()
{
	Machines.Reset();
	Connections.Reset();
	PowerConnections.Reset();

	for (const TWeakObjectPtr<AMachineBase>& WeakMachine : RegisteredMachines)
	{
		AMachineBase* Machine = WeakMachine.Get();
		if (!Machine)
		{
			continue;
		}

		const FName MachineID = MakeMachineID(Machine);
		FMachineNode& Node = Machines.FindOrAdd(MachineID);
		Node.ID = MachineID;
		Node.MachineActor = Machine;
	}

	for (const TWeakObjectPtr<AConveyor>& WeakConveyor : RegisteredConveyors)
	{
		AConveyor* Conveyor = WeakConveyor.Get();
		if (!Conveyor)
		{
			continue;
		}

		AMachineBase* SourceMachine = Conveyor->GetSourceMachine();
		AMachineBase* TargetMachine = Conveyor->GetTargetMachine();
		if (!SourceMachine || !TargetMachine)
		{
			continue;
		}

		AddConnection(SourceMachine, TargetMachine, Conveyor);
	}

	for (const TWeakObjectPtr<APowerLine>& WeakPowerLine : RegisteredPowerLines)
	{
		APowerLine* PowerLine = WeakPowerLine.Get();
		if (!PowerLine)
		{
			continue;
		}

		APowerGridNode* SourceNode = PowerLine->GetSourceNode();
		APowerGridNode* TargetNode = PowerLine->GetTargetNode();
		if (!SourceNode || !TargetNode || !CanConnectPowerGridNodes(SourceNode, TargetNode))
		{
			continue;
		}

		AddPowerConnection(SourceNode, TargetNode, PowerLine);
	}

	bGraphDirty = false;
}

void UFactoryManagerSubsystem::UpdatePowerGrid()
{
	EnsureCachedData();
	ResetConsumerPower();

	LastTotalGeneratedPower = 0.0f;
	LastTotalDemandPower = 0.0f;

	TSet<APowerGridNode*> VisitedNodes;
	TSet<AMachineBase*> SuppliedConsumers;
	TSet<AMachineBase*> UsedGenerators;

	for (const TWeakObjectPtr<APowerGridNode>& WeakNode : RegisteredPowerGridNodes)
	{
		APowerGridNode* Node = WeakNode.Get();
		if (!Node || !Node->IsPowerGridActive() || VisitedNodes.Contains(Node))
		{
			continue;
		}

		TArray<APowerGridNode*> ComponentNodes;
		BuildPowerGridComponent(Node, VisitedNodes, ComponentNodes);
		SupplyPowerToComponent(ComponentNodes, SuppliedConsumers, UsedGenerators);
	}
}

TArray<AMachineBase*> UFactoryManagerSubsystem::GetConnectedMachines(AMachineBase* Machine)
{
	TArray<AMachineBase*> Result;
	if (!Machine)
	{
		return Result;
	}

	EnsureCachedData();

	const FMachineNode* Node = Machines.Find(MakeMachineID(Machine));
	if (!Node)
	{
		return Result;
	}

	for (const FName& ConnectionID : Node->OutputConnections)
	{
		const FConnectionEdge* Edge = Connections.Find(ConnectionID);
		const FMachineNode* TargetNode = Edge ? Machines.Find(Edge->TargetMachine) : nullptr;
		if (TargetNode && TargetNode->MachineActor.IsValid())
		{
			Result.Add(TargetNode->MachineActor.Get());
		}
	}

	return Result;
}

TArray<FMachineNode> UFactoryManagerSubsystem::GetMachineNodes()
{
	EnsureCachedData();

	TArray<FMachineNode> Result;
	Machines.GenerateValueArray(Result);
	return Result;
}

TArray<FConnectionEdge> UFactoryManagerSubsystem::GetConnectionEdges()
{
	EnsureCachedData();

	TArray<FConnectionEdge> Result;
	Connections.GenerateValueArray(Result);
	return Result;
}

TArray<FPowerConnectionEdge> UFactoryManagerSubsystem::GetPowerConnectionEdges()
{
	EnsureCachedData();

	TArray<FPowerConnectionEdge> Result;
	PowerConnections.GenerateValueArray(Result);
	return Result;
}

void UFactoryManagerSubsystem::EnsureCachedData()
{
	if (bGraphDirty)
	{
		RebuildCachedData();
	}
}

void UFactoryManagerSubsystem::RemoveConnectionsForMachine(FName MachineID)
{
	if (MachineID.IsNone())
	{
		return;
	}

	TArray<FName> ConnectionsToRemove;
	for (const TPair<FName, FConnectionEdge>& ConnectionPair : Connections)
	{
		if (ConnectionPair.Value.SourceMachine == MachineID ||
			ConnectionPair.Value.TargetMachine == MachineID)
		{
			ConnectionsToRemove.Add(ConnectionPair.Key);
		}
	}

	for (const FName& ConnectionID : ConnectionsToRemove)
	{
		RemoveConnection(ConnectionID);
	}
}

void UFactoryManagerSubsystem::RemoveConnectionsForConveyor(AConveyor* Conveyor)
{
	if (!Conveyor)
	{
		return;
	}

	TArray<FName> ConnectionsToRemove;
	for (const TPair<FName, FConnectionEdge>& ConnectionPair : Connections)
	{
		if (ConnectionPair.Value.ConveyorActor.Get() == Conveyor)
		{
			ConnectionsToRemove.Add(ConnectionPair.Key);
		}
	}

	for (const FName& ConnectionID : ConnectionsToRemove)
	{
		RemoveConnection(ConnectionID);
	}
}

void UFactoryManagerSubsystem::RemovePowerConnectionsForNode(FName NodeID)
{
	if (NodeID.IsNone())
	{
		return;
	}

	TArray<FName> ConnectionsToRemove;
	for (const TPair<FName, FPowerConnectionEdge>& ConnectionPair : PowerConnections)
	{
		if (ConnectionPair.Value.SourceNode == NodeID ||
			ConnectionPair.Value.TargetNode == NodeID)
		{
			ConnectionsToRemove.Add(ConnectionPair.Key);
		}
	}

	for (const FName& ConnectionID : ConnectionsToRemove)
	{
		RemovePowerConnection(ConnectionID);
	}
}

void UFactoryManagerSubsystem::RemovePowerConnectionsForLine(APowerLine* PowerLine)
{
	if (!PowerLine)
	{
		return;
	}

	TArray<FName> ConnectionsToRemove;
	for (const TPair<FName, FPowerConnectionEdge>& ConnectionPair : PowerConnections)
	{
		if (ConnectionPair.Value.PowerLineActor.Get() == PowerLine)
		{
			ConnectionsToRemove.Add(ConnectionPair.Key);
		}
	}

	for (const FName& ConnectionID : ConnectionsToRemove)
	{
		RemovePowerConnection(ConnectionID);
	}
}

void UFactoryManagerSubsystem::ResetConsumerPower()
{
	for (const TWeakObjectPtr<AMachineBase>& WeakMachine : RegisteredMachines)
	{
		AMachineBase* Machine = WeakMachine.Get();
		if (!Machine || !Machine->NeedsPower())
		{
			continue;
		}

		SetMachinePowerIfChanged(Machine, 0.0f);
	}
}

void UFactoryManagerSubsystem::BuildPowerGridComponent(
	APowerGridNode* StartNode,
	TSet<APowerGridNode*>& VisitedNodes,
	TArray<APowerGridNode*>& OutComponent) const
{
	TArray<APowerGridNode*> PendingNodes;
	PendingNodes.Add(StartNode);
	VisitedNodes.Add(StartNode);

	while (PendingNodes.Num() > 0)
	{
		APowerGridNode* CurrentNode = PendingNodes.Pop(EAllowShrinking::No);
		OutComponent.Add(CurrentNode);

		for (const TWeakObjectPtr<APowerGridNode>& WeakCandidate : RegisteredPowerGridNodes)
		{
			APowerGridNode* CandidateNode = WeakCandidate.Get();
			if (!CandidateNode ||
				!CandidateNode->IsPowerGridActive() ||
				VisitedNodes.Contains(CandidateNode))
			{
				continue;
			}

			if (ArePowerGridNodesConnected(CurrentNode, CandidateNode))
			{
				VisitedNodes.Add(CandidateNode);
				PendingNodes.Add(CandidateNode);
			}
		}
	}
}

void UFactoryManagerSubsystem::SupplyPowerToComponent(
	const TArray<APowerGridNode*>& ComponentNodes,
	TSet<AMachineBase*>& SuppliedConsumers,
	TSet<AMachineBase*>& UsedGenerators)
{
	float ComponentGeneratedPower = 0.0f;
	TArray<AMachineBase*> ComponentConsumers;

	for (const TWeakObjectPtr<AMachineBase>& WeakMachine : RegisteredMachines)
	{
		AMachineBase* Machine = WeakMachine.Get();
		if (!Machine)
		{
			continue;
		}

		if (APowerPlant* PowerPlant = Cast<APowerPlant>(Machine))
		{
			if (!UsedGenerators.Contains(Machine) &&
				PowerPlant->CanGeneratePower() &&
				IsMachineConnectedToComponent(PowerPlant, ComponentNodes))
			{
				ComponentGeneratedPower += PowerPlant->GetCurrentPowerOutput();
				UsedGenerators.Add(Machine);
			}
			continue;
		}

		if (Cast<APowerGridNode>(Machine))
		{
			continue;
		}

		if (!Machine->NeedsPower() ||
			Machine->isBroken() ||
			SuppliedConsumers.Contains(Machine) ||
			!IsMachineSuppliedByComponent(Machine, ComponentNodes))
		{
			continue;
		}

		ComponentConsumers.Add(Machine);
		SuppliedConsumers.Add(Machine);
	}

	float ComponentDemandPower = 0.0f;
	for (AMachineBase* Consumer : ComponentConsumers)
	{
		ComponentDemandPower += FMath::Max(0.0f, Consumer->GetPowerConsumption());
	}

	LastTotalGeneratedPower += ComponentGeneratedPower;
	LastTotalDemandPower += ComponentDemandPower;

	if (ComponentDemandPower <= 0.0f || ComponentGeneratedPower <= 0.0f)
	{
		return;
	}

	const float SupplyRatio = FMath::Clamp(ComponentGeneratedPower / ComponentDemandPower, 0.0f, 1.0f);
	for (AMachineBase* Consumer : ComponentConsumers)
	{
		SetMachinePowerIfChanged(Consumer, Consumer->GetPowerConsumption() * SupplyRatio);
	}
}

FName UFactoryManagerSubsystem::MakeMachineID(const AMachineBase* Machine) const
{
	return Machine ? FName(*Machine->GetPathName()) : NAME_None;
}

FName UFactoryManagerSubsystem::MakeConnectionID(
	const AMachineBase* SourceMachine,
	const AMachineBase* TargetMachine,
	const AConveyor* Conveyor) const
{
	if (!SourceMachine || !TargetMachine || !Conveyor)
	{
		return NAME_None;
	}

	return FName(*FString::Printf(
		TEXT("%s->%s:%s"),
		*SourceMachine->GetPathName(),
		*TargetMachine->GetPathName(),
		*Conveyor->GetPathName()));
}

FName UFactoryManagerSubsystem::MakePowerConnectionID(
	const APowerGridNode* SourceNode,
	const APowerGridNode* TargetNode,
	const APowerLine* PowerLine) const
{
	if (!SourceNode || !TargetNode || !PowerLine)
	{
		return NAME_None;
	}

	return FName(*FString::Printf(
		TEXT("%s<->%s:%s"),
		*SourceNode->GetPathName(),
		*TargetNode->GetPathName(),
		*PowerLine->GetPathName()));
}

bool UFactoryManagerSubsystem::ArePowerGridNodesConnected(
	const APowerGridNode* First,
	const APowerGridNode* Second) const
{
	if (!First || !Second)
	{
		return false;
	}

	const FName FirstID = MakeMachineID(First);
	const FName SecondID = MakeMachineID(Second);
	for (const TPair<FName, FPowerConnectionEdge>& ConnectionPair : PowerConnections)
	{
		const FPowerConnectionEdge& Connection = ConnectionPair.Value;
		const bool bForward = Connection.SourceNode == FirstID && Connection.TargetNode == SecondID;
		const bool bBackward = Connection.SourceNode == SecondID && Connection.TargetNode == FirstID;
		if (bForward || bBackward)
		{
			return true;
		}
	}

	return false;
}

bool UFactoryManagerSubsystem::IsMachineInNodeRadius(
	const AMachineBase* Machine,
	const APowerGridNode* Node,
	float Radius) const
{
	if (!Machine || !Node || Radius <= 0.0f)
	{
		return false;
	}

	return FVector::DistSquared(Machine->GetActorLocation(), Node->GetActorLocation()) <= FMath::Square(Radius);
}

bool UFactoryManagerSubsystem::IsMachineConnectedToComponent(
	const AMachineBase* Machine,
	const TArray<APowerGridNode*>& ComponentNodes) const
{
	for (const APowerGridNode* Node : ComponentNodes)
	{
		if (IsMachineInNodeRadius(Machine, Node, Node->GetConnectionRadius()))
		{
			return true;
		}
	}

	return false;
}

bool UFactoryManagerSubsystem::IsMachineSuppliedByComponent(
	const AMachineBase* Machine,
	const TArray<APowerGridNode*>& ComponentNodes) const
{
	for (const APowerGridNode* Node : ComponentNodes)
	{
		if (IsMachineInNodeRadius(Machine, Node, Node->GetSupplyRadius()))
		{
			return true;
		}
	}

	return false;
}

void UFactoryManagerSubsystem::SetMachinePowerIfChanged(AMachineBase* Machine, float NewPower) const
{
	if (!Machine)
	{
		return;
	}

	if (FMath::IsNearlyEqual(Machine->GetCurrentProvidedPower(), NewPower, 0.01f))
	{
		return;
	}

	Machine->SetProvidedPower(NewPower);
}
