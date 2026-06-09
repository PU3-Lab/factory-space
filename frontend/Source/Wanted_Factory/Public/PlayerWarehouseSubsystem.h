#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PlayerWarehouseSubsystem.generated.h"

UCLASS()
class WANTED_FACTORY_API UPlayerWarehouseSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category = "Player Warehouse")
	bool AddItem(FName ItemID, int32 Count);

	UFUNCTION(BlueprintPure, Category = "Player Warehouse")
	bool CanTakeItem(FName ItemID, int32 Count) const;

	UFUNCTION(BlueprintCallable, Category = "Player Warehouse")
	bool TakeItem(FName ItemID, int32 Count);

	UFUNCTION(BlueprintPure, Category = "Player Warehouse")
	int32 GetItemCount(FName ItemID) const;

	UFUNCTION(BlueprintPure, Category = "Player Warehouse")
	const TMap<FName, int32>& GetStoredItems() const { return StoredItems; }

	UFUNCTION(BlueprintCallable, Category = "Player Warehouse")
	void ClearWarehouse();

private:
	UPROPERTY(VisibleAnywhere, Category = "Player Warehouse")
	TMap<FName, int32> StoredItems;
};
