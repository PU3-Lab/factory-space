#include "MachineSubsystem.h"

#include "EngineUtils.h"
#include "MachineBase.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PlayerWarehouseSubsystem.h"
#include "Wanted_Factory.h"
#include "Engine/DataTable.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr TCHAR MachineCsvRelativePath[] = TEXT("Source/Wanted_Factory/Data/MachineTable.csv");
}

UMachineSubsystem::UMachineSubsystem()
{
	static ConstructorHelpers::FObjectFinder<UDataTable> MachineTableFinder(
		TEXT("/Game/DataTable/DT_MachineData.DT_MachineData")
	);

	if (MachineTableFinder.Succeeded())
	{
		MachineTable = MachineTableFinder.Object;
	}
}

void UMachineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (!MachineTable)
	{
		MachineTable = NewObject<UDataTable>(this, TEXT("MachineTable"));
		if (MachineTable)
		{
			MachineTable->RowStruct = FMachineTableRow::StaticStruct();
		}
	}

	if (!MachineTable)
	{
		LOG_SSR_W(TEXT("MachineTable Load Failed"));
		return;
	}

	LOG_SSR_W(TEXT("MachineTable Loaded"));

	const FString CsvPath = FPaths::Combine(FPaths::ProjectDir(), MachineCsvRelativePath);
	FString CsvContent;
	if (FFileHelper::LoadFileToString(CsvContent, *CsvPath))
	{
		MachineTable->EmptyTable();
		const TArray<FString> ImportProblems = MachineTable->CreateTableFromCSVString(CsvContent);
		for (const FString& Problem : ImportProblems)
		{
			LOG_SSR_W(TEXT("MachineTable CSV import warning: %s"), *Problem);
		}
	}
	else
	{
		LOG_SSR_W(TEXT("MachineTable CSV Load Failed: %s"), *CsvPath);
	}

	BuildMachineIndex();
}

void UMachineSubsystem::BuildMachineIndex()
{
	if (!MachineTable)
	{
		LOG_SSR_W(TEXT("MachineTable is NULL"));
		return;
	}

	MachineNameToRowMap.Empty();
	MachineTypeLevelToRowMap.Empty();
	MachineTypeToCurrentLevel.Empty();

	const TArray<FName> RowNames = MachineTable->GetRowNames();
	for (const FName& RowName : RowNames)
	{
		const FMachineTableRow* MachineData =
			MachineTable->FindRow<FMachineTableRow>(
				RowName,
				TEXT("BuildMachineIndex")
			);

		if (!MachineData)
		{
			continue;
		}

		MachineNameToRowMap.Add(RowName, RowName);

		if (!MachineData->MachineType.IsNone() && MachineData->Level > 0)
		{
			TMap<int32, FName>& LevelToRowMap = MachineTypeLevelToRowMap.FindOrAdd(MachineData->MachineType);
			LevelToRowMap.Add(MachineData->Level, RowName);

			int32& CurrentLevel = MachineTypeToCurrentLevel.FindOrAdd(MachineData->MachineType);
			if (CurrentLevel == 0 || MachineData->Level < CurrentLevel)
			{
				CurrentLevel = MachineData->Level;
			}
		}

		LOG_SSR_W(
			TEXT("Machine Indexed: %s -> %s"),
			*RowName.ToString(),
			*RowName.ToString()
		);
	}
}

bool UMachineSubsystem::FindMachineData(FName MachineName, FMachineTableRow& OutMachineData) const
{
	if (!MachineTable || MachineName.IsNone())
	{
		return false;
	}

	const FName* FoundRowName = MachineNameToRowMap.Find(MachineName);
	if (FoundRowName)
	{
		return FindMachineDataByRowName(*FoundRowName, OutMachineData);
	}

	const int32 CurrentLevel = GetMachineLevel(MachineName);
	return FindMachineDataForLevel(MachineName, CurrentLevel, OutMachineData);
}

bool UMachineSubsystem::FindMachineDataForLevel(FName MachineType, int32 Level, FMachineTableRow& OutMachineData) const
{
	if (!MachineTable || MachineType.IsNone() || Level <= 0)
	{
		return false;
	}

	const TMap<int32, FName>* LevelToRowMap = MachineTypeLevelToRowMap.Find(MachineType);
	if (!LevelToRowMap)
	{
		return false;
	}

	const FName* FoundRowName = LevelToRowMap->Find(Level);
	if (!FoundRowName)
	{
		return false;
	}

	return FindMachineDataByRowName(*FoundRowName, OutMachineData);
}

bool UMachineSubsystem::FindMachineDataByRowName(FName RowName, FMachineTableRow& OutMachineData) const
{
	if (!MachineTable || RowName.IsNone())
	{
		return false;
	}

	const FMachineTableRow* MachineData =
		MachineTable->FindRow<FMachineTableRow>(
			RowName,
			TEXT("FindMachineDataByRowName")
		);

	if (!MachineData)
	{
		return false;
	}

	OutMachineData = *MachineData;
	return true;
}

void UMachineSubsystem::GetAllMachineData(TArray<FMachineTableRow>& OutMachineData) const
{
	OutMachineData.Empty();

	if (!MachineTable)
	{
		return;
	}

	const TArray<FName> RowNames = MachineTable->GetRowNames();
	for (const FName& RowName : RowNames)
	{
		FMachineTableRow MachineData;
		if (FindMachineDataByRowName(RowName, MachineData))
		{
			OutMachineData.Add(MachineData);
		}
	}
}

int32 UMachineSubsystem::GetMachineLevel(FName MachineType) const
{
	if (MachineType.IsNone())
	{
		return 0;
	}

	const int32* FoundLevel = MachineTypeToCurrentLevel.Find(MachineType);
	return FoundLevel ? *FoundLevel : 0;
}

bool UMachineSubsystem::SetMachineLevel(FName MachineType, int32 NewLevel)
{
	FMachineTableRow MachineData;
	if (!FindMachineDataForLevel(MachineType, NewLevel, MachineData))
	{
		return false;
	}

	MachineTypeToCurrentLevel.Add(MachineType, NewLevel);
	ApplyMachineDataToExistingMachines(MachineType, MachineData);
	return true;
}

bool UMachineSubsystem::CanUpgradeMachineType(FName MachineType) const
{
	const int32 NextLevel = GetMachineLevel(MachineType) + 1;

	FMachineTableRow NextMachineData;
	if (!FindMachineDataForLevel(MachineType, NextLevel, NextMachineData))
	{
		return false;
	}

	if (NextMachineData.CostQty <= 0)
	{
		return true;
	}

	if (NextMachineData.CostType.IsNone())
	{
		return false;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const UPlayerWarehouseSubsystem* Warehouse = GameInstance
		? GameInstance->GetSubsystem<UPlayerWarehouseSubsystem>()
		: nullptr;
	return Warehouse && Warehouse->CanTakeItem(NextMachineData.CostType, NextMachineData.CostQty);
}

bool UMachineSubsystem::UpgradeMachineType(FName MachineType)
{
	const int32 NextLevel = GetMachineLevel(MachineType) + 1;

	FMachineTableRow NextMachineData;
	if (!FindMachineDataForLevel(MachineType, NextLevel, NextMachineData))
	{
		return false;
	}

	if (NextMachineData.CostQty > 0)
	{
		if (NextMachineData.CostType.IsNone())
		{
			return false;
		}

		UGameInstance* GameInstance = GetGameInstance();
		UPlayerWarehouseSubsystem* Warehouse = GameInstance
			? GameInstance->GetSubsystem<UPlayerWarehouseSubsystem>()
			: nullptr;
		if (!Warehouse || !Warehouse->TakeItem(NextMachineData.CostType, NextMachineData.CostQty))
		{
			return false;
		}
	}

	MachineTypeToCurrentLevel.Add(MachineType, NextLevel);
	ApplyMachineDataToExistingMachines(MachineType, NextMachineData);
	return true;
}

FText UMachineSubsystem::GetMachineDisplayName(FName MachineType) const
{
	if (MachineType.IsNone())
	{
		return FText::GetEmpty();
	}

	FMachineTableRow MachineData;
	if (FindMachineData(MachineType, MachineData) && !MachineData.DisplayName.IsEmpty())
	{
		return FText::FromString(MachineData.DisplayName);
	}

	return FText::FromName(MachineType);
}

void UMachineSubsystem::RestoreMachineLevels(const TMap<FName, int32>& Levels)
{
	MachineTypeToCurrentLevel.Reset();

	for (const TPair<FName, int32>& Pair : Levels)
	{
		FMachineTableRow MachineData;
		if (FindMachineDataForLevel(Pair.Key, Pair.Value, MachineData))
		{
			MachineTypeToCurrentLevel.Add(Pair.Key, Pair.Value);
		}
	}
}

void UMachineSubsystem::ApplyMachineDataToExistingMachines(FName MachineType, const FMachineTableRow& MachineData) const
{
	UWorld* World = GetWorld();
	if (!World || MachineType.IsNone())
	{
		return;
	}

	for (TActorIterator<AMachineBase> MachineIt(World); MachineIt; ++MachineIt)
	{
		AMachineBase* Machine = *MachineIt;
		if (Machine && Machine->GetMachineType() == MachineType)
		{
			Machine->ApplyMachineData(MachineData);
		}
	}
}
