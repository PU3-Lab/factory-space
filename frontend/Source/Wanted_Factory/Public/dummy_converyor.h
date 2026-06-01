// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "dummy_converyor.generated.h"

class UInstancedStaticMeshComponent;
class USceneComponent;

UCLASS()
class WANTED_FACTORY_API ADummyConveyor : public AActor
{
	GENERATED_BODY()

public:
	ADummyConveyor();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Conveyor|Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Conveyor|Components")
	TObjectPtr<UInstancedStaticMeshComponent> StraightSegmentInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Conveyor|Components")
	TObjectPtr<UInstancedStaticMeshComponent> CornerSegmentInstances;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Grid", meta = (ClampMin = "1.0"))
	float CellSize = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Visual", meta = (ClampMin = "0.0"))
	float SegmentWidthRatio = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Visual", meta = (ClampMin = "0.0"))
	float SegmentHeight = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Visual")
	float ZOffset = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Conveyor|Path")
	TArray<FIntPoint> PathCells;

public:
	UFUNCTION(BlueprintCallable, Category = "Conveyor|Path")
	void SetPath(const TArray<FIntPoint>& NewPathCells, float NewCellSize);

	UFUNCTION(BlueprintCallable, Category = "Conveyor|Path")
	void ClearPath();

	UFUNCTION(BlueprintPure, Category = "Conveyor|Path")
	TArray<FIntPoint> GetPathCells() const { return PathCells; }

	UFUNCTION(BlueprintPure, Category = "Conveyor|Grid")
	float GetCellSize() const { return CellSize; }

private:
	void RebuildVisuals();
};
