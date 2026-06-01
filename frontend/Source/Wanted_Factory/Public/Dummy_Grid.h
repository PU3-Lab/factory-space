// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Dummy_Grid.generated.h"

class ADummyConveyor;
class AMachineBase;
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;

UCLASS()
class WANTED_FACTORY_API ADummyGrid : public AActor
{
	GENERATED_BODY()

public:
	ADummyGrid();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Settings", meta = (ClampMin = "1.0"))
	float CellSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Settings", meta = (ClampMin = "1"))
	FIntPoint GridSize = FIntPoint(20, 20);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid|Visualization")
	TObjectPtr<UStaticMeshComponent> GridFloorMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Visualization", meta = (ClampMin = "1"))
	int32 VisualizationRange;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid|Hover")
	TObjectPtr<UInstancedStaticMeshComponent> ValidHoverISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid|Hover")
	TObjectPtr<UInstancedStaticMeshComponent> InvalidHoverISM;

	UPROPERTY(Transient)
	TMap<FIntPoint, TWeakObjectPtr<AActor>> OccupiedCells;

	TMap<TWeakObjectPtr<AActor>, TArray<FIntPoint>> ActorToCells;

private:
	TArray<FIntPoint> CalculateFootprint(AMachineBase* Machine, FIntPoint Origin, int32 RotationSteps = 0) const;
	void SweepStaleEntries();
	bool RegisterActorCells(AActor* Actor, const TArray<FIntPoint>& Cells, FString& OutReason);
	bool RemoveActor(AActor* Actor);

public:
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintPure, Category = "Grid|Coordinate")
	FIntPoint WorldToGrid(FVector WorldPos) const;

	UFUNCTION(BlueprintPure, Category = "Grid|Coordinate")
	FVector GridToWorld(FIntPoint Coord) const;

	UFUNCTION(BlueprintPure, Category = "Grid|Coordinate")
	FVector GetGridCenter() const;

	UFUNCTION(BlueprintPure, Category = "Grid|Coordinate")
	float GetCellSize() const { return CellSize; }

	UFUNCTION(BlueprintPure, Category = "Grid|Coordinate")
	static FIntPoint EffectiveSize(FVector2D RawSize, int32 RotationSteps);

	UFUNCTION(BlueprintPure, Category = "Grid|Coordinate")
	FVector GetMachinePlacementLocation(AMachineBase* Machine, FIntPoint Origin, int32 RotationSteps = 0) const;

	UFUNCTION(BlueprintPure, Category = "Grid|Coordinate")
	bool IsValidGridCell(FIntPoint Cell) const;

	UFUNCTION(BlueprintPure, Category = "Grid|Placement")
	bool CanPlaceCells(const TArray<FIntPoint>& Cells) const;

	bool BuildConveyorPlacementPath(
		const TArray<FIntPoint>& DragCells,
		TArray<FIntPoint>& OutPathCells,
		FString& OutReason) const;

	bool CanPlaceConveyorPath(const TArray<FIntPoint>& PathCells) const;

	UFUNCTION(BlueprintPure, Category = "Grid|Placement")
	bool CanPlaceMachine(AMachineBase* Machine, FIntPoint Origin, int32 RotationSteps = 0) const;

	UFUNCTION(BlueprintCallable, Category = "Grid|Placement")
	bool TryPlaceMachine(AMachineBase* Machine, FIntPoint Origin, FString& OutReason, int32 RotationSteps = 0);

	UFUNCTION(BlueprintCallable, Category = "Grid|Placement")
	bool TryPlaceConveyor(ADummyConveyor* Conveyor, const TArray<FIntPoint>& PathCells, FString& OutReason);

	UFUNCTION(BlueprintCallable, Category = "Grid|Placement")
	bool RemoveMachine(AMachineBase* Machine);

	UFUNCTION(BlueprintCallable, Category = "Grid|Placement")
	bool RemoveActorAt(FIntPoint Coord);

	UFUNCTION(BlueprintCallable, Category = "Grid|Placement")
	bool RegisterExistingMachine(AMachineBase* Machine, FIntPoint Origin, FString& OutReason);

	UFUNCTION(BlueprintCallable, Category = "Grid|Visualization")
	void SetVisualizationVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "Grid|Visualization")
	UStaticMeshComponent* GetGridFloorMesh() const { return GridFloorMesh; }

	UFUNCTION(BlueprintCallable, Category = "Grid|Hover")
	void UpdateHoverPreview(AMachineBase* Machine, FIntPoint Origin, int32 RotationSteps = 0);

	UFUNCTION(BlueprintCallable, Category = "Grid|Hover")
	void UpdatePathHoverPreview(const TArray<FIntPoint>& PathCells);

	void UpdateConveyorPathHoverPreview(const TArray<FIntPoint>& PathCells);

	UFUNCTION(BlueprintCallable, Category = "Grid|Hover")
	void ClearHoverPreview();
};
