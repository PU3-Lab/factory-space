// Fill out your copyright notice in the Description page of Project Settings.

#include "FactoryManagerSubsystem.h"

#include "Conveyor.h"
#include "MachineBase.h"
#include "Machines/PowerLine.h"
#include "Machines/PowerGridNode.h"
#include "Machines/PowerPlant.h"
#include "Containers/Queue.h"

namespace
{
	bool IsPowerRelevantMachine(const AMachineBase* Machine)
	{
		return Machine &&
			(Machine->NeedsPower() ||
			 Machine->IsA<APowerGridNode>() ||
			 Machine->IsA<APowerPlant>() ||
			 Machine->GetMachineType() == FName(TEXT("BasicGenerator")));
	}

	void SortNames(TArray<FName>& Names)
	{
		Names.Sort([](const FName& Left, const FName& Right)
		{
			return Left.LexicalLess(Right);
		});
	}

	bool AreSectorSnapshotsEquivalent(
		const FFactorySectorSnapshot& Left,
		const FFactorySectorSnapshot& Right)
	{
		if (Left.CellIDs.Num() != Right.CellIDs.Num() ||
			Left.EdgeIDs.Num() != Right.EdgeIDs.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < Left.CellIDs.Num(); ++Index)
		{
			if (Left.CellIDs[Index] != Right.CellIDs[Index])
			{
				return false;
			}
		}

		for (int32 Index = 0; Index < Left.EdgeIDs.Num(); ++Index)
		{
			if (Left.EdgeIDs[Index] != Right.EdgeIDs[Index])
			{
				return false;
			}
		}

		return true;
	}
}

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
	SectorSnapshots.Reset();
	MachineToSector.Reset();
	DirtySectorIDs.Reset();
	RemovedSectorIDs.Reset();
	bGraphDirty = true;
	bPowerDirty = true;
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
	if (IsPowerRelevantMachine(Machine))
	{
		UpdatePowerGrid();
	}
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
	if (IsPowerRelevantMachine(Machine))
	{
		UpdatePowerGrid();
	}
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
	UpdatePowerGrid();
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
	UpdatePowerGrid();
}

bool UFactoryManagerSubsystem::AddPowerConnection(
	AMachineBase* SourceMachine,
	AMachineBase* TargetMachine,
	APowerLine* PowerLine)
{
	if (!SourceMachine || !TargetMachine || !PowerLine || !CanConnectPowerLineEndpoints(SourceMachine, TargetMachine))
	{
		return false;
	}

	const FName SourceID = MakeMachineID(SourceMachine);
	const FName TargetID = MakeMachineID(TargetMachine);
	const FName ConnectionID = MakePowerConnectionID(SourceMachine, TargetMachine, PowerLine);

	FPowerConnectionEdge& Edge = PowerConnections.FindOrAdd(ConnectionID);
	Edge.ID = ConnectionID;
	Edge.SourceMachine = SourceID;
	Edge.TargetMachine = TargetID;
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
	return CanConnectPowerLineEndpoints(SourceNode, TargetNode);
}

bool UFactoryManagerSubsystem::CanConnectPowerLineEndpoints(AMachineBase* SourceMachine, AMachineBase* TargetMachine) const
{
	if (!SourceMachine || !TargetMachine || SourceMachine == TargetMachine)
	{
		return false;
	}

	APowerGridNode* SourceNode = Cast<APowerGridNode>(SourceMachine);
	APowerGridNode* TargetNode = Cast<APowerGridNode>(TargetMachine);
	const bool bNodeToNode = SourceNode && TargetNode;
	const bool bPlantToNode = (IsPowerGeneratorMachine(SourceMachine) && TargetNode) ||
		(IsPowerGeneratorMachine(TargetMachine) && SourceNode);
	if (!bNodeToNode && !bPlantToNode)
	{
		return false;
	}

	const float MaxDistance = bNodeToNode
		? FMath::Min(SourceNode->GetConnectionRadius(), TargetNode->GetConnectionRadius())
		: (SourceNode ? SourceNode->GetConnectionRadius() : TargetNode->GetConnectionRadius());
	if (MaxDistance <= 0.0f)
	{
		return false;
	}

	if (ArePowerEndpointsConnected(SourceMachine, TargetMachine))
	{
		return false;
	}

	return FVector::DistSquared(SourceMachine->GetActorLocation(), TargetMachine->GetActorLocation()) <= FMath::Square(MaxDistance);
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
	bPowerDirty = true;
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

		AMachineBase* SourceMachine = PowerLine->GetSourceMachine();
		AMachineBase* TargetMachine = PowerLine->GetTargetMachine();
		if (!SourceMachine || !TargetMachine || !CanConnectPowerLineEndpoints(SourceMachine, TargetMachine))
		{
			continue;
		}

		AddPowerConnection(SourceMachine, TargetMachine, PowerLine);
	}

	RebuildSectorData();
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

	for (const TWeakObjectPtr<APowerLine>& WeakPowerLine : RegisteredPowerLines)
	{
		if (APowerLine* PowerLine = WeakPowerLine.Get())
		{
			PowerLine->UpdateLineVisual();
		}
	}

	bPowerDirty = false;
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

TArray<FFactorySectorSnapshot> UFactoryManagerSubsystem::GetSectorSnapshots()
{
	EnsureCachedData();

	TArray<FFactorySectorSnapshot> Result;
	SectorSnapshots.GenerateValueArray(Result);
	Result.Sort([](const FFactorySectorSnapshot& Left, const FFactorySectorSnapshot& Right)
	{
		return Left.SectorID.LexicalLess(Right.SectorID);
	});
	return Result;
}

TArray<FFactorySectorSnapshot> UFactoryManagerSubsystem::GetDirtySectorSnapshots()
{
	EnsureCachedData();

	TArray<FFactorySectorSnapshot> Result;
	for (const FName& SectorID : DirtySectorIDs)
	{
		if (const FFactorySectorSnapshot* Snapshot = SectorSnapshots.Find(SectorID))
		{
			Result.Add(*Snapshot);
		}
	}

	Result.Sort([](const FFactorySectorSnapshot& Left, const FFactorySectorSnapshot& Right)
	{
		return Left.SectorID.LexicalLess(Right.SectorID);
	});
	return Result;
}

TArray<FName> UFactoryManagerSubsystem::GetRemovedSectorIDs()
{
	EnsureCachedData();

	TArray<FName> Result;
	for (const FName& SectorID : RemovedSectorIDs)
	{
		Result.Add(SectorID);
	}
	SortNames(Result);
	return Result;
}

void UFactoryManagerSubsystem::ClearSectorDirtyState()
{
	DirtySectorIDs.Reset();
	RemovedSectorIDs.Reset();
}

FName UFactoryManagerSubsystem::GetSectorIDForMachine(const AMachineBase* Machine)
{
	EnsureCachedData();
	return Machine ? MachineToSector.FindRef(MakeMachineID(Machine)) : NAME_None;
}

TArray<FPowerConnectionEdge> UFactoryManagerSubsystem::GetPowerConnectionEdges()
{
	EnsureCachedData();

	TArray<FPowerConnectionEdge> Result;
	PowerConnections.GenerateValueArray(Result);
	return Result;
}

bool UFactoryManagerSubsystem::IsPowerLineEnergized(const APowerLine* PowerLine)
{
	EnsureCachedData();

	if (!PowerLine)
	{
		return false;
	}

	const FPowerConnectionEdge* MatchingEdge = nullptr;
	for (const TPair<FName, FPowerConnectionEdge>& ConnectionPair : PowerConnections)
	{
		if (ConnectionPair.Value.PowerLineActor.Get() == PowerLine)
		{
			MatchingEdge = &ConnectionPair.Value;
			break;
		}
	}

	if (!MatchingEdge)
	{
		return false;
	}

	AMachineBase* SourceMachine = nullptr;
	AMachineBase* TargetMachine = nullptr;
	if (const FMachineNode* SourceNode = Machines.Find(MatchingEdge->SourceMachine))
	{
		SourceMachine = SourceNode->MachineActor.Get();
	}
	if (const FMachineNode* TargetNode = Machines.Find(MatchingEdge->TargetMachine))
	{
		TargetMachine = TargetNode->MachineActor.Get();
	}

	APowerGridNode* SourceGridNode = Cast<APowerGridNode>(SourceMachine);
	APowerGridNode* TargetGridNode = Cast<APowerGridNode>(TargetMachine);
	APowerPlant* SourcePowerPlant = Cast<APowerPlant>(SourceMachine);
	APowerPlant* TargetPowerPlant = Cast<APowerPlant>(TargetMachine);

	if (SourcePowerPlant && TargetGridNode)
	{
		return SourcePowerPlant->CanGeneratePower() && TargetGridNode->IsPowerGridActive();
	}

	if (TargetPowerPlant && SourceGridNode)
	{
		return TargetPowerPlant->CanGeneratePower() && SourceGridNode->IsPowerGridActive();
	}

	if (!SourceGridNode || !TargetGridNode ||
		!SourceGridNode->IsPowerGridActive() || !TargetGridNode->IsPowerGridActive())
	{
		return false;
	}

	TSet<APowerGridNode*> VisitedNodes;
	TArray<APowerGridNode*> ComponentNodes;
	BuildPowerGridComponent(SourceGridNode, VisitedNodes, ComponentNodes);

	for (const TWeakObjectPtr<AMachineBase>& WeakMachine : RegisteredMachines)
	{
		APowerPlant* PowerPlant = Cast<APowerPlant>(WeakMachine.Get());
		if (!PowerPlant || !PowerPlant->CanGeneratePower())
		{
			continue;
		}

		if (IsMachineConnectedToComponent(PowerPlant, ComponentNodes))
		{
			return true;
		}
	}

	return false;
}

void UFactoryManagerSubsystem::EnsureCachedData()
{
	if (bGraphDirty)
	{
		RebuildCachedData();
	}
}

void UFactoryManagerSubsystem::RebuildSectorData()
{
	const TMap<FName, FFactorySectorSnapshot> PreviousSectors = SectorSnapshots;
	const TSet<FName> PreviousDirtySectorIDs = DirtySectorIDs;
	const TSet<FName> PreviousRemovedSectorIDs = RemovedSectorIDs;

	SectorSnapshots.Reset();
	MachineToSector.Reset();
	DirtySectorIDs = PreviousDirtySectorIDs;
	RemovedSectorIDs = PreviousRemovedSectorIDs;

	TSet<FName> VisitedMachines;
	for (const TPair<FName, FMachineNode>& MachinePair : Machines)
	{
		const FName RootMachineID = MachinePair.Key;
		if (RootMachineID.IsNone() || VisitedMachines.Contains(RootMachineID))
		{
			continue;
		}

		TArray<FName> ComponentCellIDs;
		TArray<FName> ComponentEdgeIDs;
		TSet<FName> ComponentEdgeSet;
		TQueue<FName> PendingMachines;
		PendingMachines.Enqueue(RootMachineID);
		VisitedMachines.Add(RootMachineID);

		while (!PendingMachines.IsEmpty())
		{
			FName CurrentMachineID = NAME_None;
			PendingMachines.Dequeue(CurrentMachineID);
			ComponentCellIDs.Add(CurrentMachineID);

			const FMachineNode* CurrentNode = Machines.Find(CurrentMachineID);
			if (!CurrentNode)
			{
				continue;
			}

			TArray<FName> NeighborConnectionIDs = CurrentNode->InputConnections;
			NeighborConnectionIDs.Append(CurrentNode->OutputConnections);
			for (const FName& ConnectionID : NeighborConnectionIDs)
			{
				const FConnectionEdge* Connection = Connections.Find(ConnectionID);
				if (!Connection)
				{
					continue;
				}

				if (!ComponentEdgeSet.Contains(ConnectionID))
				{
					ComponentEdgeSet.Add(ConnectionID);
					ComponentEdgeIDs.Add(ConnectionID);
				}

				const FName NeighborMachineID =
					Connection->SourceMachine == CurrentMachineID
						? Connection->TargetMachine
						: Connection->SourceMachine;
				if (NeighborMachineID.IsNone() || VisitedMachines.Contains(NeighborMachineID))
				{
					continue;
				}

				VisitedMachines.Add(NeighborMachineID);
				PendingMachines.Enqueue(NeighborMachineID);
			}
		}

		SortNames(ComponentCellIDs);
		SortNames(ComponentEdgeIDs);

		FFactorySectorSnapshot Snapshot;
		Snapshot.SectorID = MakeSectorID(ComponentCellIDs);
		Snapshot.CellIDs = ComponentCellIDs;
		Snapshot.EdgeIDs = ComponentEdgeIDs;

		const FFactorySectorSnapshot* PreviousSnapshot = PreviousSectors.Find(Snapshot.SectorID);
		if (PreviousSnapshot)
		{
			Snapshot.Revision = AreSectorSnapshotsEquivalent(*PreviousSnapshot, Snapshot)
				? PreviousSnapshot->Revision
				: PreviousSnapshot->Revision + 1;
		}
		else
		{
			Snapshot.Revision = 1;
		}

		if (!PreviousSnapshot || !AreSectorSnapshotsEquivalent(*PreviousSnapshot, Snapshot))
		{
			DirtySectorIDs.Add(Snapshot.SectorID);
		}

		for (const FName& CellID : Snapshot.CellIDs)
		{
			MachineToSector.Add(CellID, Snapshot.SectorID);
		}

		SectorSnapshots.Add(Snapshot.SectorID, Snapshot);
	}

	for (const TPair<FName, FFactorySectorSnapshot>& PreviousPair : PreviousSectors)
	{
		if (!SectorSnapshots.Contains(PreviousPair.Key))
		{
			RemovedSectorIDs.Add(PreviousPair.Key);
		}
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
		if (ConnectionPair.Value.SourceMachine == NodeID ||
			ConnectionPair.Value.TargetMachine == NodeID)
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

	float RemainingPower = ComponentGeneratedPower;
	for (AMachineBase* Consumer : ComponentConsumers)
	{
		const float RequiredPower = FMath::Max(0.0f, Consumer->GetPowerConsumption());
		if (RequiredPower <= 0.0f)
		{
			continue;
		}

		if (RemainingPower + 0.01f >= RequiredPower)
		{
			SetMachinePowerIfChanged(Consumer, RequiredPower);
			RemainingPower -= RequiredPower;
		}
	}
}

FName UFactoryManagerSubsystem::MakeSectorID(const TArray<FName>& CellIDs) const
{
	if (CellIDs.Num() == 0)
	{
		return NAME_None;
	}

	return FName(*FString::Printf(TEXT("sector:%s"), *CellIDs[0].ToString()));
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
	const AMachineBase* SourceMachine,
	const AMachineBase* TargetMachine,
	const APowerLine* PowerLine) const
{
	if (!SourceMachine || !TargetMachine || !PowerLine)
	{
		return NAME_None;
	}

	return FName(*FString::Printf(
		TEXT("%s<->%s:%s"),
		*SourceMachine->GetPathName(),
		*TargetMachine->GetPathName(),
		*PowerLine->GetPathName()));
}

bool UFactoryManagerSubsystem::IsPowerGeneratorMachine(const AMachineBase* Machine) const
{
	return Machine &&
		(Machine->IsA<APowerPlant>() || Machine->GetMachineType() == FName(TEXT("BasicGenerator")));
}

bool UFactoryManagerSubsystem::ArePowerEndpointsConnected(
	const AMachineBase* First,
	const AMachineBase* Second) const
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
		const bool bForward = Connection.SourceMachine == FirstID && Connection.TargetMachine == SecondID;
		const bool bBackward = Connection.SourceMachine == SecondID && Connection.TargetMachine == FirstID;
		if (bForward || bBackward)
		{
			return true;
		}
	}

	return false;
}

bool UFactoryManagerSubsystem::ArePowerGridNodesConnected(
	const APowerGridNode* First,
	const APowerGridNode* Second) const
{
	return ArePowerEndpointsConnected(First, Second);
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
		if (ArePowerEndpointsConnected(Machine, Node))
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
