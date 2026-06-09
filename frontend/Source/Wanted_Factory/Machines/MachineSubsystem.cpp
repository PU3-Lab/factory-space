#include "MachineSubsystem.h"

#include "Wanted_Factory.h"
#include "Engine/DataTable.h"
#include "UObject/ConstructorHelpers.h"

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
		LOG_SSR_W(TEXT("MachineTable Load Failed"));
		return;
	}

	LOG_SSR_W(TEXT("MachineTable Loaded"));

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

		if (!MachineData->MachineType.IsNone() && MachineData->Level == 2)
		{
			MachineNameToRowMap.Add(MachineData->MachineType, RowName);
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

	return FindMachineDataByRowName(MachineName, OutMachineData);
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
