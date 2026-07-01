#include "FactorySaveSubsystem.h"

#include "Conveyor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FactoryManagerSubsystem.h"
#include "FactoryAgentClientSubsystem.h"
#include "FactorySaveGame.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "MachineBase.h"
#include "Machines/EscapePod.h"
#include "Machines/LiquidTank.h"
#include "Machines/MachineSubsystem.h"
#include "Machines/MoldingMachine.h"
#include "Machines/PowerLine.h"
#include "Machines/WarehousePort.h"
#include "Misc/CoreDelegates.h"
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

	FIntPoint ResolveMachineOriginForSave(const AOJJ_Grid* Grid, const AMachineBase* Machine, int32 RotationSteps)
	{
		if (!Grid || !Machine)
		{
			return FIntPoint(INT_MIN, INT_MIN);
		}

		const FIntPoint RegisteredOrigin = Grid->GetMachineOrigin(const_cast<AMachineBase*>(Machine));
		if (RegisteredOrigin.X != INT_MIN && RegisteredOrigin.Y != INT_MIN)
		{
			return RegisteredOrigin;
		}

		const FIntPoint CursorCell = Grid->WorldToGrid(Machine->GetActorLocation());
		const FIntPoint EffectiveFootprint = AOJJ_Grid::EffectiveSize(Machine->GetMachineSize(), RotationSteps);
		return AOJJ_Grid::OJJ_OriginFromCursorCellForSize(CursorCell, EffectiveFootprint);
	}

	FIntPoint ResolveFoundationOriginForSave(const AOJJ_Grid* Grid, const AOJJ_Foundation* Foundation)
	{
		if (!Grid || !Foundation)
		{
			return FIntPoint::ZeroValue;
		}

		if (const TArray<FIntPoint>* FoundationCells = Grid->GetFoundationCells(const_cast<AOJJ_Foundation*>(Foundation)))
		{
			if (FoundationCells->Num() > 0)
			{
				return FindMinCell(*FoundationCells);
			}
		}

		const FIntPoint CursorCell = Grid->WorldToGrid(Foundation->GetActorLocation());
		return AOJJ_Grid::OJJ_OriginFromCursorCellForSize(CursorCell, Foundation->GetFoundationSize());
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

	FName ResolveSavedLiquidTankOutput(const FName SelectedOutputItemId, const TMap<FName, int32>& OutputBuffer)
	{
		if (!SelectedOutputItemId.IsNone())
		{
			return SelectedOutputItemId;
		}

		for (const TPair<FName, int32>& OutputEntry : OutputBuffer)
		{
			if (!OutputEntry.Key.IsNone() && OutputEntry.Value > 0)
			{
				return OutputEntry.Key;
			}
		}

		return NAME_None;
	}
}

void UFactorySaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency(UFactoryAgentClientSubsystem::StaticClass());
	FCoreDelegates::OnPreExit.AddUObject(this, &UFactorySaveSubsystem::HandlePreExitSave);
	FWorldDelegates::OnWorldBeginTearDown.AddUObject(this, &UFactorySaveSubsystem::HandleWorldBeginTearDown);
}

void UFactorySaveSubsystem::Deinitialize()
{
	HandlePreExitSave();
	FCoreDelegates::OnPreExit.RemoveAll(this);
	FWorldDelegates::OnWorldBeginTearDown.RemoveAll(this);
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
	bIsResettingToNewGame = false;
	bHasHandledShutdownSave = false;
	if (!bHasLoadedInitialState)
	{
		LoadCurrentGame();
		bHasLoadedInitialState = true;
	}

	EnsureAgentConnection();
	StartAutoSaveTimer();
}

bool UFactorySaveSubsystem::SaveCurrentGame()
{
	if (bIsRestoring)
	{
		return false;
	}

	if (bIsResettingToNewGame)
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
		SaveGame->LastTutorialDialogueLines,
		SaveGame->TutorialProgressCounts);
	SaveGame->TimeState = PlanetManager->GetTimeState();
	SaveGame->WeatherState = PlanetManager->GetWeatherState();
	SaveGame->EventState = PlanetManager->GetEventState();

	// [게임진입] 선택 캐릭터 저장(OJJ 합의). Continue 시 LoadCurrentGame이 SetSelectedCharacter로 복원한다.
	if (UOJJ_CharacterSelectionSubsystem* CharacterSelection = GetGameInstance()->GetSubsystem<UOJJ_CharacterSelectionSubsystem>())
	{
		SaveGame->SavedCharacterType = CharacterSelection->GetSelectedCharacter();
	}

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
		const int32 RotationSteps = RotationToSteps(Machine->GetActorRotation());
		const FIntPoint Origin = ResolveMachineOriginForSave(Grid, Machine, RotationSteps);
		if (Origin.X == INT_MIN || Origin.Y == INT_MIN)
		{
			continue;
		}

		FFactorySavedMachineData SavedMachine;
		SavedMachine.InstanceId = Index;
		SavedMachine.ClassPath = Machine->GetClass()->GetPathName();
		SavedMachine.Origin = Origin;
		SavedMachine.RotationSteps = RotationSteps;
		SavedMachine.Transform = Machine->GetActorTransform();
		SavedMachine.bOccupancyOnly = Machine->OJJ_RequiresOccupancyOnlyRegistration();
		Machine->GetSaveState(SavedMachine.InputInventory, SavedMachine.OutputBuffer, SavedMachine.CurrentDurability);
		if (const AWarehousePort* WarehousePort = Cast<AWarehousePort>(Machine))
		{
			SavedMachine.SelectedOutputItemId = WarehousePort->GetSelectedOutputItem();
		}
		else if (const ALiquidTank* LiquidTank = Cast<ALiquidTank>(Machine))
		{
			SavedMachine.SelectedOutputItemId =
				ResolveSavedLiquidTankOutput(LiquidTank->GetSelectedOutputLiquid(), SavedMachine.OutputBuffer);
		}
		else if (const AMoldingMachine* MoldingMachine = Cast<AMoldingMachine>(Machine))
		{
			SavedMachine.MoldingShape = MoldingMachine->GetMoldingShape();
		}
		SaveGame->Machines.Add(SavedMachine);
		MachineIds.Add(Machine, SavedMachine.InstanceId);
	}

	for (TActorIterator<AOJJ_Foundation> It(World); It; ++It)
	{
		if (AOJJ_Foundation* Foundation = *It)
		{
			FFactorySavedFoundationData SavedFoundation;
			const TArray<FIntPoint>* FoundationCells = Grid->GetFoundationCells(Foundation);
			SavedFoundation.ClassPath = Foundation->GetClass()->GetPathName();
			SavedFoundation.Origin = ResolveFoundationOriginForSave(Grid, Foundation);
			SavedFoundation.Size = (FoundationCells && FoundationCells->Num() > 0)
				? FindSizeFromCells(*FoundationCells)
				: Foundation->GetFoundationSize();
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

	// [게임진입] 선택 캐릭터 복원(OJJ 합의). 외형 재적용은 OJJ_Player::BeginPlay 말미가 담당(영역 분리) — 여기선 값만 복원.
	if (UOJJ_CharacterSelectionSubsystem* CharacterSelection = GetGameInstance()->GetSubsystem<UOJJ_CharacterSelectionSubsystem>())
	{
		CharacterSelection->SetSelectedCharacter(SaveGame->SavedCharacterType);
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
		SaveGame->LastTutorialDialogueLines,
		SaveGame->TutorialProgressCounts);
	PlanetManager->RestoreSaveState(SaveGame->TimeState, SaveGame->WeatherState, SaveGame->EventState);

	DestroyActorsOfType<APowerLine>(World);
	DestroyActorsOfType<APipe>(World);
	DestroyActorsOfType<AConveyor>(World);
	DestroyActorsOfType<AOJJ_Ladder>(World);
	DestroyActorsOfType<AOJJ_Foundation>(World);
	TArray<AEscapePod*> ExistingEscapePods;
	for (TActorIterator<AEscapePod> It(World); It; ++It)
	{
		if (AEscapePod* EscapePod = *It)
		{
			ExistingEscapePods.Add(EscapePod);
		}
	}

	for (AEscapePod* ExistingEscapePod : ExistingEscapePods)
	{
		if (IsValid(ExistingEscapePod))
		{
			Grid->RemoveMachine(ExistingEscapePod);
		}
	}

	TArray<AMachineBase*> MachinesToDestroy;
	for (TActorIterator<AMachineBase> It(World); It; ++It)
	{
		if (AMachineBase* Machine = *It; Machine && !Machine->IsA<AEscapePod>())
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
			AMachineBase* Machine = nullptr;
			if (SavedMachine.bOccupancyOnly && MachineClass->IsChildOf(AEscapePod::StaticClass()))
			{
				for (int32 EscapePodIndex = 0; EscapePodIndex < ExistingEscapePods.Num(); ++EscapePodIndex)
				{
					AEscapePod* ExistingEscapePod = ExistingEscapePods[EscapePodIndex];
					if (ExistingEscapePod && ExistingEscapePod->GetClass() == MachineClass)
					{
						Machine = ExistingEscapePod;
						ExistingEscapePods.RemoveAt(EscapePodIndex);
						break;
					}
				}
			}

			if (!Machine)
			{
				Machine = World->SpawnActor<AMachineBase>(MachineClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
			}

			if (Machine)
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
						const FTransform PlacementTransform =
							Grid->OJJ_GetMachinePlacementTransform(Machine, SavedMachine.Origin, SavedMachine.RotationSteps);
						Machine->SetActorLocationAndRotation(
							PlacementTransform.GetLocation(),
							PlacementTransform.GetRotation());
					}
				}

				if (bPlaced)
				{
					if (AMoldingMachine* MoldingMachine = Cast<AMoldingMachine>(Machine))
					{
						MoldingMachine->SetMoldingShape(
							SavedMachine.MoldingShape.IsEmpty() ? TEXT("판") : SavedMachine.MoldingShape);
					}

					Machine->ApplySaveState(
						SavedMachine.InputInventory,
						SavedMachine.OutputBuffer,
						SavedMachine.CurrentDurability);
					if (AWarehousePort* WarehousePort = Cast<AWarehousePort>(Machine))
					{
						WarehousePort->SetSelectedOutputItem(SavedMachine.SelectedOutputItemId);
					}
					else if (ALiquidTank* LiquidTank = Cast<ALiquidTank>(Machine))
					{
						LiquidTank->SetSelectedOutputLiquid(
							ResolveSavedLiquidTankOutput(SavedMachine.SelectedOutputItemId, SavedMachine.OutputBuffer));
					}
					RestoredMachines.Add(SavedMachine.InstanceId, Machine);
				}
				else
				{
					if (!Machine->IsA<AEscapePod>())
					{
						Machine->Destroy();
					}
				}
			}
		}
	}

	for (AEscapePod* RemainingEscapePod : ExistingEscapePods)
	{
		if (IsValid(RemainingEscapePod))
		{
			RemainingEscapePod->Destroy();
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
				// [#184 철거] 로드 시에도 사다리 레이어 재등록 — 지면 셀 맵 + OwningFoundation 링크를 transform에서
				// 재계산(세이브에 링크 저장 불필요). Foundation은 위 루프(452)에서 선복원돼 조회 성립 → 리로드 후에도
				// 개별 철거 + Foundation 철거 cascade 정상.
				if (Grid)
				{
					Grid->OJJ_RegisterLadder(Ladder);
				}
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
	bIsResettingToNewGame = true;
	return UGameplayStatics::DeleteGameInSlot(SaveSlotName, 0);
}

bool UFactorySaveSubsystem::BackupCurrentGame()
{
	if (!SaveCurrentGame())
	{
		return false;
	}

	UFactorySaveGame* SaveGame = Cast<UFactorySaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));
	if (!SaveGame)
	{
		return false;
	}

	return UGameplayStatics::SaveGameToSlot(SaveGame, BackupSlotName, 0);
}

bool UFactorySaveSubsystem::RestoreBackupGame()
{
	if (!UGameplayStatics::DoesSaveGameExist(BackupSlotName, 0))
	{
		return false;
	}

	UFactorySaveGame* BackupSave = Cast<UFactorySaveGame>(UGameplayStatics::LoadGameFromSlot(BackupSlotName, 0));
	if (!BackupSave)
	{
		return false;
	}

	bHasLoadedInitialState = false;
	bIsResettingToNewGame = true;
	bHasHandledShutdownSave = false;

	if (!UGameplayStatics::SaveGameToSlot(BackupSave, SaveSlotName, 0))
	{
		bIsResettingToNewGame = false;
		return false;
	}

	return true;
}

bool UFactorySaveSubsystem::HasBackupGame() const
{
	return UGameplayStatics::DoesSaveGameExist(BackupSlotName, 0);
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
	World->GetTimerManager().ClearTimer(AutoSaveWarningBlinkTimerHandle);
	RemainingAutoSaveWarningBlinks = 0;
}

void UFactorySaveSubsystem::AutoSaveTick()
{
	if (AOJJ_Player* Player = CachedPlayer.Get())
	{
		if (UWorld* World = Player->GetWorld())
		{
			World->GetTimerManager().ClearTimer(AutoSaveWarningBlinkTimerHandle);
		}
		RemainingAutoSaveWarningBlinks = 0;
		Player->ShowMainHUDSaveIndicator(1.0f);
	}

	if (SaveCurrentGame())
	{
		NotifyProcessOptimizerAutoSave();
	}
}

void UFactorySaveSubsystem::AutoSaveWarningTick()
{
	AOJJ_Player* Player = CachedPlayer.Get();
	UWorld* World = Player ? Player->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(AutoSaveWarningBlinkTimerHandle);
	RemainingAutoSaveWarningBlinks = FMath::Max(1, FMath::CeilToInt(AutoSaveWarningLeadSeconds));
	ShowAutoSaveWarning();
	--RemainingAutoSaveWarningBlinks;

	if (RemainingAutoSaveWarningBlinks > 0)
	{
		World->GetTimerManager().SetTimer(
			AutoSaveWarningBlinkTimerHandle,
			this,
			&UFactorySaveSubsystem::AutoSaveWarningBlinkTick,
			1.0f,
			true);
	}
}

void UFactorySaveSubsystem::AutoSaveWarningBlinkTick()
{
	if (RemainingAutoSaveWarningBlinks <= 0)
	{
		if (AOJJ_Player* Player = CachedPlayer.Get())
		{
			if (UWorld* World = Player->GetWorld())
			{
				World->GetTimerManager().ClearTimer(AutoSaveWarningBlinkTimerHandle);
			}
		}
		return;
	}

	ShowAutoSaveWarning();
	--RemainingAutoSaveWarningBlinks;
}

void UFactorySaveSubsystem::ShowAutoSaveWarning() const
{
	if (AOJJ_Player* Player = CachedPlayer.Get())
	{
		Player->ShowMainHUDSaveIndicator(0.45f);
		return;
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1771,
			FMath::Max(0.5f, AutoSaveWarningLeadSeconds),
			FColor::Yellow,
			TEXT("1분마다 자동 저장됩니다. 곧 저장합니다..."));
	}
}

void UFactorySaveSubsystem::EnsureAgentConnection()
{
	if (UFactoryAgentClientSubsystem* AgentClient = GetGameInstance()->GetSubsystem<UFactoryAgentClientSubsystem>())
	{
		if (AgentClient->GetConnectionState() == EFactoryAgentConnectionState::Disconnected)
		{
			AgentClient->ConnectToDefaultServer();
		}
	}
}

void UFactorySaveSubsystem::NotifyProcessOptimizerAutoSave()
{
	UFactoryAgentClientSubsystem* AgentClient = GetGameInstance()->GetSubsystem<UFactoryAgentClientSubsystem>();
	if (!AgentClient)
	{
		return;
	}

	if (!AgentClient->SendProcessOptimizerStateUpdate(NextProcessOptimizerFactoryRevision, TEXT(""), TEXT("")))
	{
		if (AgentClient->GetConnectionState() == EFactoryAgentConnectionState::Disconnected)
		{
			AgentClient->ConnectToDefaultServer();
		}
		return;
	}

	++NextProcessOptimizerFactoryRevision;
}

void UFactorySaveSubsystem::HandlePreExitSave()
{
	if (bIsResettingToNewGame || bHasHandledShutdownSave)
	{
		return;
	}

	if (SaveCurrentGame())
	{
		bHasHandledShutdownSave = true;
	}
}

void UFactorySaveSubsystem::HandleWorldBeginTearDown(UWorld* World)
{
	if (bIsResettingToNewGame || bHasHandledShutdownSave)
	{
		return;
	}

	AOJJ_Player* Player = CachedPlayer.Get();
	if (!Player || Player->GetWorld() != World)
	{
		return;
	}

	if (SaveCurrentGame())
	{
		bHasHandledShutdownSave = true;
	}
}
