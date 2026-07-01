#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FactorySaveSubsystem.generated.h"

class AOJJ_Player;

UCLASS()
class WANTED_FACTORY_API UFactorySaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Factory Save")
	void HandlePlayerReady(AOJJ_Player* Player);

	UFUNCTION(BlueprintCallable, Category = "Factory Save")
	bool SaveCurrentGame();

	UFUNCTION(BlueprintCallable, Category = "Factory Save")
	bool LoadCurrentGame();

	UFUNCTION(BlueprintCallable, Category = "Factory Save")
	bool ResetToNewGame();

	UFUNCTION(BlueprintCallable, Category = "Factory Save")
	bool BackupCurrentGame();

	UFUNCTION(BlueprintCallable, Category = "Factory Save")
	bool RestoreBackupGame();

	UFUNCTION(BlueprintPure, Category = "Factory Save")
	bool HasBackupGame() const;

	UFUNCTION(BlueprintPure, Category = "Factory Save")
	bool HasLoadedInitialState() const { return bHasLoadedInitialState; }

private:
	UPROPERTY()
	FString SaveSlotName = TEXT("FactorySpace_Autosave");

	UPROPERTY()
	FString BackupSlotName = TEXT("FactorySpace_Backup");

	UPROPERTY()
	float AutoSaveIntervalSeconds = 60.0f;

	UPROPERTY()
	float AutoSaveWarningLeadSeconds = 5.0f;

	TWeakObjectPtr<AOJJ_Player> CachedPlayer;
	FTimerHandle AutoSaveTimerHandle;
	FTimerHandle AutoSaveWarningTimerHandle;
	FTimerHandle AutoSaveWarningBlinkTimerHandle;
	int32 NextProcessOptimizerFactoryRevision = 1;
	int32 RemainingAutoSaveWarningBlinks = 0;
	bool bHasLoadedInitialState = false;
	bool bIsRestoring = false;
	bool bIsResettingToNewGame = false;
	bool bHasHandledShutdownSave = false;

	void StartAutoSaveTimer();
	void StopAutoSaveTimer();
	void AutoSaveTick();
	void AutoSaveWarningTick();
	void AutoSaveWarningBlinkTick();
	void ShowAutoSaveWarning() const;
	void EnsureAgentConnection();
	void NotifyProcessOptimizerAutoSave();
	void HandlePreExitSave();
	void HandleWorldBeginTearDown(UWorld* World);
};
