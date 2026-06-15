#pragma once

#include "CoreMinimal.h"
#include "MachineBase.h"
#include "WarehousePort.generated.h"

class UPlayerWarehouseSubsystem;
class UDataTable;

UCLASS()
class WANTED_FACTORY_API AWarehousePort : public AMachineBase
{
	GENERATED_BODY()

public:
	AWarehousePort();

	UFUNCTION(BlueprintCallable, Category = "Warehouse Port")
	void SetSelectedOutputItem(FName ItemID);

	UFUNCTION(BlueprintPure, Category = "Warehouse Port")
	FName GetSelectedOutputItem() const { return SelectedOutputItemID; }

	UFUNCTION(BlueprintPure, Category = "Warehouse Port")
	int32 GetSelectedOutputItemCount() const;

	virtual bool AddItem(FName ItemID, int32 Count) override;
	virtual bool CanReceiveConveyorItem(FName ItemID, int32 Count = 1) const override;
	virtual bool ReceiveConveyorItem(FName ItemID, int32 Count = 1) override;
	virtual bool PeekFirstOutputItem(FName& OutItemID) const override;
	virtual bool TryTakeFirstOutputItem(FName& OutItemID) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warehouse Port")
	FName SelectedOutputItemID = NAME_None;

private:
	UPlayerWarehouseSubsystem* GetWarehouse() const;
	bool IsSolidItem(FName ItemID) const;
	bool CanStoreSolid(FName ItemID, int32 Count) const;
	bool StoreInputItem(FName ItemID, int32 Count);

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> ResourceTable;
};
