// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MachineBase.h"
#include "Dummy_MachineBase.generated.h"

UCLASS()
class WANTED_FACTORY_API ADummyMachineBase : public AMachineBase
{
	GENERATED_BODY()

public:
	ADummyMachineBase();

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dummy Machine|Debug")
	TObjectPtr<class UTextRenderComponent> DebugBufferText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy Machine|Debug")
	bool bShowDebugBufferText = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy Machine|Debug")
	FVector DebugTextOffset = FVector(0.0f, 0.0f, 180.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy Machine|Debug", meta = (ClampMin = "1.0"))
	float DebugTextWorldSize = 24.0f;

public:
	UFUNCTION(BlueprintPure, Category = "Dummy Machine|Conveyor")
	bool PeekFirstOutputItem(FName& OutItemID) const;

	UFUNCTION(BlueprintCallable, Category = "Dummy Machine|Conveyor")
	bool TryTakeFirstOutputItem(FName& OutItemID);

	UFUNCTION(BlueprintPure, Category = "Dummy Machine|Conveyor")
	bool CanReceiveConveyorItem(FName ItemID, int32 Count = 1) const;

	UFUNCTION(BlueprintCallable, Category = "Dummy Machine|Conveyor")
	bool ReceiveConveyorItem(FName ItemID, int32 Count = 1);

	UFUNCTION(BlueprintCallable, Category = "Dummy Machine|Debug")
	void UpdateDebugBufferText();
};
