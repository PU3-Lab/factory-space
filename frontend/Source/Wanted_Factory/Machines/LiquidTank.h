#pragma once

#include "CoreMinimal.h"
#include "MachineBase.h"
#include "LiquidTank.generated.h"

class UDataTable;
class UPlayerWarehouseSubsystem;

UCLASS()
class WANTED_FACTORY_API ALiquidTank : public AMachineBase
{
	GENERATED_BODY()

public:
	ALiquidTank();

	UFUNCTION(BlueprintCallable, Category = "Liquid Tank")
	void SetSelectedOutputLiquid(FName ItemID);

	UFUNCTION(BlueprintPure, Category = "Liquid Tank")
	FName GetSelectedOutputLiquid() const { return SelectedOutputLiquidID; }

	UFUNCTION(BlueprintPure, Category = "Liquid Tank")
	FName GetStoredLiquidID() const;

	UFUNCTION(BlueprintPure, Category = "Liquid Tank")
	int32 GetStoredLiquidAmount() const;

	UFUNCTION(BlueprintPure, Category = "Liquid Tank")
	int32 GetCapacity() const { return Capacity; }

	virtual bool AddItem(FName ItemID, int32 Count) override;
	bool TakeOutputItem(FName ItemID, int32 Count);
	virtual bool CanReceiveConveyorItem(FName ItemID, int32 Count = 1) const override;
	virtual bool ReceiveConveyorItem(FName ItemID, int32 Count = 1) override;
	virtual bool PeekFirstOutputItem(FName& OutItemID) const override;
	virtual bool TryTakeFirstOutputItem(FName& OutItemID) override;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Liquid Tank", meta = (ClampMin = "1"))
	int32 Capacity = 500;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Liquid Tank")
	FName SelectedOutputLiquidID = NAME_None;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> ResourceTable;

private:
	UPlayerWarehouseSubsystem* GetWarehouse() const;
	bool IsLiquidItem(FName ItemID) const;
	bool CanStoreLiquid(FName ItemID, int32 Count) const;
	bool StoreLiquid(FName ItemID, int32 Count);
	void SyncDisplayedBuffer();
};
