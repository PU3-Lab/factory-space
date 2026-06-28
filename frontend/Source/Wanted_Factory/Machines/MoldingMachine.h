// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MachineBase.h"
#include "MoldingMachine.generated.h"

UCLASS()
class WANTED_FACTORY_API AMoldingMachine : public AMachineBase
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AMoldingMachine();

	virtual void ApplyMachineData(const FMachineTableRow& MachineData) override;
	virtual bool AddItem(FName ItemID, int32 Count) override;
	virtual void AddOutputItem(FName ItemID, int32 Count) override;
	virtual bool CanAddToOutputBuffer(const FRecipeTable& Recipe) const override;
	virtual bool CanReceiveConveyorItem(FName ItemID, int32 Count = 1) const override;
	void SetMoldingShape(const FString& NewShape) { CurrentShape = NewShape; }
	FString GetMoldingShape() const { return CurrentShape; }
	
protected:
	// ---UI----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine | State")
	FString CurrentShape = TEXT("판");
};
