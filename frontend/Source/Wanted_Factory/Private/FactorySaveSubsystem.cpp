#include "FactorySaveSubsystem.h"

#include "Conveyor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FactoryManagerSubsystem.h"
#include "FactorySaveGame.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "MachineBase.h"
#include "Machines/EscapePod.h"
#include "Machines/MachineSubsystem.h"
#include "Machines/PowerLine.h"
#include "OJJ_Foundation.h"
#include "OJJ_Grid.h"
#include "OJJ_Ladder.h"
#include "OJJ_Player.h"
#include "Pipe.h"
#include "PlanetEventManagerSubsystem.h"
#include "PlayerWarehouseSubsystem.h"
#include "QuestManagerSubsystem.h"

namespace
{
	int32 RotationToSteps(const FRotator& Rotation)
	{
		return FMath::RoundToInt(Rotation.Yaw / 90.0f);
	}

	FIntPoint FindMinCell(const TArray<FIntPoint>& Cells)
	{
		if (Cells.Num() == 0)
		{
			return FIntPoint::ZeroValue;
		}

		FIntPoint MinCell = Cells[0];
		for (const FIntPoint& Cell : Cells)
		{
			MinCell.X = FMath::Min(MinCell.X, Cell.X);
			MinCell.Y = FMath::Min(MinCell.Y, Cell.Y);
		}
		return MinCell;
	}

	FIntPoint FindSizeFromCells(const TArray<FIntPoint>& Cells)
	{
		if (Cells.Num() == 0)
		{
			return FIntPoint(1, 1);
		}

		FIntPoint MinCell = Cells[0];
		FIntPoint MaxCell = Cells[0];
		for (const FIntPoint& Cell : Cells)
		{
			MinCell.X = FMath::Min(MinCell.X, Cell.X);
			MinCell.Y = FMath::Min(MinCell.Y, Cell.Y);
			MaxCell.X = FMath::Max(MaxCell.X, Cell.X);
			MaxCell.Y = FMath::Max(MaxCell.Y, Cell.Y);
		}

		return FIntPoint((MaxCell.X - MinCell.X) + 1, (MaxCell.Y - MinCell.Y) + 1);
	}

	template <typename ActorType>
	void DestroyActorsOfType(UWorld* World)
	{
		TArray<ActorType*> Actors;
		for (TActorIterator<ActorType> It(World); It; ++It)
		{
			if (ActorType* Actor = *It)
			{
				Actors.Add(Actor);
			}
		}

		for (ActorType* Actor : Actors)
		{
			if (IsValid(Actor))
			{
				Actor->Destroy();
			}
		}
	}

	template <typename ActorType>
	TSubclassOf<ActorType> LoadActorClass(const FString& ClassPath)
	{
		if (ClassPath.IsEmpty())
		{
			return nullptr;
		}

		return FSoftClassPath(ClassPath).TryLoadClass<ActorType>();
	}

	bool IsManagedSavedMachine(const AMachineBase* Machine)
	{
		return Machine && !Machine->IsA<AEscapePod>();
	}
}

void UFactorySaveSubsystem::Deinitialize()
{
	StopAutoSaveTimer();
	CachedPlayer.Reset();
	Super::Deinitialize();
}

void UFactorySaveSubsystem::HandlePlayerReady(AOJJ_Player* Player)
{
	if (!Player)
	{
		return;
	}

	CachedPlayer = Player;
	if (!bHasLoadedInitialState)
	{
		LoadCurrentGame();
		bHasLoadedInitialState = true;
	}

	StartAutoSaveTimer();
}

bool UFactorySaveSubsystem::SaveCurrentGame()
{
	if (bIsRestoring)
	{
		return false;
	}

	AOJJ_Player* Player = CachedPlayer.Get();
	if (!Player)
	{
		return false;
	}

	UWorld* World = Player->GetWorld();
	if (!World)
	{
		return false;
	}

	AOJJ_Grid* Grid = Cast<AOJJ_Grid>(UGameplayStatics::GetActorOfClass(World, AOJJ_Grid::StaticClass()));
	UPlayerWarehouseSubsystem* Warehouse = GetGameInstance()->GetSubsystem<UPlayerWarehouseSubsystem>();
	UQuestManagerSubsystem* QuestManager = GetGameInstance()->GetSubsystem<UQuestManagerSubsystem>();
	UMachineSubsystem* MachineSubsystem = GetGameInstance()->GetSubsystem<UMachineSubsystem>();
	UPlanetEventManagerSubsystem* PlanetManager = World->GetSubsystem<UPlanetEventManagerSubsystem>();
	if (!Grid || !Warehouse || !QuestManager || !MachineSubsystem || !PlanetManager)
	{
		return false;
	}

	UFactorySaveGame* SaveGame = Cast<UFactorySaveGame>(UGameplayStatics::CreateSaveGameObject(UFactorySaveGame::StaticClass()));
	if (!SaveGame)
	{
		return false;
	}

	SaveGame->PlayerTransform = Player->GetActorTransform();
	SaveGame->WarehouseItems = Warehouse->GetStoredItems();
	SaveGame->MachineLevels = MachineSubsystem->GetMachineLevels();
	SaveGame->CurrentMainQuestIndex = QuestManager->GetCurrentMainQuestIndex();
	SaveGame->MainQuestSequence = QuestManager->GetMainQuestSequenceForSave();
	QuestManager->GetSubQuests(SaveGame->SubQuests);
	QuestManager->GetSubQuestTitles(SaveGame->SubQuestTitles);
	QuestManager->GetTutorialSaveState(
		SaveGame->bTutorialQuestTestActive,
		SaveGame->CurrentTutorialQuestId,
		SaveGame->bPendingTutorialStartDialogueReveal,
		SaveGame->LastTutorialDialogueQuestId,
		SaveGame->LastTutorialDialogueTriggerType,
		SaveGame->LastTutorialDialogueLines);
	SaveGame->TimeState = PlanetManager->GetTimeState();
	SaveGame->WeatherState = PlanetManager->GetWeatherState();
	SaveGame->EventState = PlanetManager->GetEventState();

	TArray<AMachineBase*> Machines;
	for (TActorIterator<AMachineBase> It(World); It; ++It)
	{
		if (AMachineBase* Machine = *It)
		{
			Machines.Add(Machine);
		}
	}

	Machines.Sort([](const AMachineBase& Left, const AMachineBase& Right)
	{
		return Left.GetName() < Right.GetName();
	});

	TMap<const AMachineBase*, int32> MachineIds;
	for (int32 Index = 0; Index < Machines.Num(); ++Index)
	{
		AMachineBase* Machine = Machines[Index];
		if (!IsManagedSavedMachine(Machine))
		{
			continue;
		}

		const FIntPoint Origin = Grid->GetMachineOrigin(Machine);
		if (Origin.X == INT_MIN || Origin.Y == INT_MIN)
		{
			continue;
		}

		FFactorySavedMachineData SavedMachine;
		SavedMachine.InstanceId = Index;
		SavedMachine.ClassPath = Machine->GetClass()->GetPathName();
		SavedMachine.Origin = Origin;
		SavedMachine.RotationSteps = RotationToSteps(Machine->GetActorRotation());
		SavedMachine.Transform = Machine->GetActorTransform();
		SavedMachine.bOccupancyOnly = Machine->OJJ_RequiresOccupancyOnlyRegistration();
		Machine->GetSaveState(SavedMachine.InputInventory, SavedMachine.OutputBuffer, SavedMachine.CurrentDurability);
		SaveGame->Machines.Add(SavedMachine);
		MachineIds.Add(Machine, SavedMachine.InstanceId);
	}

	for (TActorIterator<AOJJ_Foundation> It(World); It; ++It)
	{
		if (AOJJ_Foundation* Foundation = *It)
		{
			const TArray<FIntPoint>* FoundationCells = Grid->GetFoundationCells(Foundation);
			if (!FoundationCells || FoundationCells->Num() == 0)
			{
				continue;
			}

			FFactorySavedFoundationData SavedFoundation;
			SavedFoundation.ClassPath = Foundation->GetClass()->GetPathName();
			SavedFoundation.Origin = FindMinCell(*FoundationCells);
			SavedFoundation.Size = FindSizeFromCells(*FoundationCells);
			SavedFoundation.RotationSteps = RotationToSteps(Foundation->GetActorRotation());
			SavedFoundation.Transform = Foundation->GetActorTransform();
			SavedFoundation.SurfaceZ = Foundation->GetActorLocation().Z + Foundation->GetThickness();
			Foundation->GetSaveState(
				SavedFoundation.RiseSteps,
				SavedFoundation.bOneSideGroundRamp,
				SavedFoundation.LoEndLowestGroundRaw,
				SavedFoundation.bLoEndLowestValid);
			Foundation->OJJ_BuildPerCellSurfaceZ(
				SavedFoundation.Size,
				SavedFoundation.RotationSteps,
				SavedFoundation.SurfaceZ,
				SavedFoundation.RiseSteps,
				SavedFoundation.CellSurfaceZs);
			SaveGame->Foundations.Add(SavedFoundation);
		}
	}

	for (TActorIterator<AConveyor> It(World); It; ++It)
	{
		if (AConveyor* Conveyor = *It)
		{
			FFactorySavedConveyorData SavedConveyor;
			SavedConveyor.ClassPath = Conveyor->GetClass()->GetPathName();
			SavedConveyor.PathCells = Conveyor->GetPathCells();
			SavedConveyor.ItemSlots = Conveyor->GetItemSlotsForSave();
			SaveGame->Conveyors.Add(SavedConveyor);
		}
	}

	for (TActorIterator<APipe> It(World); It; ++It)
	{
		if (APipe* Pipe = *It)
		{
			FFactorySavedPipeData SavedPipe;
			SavedPipe.ClassPath = Pipe->GetClass()->GetPathName();
			SavedPipe.PathCells = Pipe->GetPathCells();
			for (const auto& Slot : Pipe->GetLiquidSlotsForSave())
			{
				FFactorySavedPipeSlot SavedSlot;
				SavedSlot.LiquidId = Slot.LiquidID;
				SavedSlot.Amount = Slot.Amount;
				SavedPipe.LiquidSlots.Add(SavedSlot);
			}
			SaveGame->Pipes.Add(SavedPipe);
		}
	}

	for (TActorIterator<APowerLine> It(World); It; ++It)
	{
		if (APowerLine* PowerLine = *It)
		{
			FFactorySavedPowerLineData SavedPowerLine;
			SavedPowerLine.ClassPath = PowerLine->GetClass()->GetPathName();
			SavedPowerLine.SourceMachineId = MachineIds.FindRef(PowerLine->GetSourceMachine());
			SavedPowerLine.TargetMachineId = MachineIds.FindRef(PowerLine->GetTargetMachine());
			if (SavedPowerLine.SourceMachineId != INDEX_NONE && SavedPowerLine.TargetMachineId != INDEX_NONE)
			{
				SaveGame->PowerLines.Add(SavedPowerLine);
			}
		}
	}

	for (TActorIterator<AOJJ_Ladder> It(World); It; ++It)
	{
		if (AOJJ_Ladder* Ladder = *It)
		{
			FFactorySavedLadderData SavedLadder;
			SavedLadder.ClassPath = Ladder->GetClass()->GetPathName();
			SavedLadder.Transform = Ladder->GetActorTransform();
			SavedLadder.ClimbHeight = Ladder->GetClimbHeight();
			SaveGame->Ladders.Add(SavedLadder);
		}
	}

	return UGameplayStatics::SaveGameToSlot(SaveGame, SaveSlotName, 0);
}

bool UFactorySaveSubsystem::LoadCurrentGame()
{
	if (bIsRestoring || !UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
	{
		return false;
	}

	AOJJ_Player* Player = CachedPlayer.Get();
	if (!Player)
	{
		return false;
	}

	UWorld* World = Player->GetWorld();
	if (!World)
	{
		return false;
	}

	AOJJ_Grid* Grid = Cast<AOJJ_Grid>(UGameplayStatics::GetActorOfClass(World, AOJJ_Grid::StaticClass()));
	UPlayerWarehouseSubsystem* Warehouse = GetGameInstance()->GetSubsystem<UPlayerWarehouseSubsystem>();
	UQuestManagerSubsystem* QuestManager = GetGameInstance()->GetSubsystem<UQuestManagerSubsystem>();
	UMachineSubsystem* MachineSubsystem = GetGameInstance()->GetSubsystem<UMachineSubsystem>();
	UPlanetEventManagerSubsystem* PlanetManager = World->GetSubsystem<UPlanetEventManagerSubsystem>();
	if (!Grid || !Warehouse || !QuestManager || !MachineSubsystem || !PlanetManager)
	{
		return false;
	}

	UFactorySaveGame* SaveGame = Cast<UFactorySaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));
	if (!SaveGame)
	{
		return false;
	}

	bIsRestoring = true;
	StopAutoSaveTimer();

	MachineSubsystem->RestoreMachineLevels(SaveGame->MachineLevels);
	Warehouse->SetStoredItemsForSave(SaveGame->WarehouseItems);
	QuestManager->SetMainQuestSequenceForSave(SaveGame->MainQuestSequence, SaveGame->CurrentMainQuestIndex);
	QuestManager->SetSubQuestsForSave(SaveGame->SubQuests, SaveGame->SubQuestTitles);
	QuestManager->RestoreTutorialSaveState(
		SaveGame->bTutorialQuestTestActive,
		SaveGame->CurrentTutorialQuestId,
		SaveGame->bPendingTutorialStartDialogueReveal,
		SaveGame->LastTutorialDialogueQuestId,
		SaveGame->LastTutorialDialogueTriggerType,
		SaveGame->LastTutorialDialogueLines);
	PlanetManager->RestoreSaveState(SaveGame->TimeState, SaveGame->WeatherState, SaveGame->EventState);

	DestroyActorsOfType<APowerLine>(World);
	DestroyActorsOfType<APipe>(World);
	DestroyActorsOfType<AConveyor>(World);
	DestroyActorsOfType<AOJJ_Ladder>(World);
	DestroyActorsOfType<AOJJ_Foundation>(World);
	{
		TArray<AMachineBase*> MachinesToDestroy;
		for (TActorIterator<AMachineBase> It(World); It; ++It)
		{
			if (AMachineBase* Machine = *It; IsManagedSavedMachine(Machine))
			{
				MachinesToDestroy.Add(Machine);
			}
		}

		for (AMachineBase* Machine : MachinesToDestroy)
		{
			if (IsValid(Machine))
			{
				Machine->Destroy();
			}
		}
	}

	TMap<int32, AMachineBase*> RestoredMachines;
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (const FFactorySavedFoundationData& SavedFoundation : SaveGame->Foundations)
	{
		if (TSubclassOf<AOJJ_Foundation> FoundationClass = LoadActorClass<AOJJ_Foundation>(SavedFoundation.ClassPath))
		{
			if (AOJJ_Foundation* Foundation = World->SpawnActor<AOJJ_Foundation>(FoundationClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams))
			{
				Foundation->SetFoundationSizeForSave(SavedFoundation.Size);
				Foundation->ApplySaveState(
					SavedFoundation.RiseSteps,
					SavedFoundation.bOneSideGroundRamp,
					SavedFoundation.LoEndLowestGroundRaw,
					SavedFoundation.bLoEndLowestValid);
				FString OutReason;
				const bool bPlaced = SavedFoundation.CellSurfaceZs.Num() > 0
					? Grid->OJJ_TryPlaceFoundationPerCell(
						Foundation,
						SavedFoundation.Origin,
						SavedFoundation.Size,
						SavedFoundation.CellSurfaceZs,
						OutReason,
						SavedFoundation.bOneSideGroundRamp)
					: Grid->TryPlaceFoundation(
						Foundation,
						SavedFoundation.Origin,
						SavedFoundation.Size,
						SavedFoundation.SurfaceZ,
						OutReason);
				if (bPlaced)
				{
					Foundation->SetActorTransform(SavedFoundation.Transform);
					Foundation->ApplySaveState(
						SavedFoundation.RiseSteps,
						SavedFoundation.bOneSideGroundRamp,
						SavedFoundation.LoEndLowestGroundRaw,
						SavedFoundation.bLoEndLowestValid);
					Foundation->OJJ_NotifyPlacedOnGrid(Grid);
				}
				else
				{
					Foundation->Destroy();
				}
			}
		}
	}

	for (const FFactorySavedMachineData& SavedMachine : SaveGame->Machines)
	{
		if (TSubclassOf<AMachineBase> MachineClass = LoadActorClass<AMachineBase>(SavedMachine.ClassPath))
		{
			if (AMachineBase* Machine = World->SpawnActor<AMachineBase>(MachineClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams))
			{
				FString OutReason;
				bool bPlaced = false;
				if (SavedMachine.bOccupancyOnly)
				{
					Machine->SetActorTransform(SavedMachine.Transform);
					bPlaced = Grid->RegisterExistingMachineOccupancyOnly(Machine, SavedMachine.Origin, OutReason);
				}
				else
				{
					bPlaced = Grid->TryPlaceMachine(Machine, SavedMachine.Origin, OutReason, SavedMachine.RotationSteps);
					if (bPlaced)
					{
						Machine->SetActorTransform(SavedMachine.Transform);
					}
				}

				if (bPlaced)
				{
					Machine->ApplySaveState(
						SavedMachine.InputInventory,
						SavedMachine.OutputBuffer,
						SavedMachine.CurrentDurability);
					RestoredMachines.Add(SavedMachine.InstanceId, Machine);
				}
				else
				{
					Machine->Destroy();
				}
			}
		}
	}

	for (const FFactorySavedConveyorData& SavedConveyor : SaveGame->Conveyors)
	{
		if (TSubclassOf<AConveyor> ConveyorClass = LoadActorClass<AConveyor>(SavedConveyor.ClassPath))
		{
			if (AConveyor* Conveyor = World->SpawnActor<AConveyor>(ConveyorClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams))
			{
				FString OutReason;
				if (Grid->OJJ_TryPlaceConveyor(Conveyor, SavedConveyor.PathCells, OutReason))
				{
					Conveyor->ApplyItemSlotsForSave(SavedConveyor.ItemSlots);
				}
				else
				{
					Conveyor->Destroy();
				}
			}
		}
	}

	for (const FFactorySavedPipeData& SavedPipe : SaveGame->Pipes)
	{
		if (TSubclassOf<APipe> PipeClass = LoadActorClass<APipe>(SavedPipe.ClassPath))
		{
			if (APipe* Pipe = World->SpawnActor<APipe>(PipeClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams))
			{
				FString OutReason;
				if (Grid->OJJ_TryPlacePipe(Pipe, SavedPipe.PathCells, OutReason))
				{
					TArray<FPipeLiquidSlot> LiquidSlots;
					for (const FFactorySavedPipeSlot& SavedSlot : SavedPipe.LiquidSlots)
					{
						FPipeLiquidSlot& Slot = LiquidSlots.AddDefaulted_GetRef();
						Slot.LiquidID = SavedSlot.LiquidId;
						Slot.Amount = SavedSlot.Amount;
					}
					Pipe->ApplyLiquidSlotsForSave(LiquidSlots);
				}
				else
				{
					Pipe->Destroy();
				}
			}
		}
	}

	for (const FFactorySavedPowerLineData& SavedPowerLine : SaveGame->PowerLines)
	{
		AMachineBase* SourceMachine = RestoredMachines.FindRef(SavedPowerLine.SourceMachineId);
		AMachineBase* TargetMachine = RestoredMachines.FindRef(SavedPowerLine.TargetMachineId);
		if (!SourceMachine || !TargetMachine)
		{
			continue;
		}

		if (TSubclassOf<APowerLine> PowerLineClass = LoadActorClass<APowerLine>(SavedPowerLine.ClassPath))
		{
			if (APowerLine* PowerLine = World->SpawnActor<APowerLine>(PowerLineClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams))
			{
				PowerLine->ConfigurePowerLine(SourceMachine, TargetMachine);
			}
		}
	}

	for (const FFactorySavedLadderData& SavedLadder : SaveGame->Ladders)
	{
		if (TSubclassOf<AOJJ_Ladder> LadderClass = LoadActorClass<AOJJ_Ladder>(SavedLadder.ClassPath))
		{
			if (AOJJ_Ladder* Ladder = World->SpawnActorDeferred<AOJJ_Ladder>(
				LadderClass,
				SavedLadder.Transform,
				nullptr,
				nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				Ladder->OJJ_SetClimbHeight(SavedLadder.ClimbHeight);
				Ladder->FinishSpawning(SavedLadder.Transform);
			}
		}
	}

	Player->SetActorTransform(SaveGame->PlayerTransform);
	if (UFactoryManagerSubsystem* FactoryManager = GetGameInstance()->GetSubsystem<UFactoryManagerSubsystem>())
	{
		FactoryManager->MarkGraphDirty();
		FactoryManager->UpdatePowerGrid();
	}

	bIsRestoring = false;
	StartAutoSaveTimer();
	return true;
}

bool UFactorySaveSubsystem::ResetToNewGame()
{
	StopAutoSaveTimer();
	bIsRestoring = false;
	bHasLoadedInitialState = false;
	return UGameplayStatics::DeleteGameInSlot(SaveSlotName, 0);
}

void UFactorySaveSubsystem::StartAutoSaveTimer()
{
	AOJJ_Player* Player = CachedPlayer.Get();
	UWorld* World = Player ? Player->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		AutoSaveTimerHandle,
		this,
		&UFactorySaveSubsystem::AutoSaveTick,
		FMath::Max(1.0f, AutoSaveIntervalSeconds),
		true);

	if (AutoSaveIntervalSeconds > AutoSaveWarningLeadSeconds)
	{
		World->GetTimerManager().SetTimer(
			AutoSaveWarningTimerHandle,
			this,
			&UFactorySaveSubsystem::AutoSaveWarningTick,
			FMath::Max(1.0f, AutoSaveIntervalSeconds),
			true,
			FMath::Max(0.0f, AutoSaveIntervalSeconds - AutoSaveWarningLeadSeconds));
	}
}

void UFactorySaveSubsystem::StopAutoSaveTimer()
{
	AOJJ_Player* Player = CachedPlayer.Get();
	UWorld* World = Player ? Player->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(AutoSaveTimerHandle);
	World->GetTimerManager().ClearTimer(AutoSaveWarningTimerHandle);
}

void UFactorySaveSubsystem::AutoSaveTick()
{
	SaveCurrentGame();
}

void UFactorySaveSubsystem::AutoSaveWarningTick()
{
	ShowAutoSaveWarning();
}

void UFactorySaveSubsystem::ShowAutoSaveWarning() const
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1771,
			FMath::Max(0.5f, AutoSaveWarningLeadSeconds),
			FColor::Yellow,
			TEXT("1분마다 자동 저장됩니다. 곧 저장합니다..."));
	}
}
