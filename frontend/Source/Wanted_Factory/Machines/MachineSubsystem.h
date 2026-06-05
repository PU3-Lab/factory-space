#pragma once

#include "CoreMinimal.h"
#include "MachineTable.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MachineSubsystem.generated.h"

UCLASS()
class WANTED_FACTORY_API UMachineSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UMachineSubsystem();

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Machine")
	UDataTable* MachineTable;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	void BuildMachineIndex();

	UFUNCTION(BlueprintCallable, Category = "Machine")
	bool FindMachineData(FName MachineName, FMachineTableRow& OutMachineData) const;

	UFUNCTION(BlueprintCallable, Category = "Machine")
	bool FindMachineDataByRowName(FName RowName, FMachineTableRow& OutMachineData) const;

	UFUNCTION(BlueprintCallable, Category = "Machine")
	void GetAllMachineData(TArray<FMachineTableRow>& OutMachineData) const;

private:
	TMap<FName, FName> MachineNameToRowMap;
};
