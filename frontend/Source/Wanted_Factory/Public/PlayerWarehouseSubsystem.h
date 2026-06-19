#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PlayerWarehouseSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWarehouseItemAdded, FName, ItemID, int32, AddedCount, int32, NewTotalCount);

USTRUCT(BlueprintType)
struct FWarehouseItemStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Warehouse")
	FName ItemID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Warehouse", meta = (ClampMin = "1"))
	int32 Count = 1;
};

UCLASS()
class WANTED_FACTORY_API UPlayerWarehouseSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Player Warehouse")
	FOnWarehouseItemAdded OnItemAdded;

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

	UFUNCTION(BlueprintCallable, Category = "Player Warehouse")
	void GrantInitialItems(const TArray<FWarehouseItemStack>& Items);

	void SetStoredItemsForSave(const TMap<FName, int32>& Items);

private:
	UPROPERTY(VisibleAnywhere, Category = "Player Warehouse")
	TMap<FName, int32> StoredItems;
};
