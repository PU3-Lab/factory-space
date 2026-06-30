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
	bool FindMachineDataForLevel(FName MachineType, int32 Level, FMachineTableRow& OutMachineData) const;

	UFUNCTION(BlueprintCallable, Category = "Machine")
	bool FindMachineDataByRowName(FName RowName, FMachineTableRow& OutMachineData) const;

	UFUNCTION(BlueprintCallable, Category = "Machine")
	void GetAllMachineData(TArray<FMachineTableRow>& OutMachineData) const;

	UFUNCTION(BlueprintPure, Category = "Machine")
	int32 GetMachineLevel(FName MachineType) const;

	UFUNCTION(BlueprintCallable, Category = "Machine")
	bool SetMachineLevel(FName MachineType, int32 NewLevel);

	UFUNCTION(BlueprintPure, Category = "Machine")
	bool CanUpgradeMachineType(FName MachineType) const;

	UFUNCTION(BlueprintCallable, Category = "Machine")
	bool UpgradeMachineType(FName MachineType);

	UFUNCTION(BlueprintPure, Category = "Machine")
	FText GetMachineDisplayName(FName MachineType) const;

	const TMap<FName, int32>& GetMachineLevels() const { return MachineTypeToCurrentLevel; }
	void RestoreMachineLevels(const TMap<FName, int32>& Levels);

	// 머신 레벨 초기화
    void ResetAllMachineLevels();
private:
	TMap<FName, FName> MachineNameToRowMap;
	TMap<FName, TMap<int32, FName>> MachineTypeLevelToRowMap;
	TMap<FName, int32> MachineTypeToCurrentLevel;

	void ApplyMachineDataToExistingMachines(FName MachineType, const FMachineTableRow& MachineData) const;
};
