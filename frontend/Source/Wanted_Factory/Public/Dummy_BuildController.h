// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Dummy_BuildController.generated.h"

class ADummyConveyor;
class ADummyGrid;
class AMachineBase;

UENUM(BlueprintType)
enum class EDummyBuildPlacementMode : uint8
{
	Machine,
	Conveyor
};

UCLASS()
class WANTED_FACTORY_API ADummyBuildController : public AActor
{
	GENERATED_BODY()

public:
	ADummyBuildController();

	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "BuildController")
	TObjectPtr<ADummyGrid> TargetGrid;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildController")
	TSubclassOf<AMachineBase> MachineClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildController")
	TSubclassOf<ADummyConveyor> ConveyorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildController")
	EDummyBuildPlacementMode PlacementMode = EDummyBuildPlacementMode::Machine;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BuildController")
	bool bIsBuildMode = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BuildController")
	FIntPoint CurrentHoverCell = FIntPoint::ZeroValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BuildController")
	int32 HoverRotationSteps = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BuildController|Conveyor")
	bool bIsDraggingConveyor = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BuildController|Conveyor")
	TArray<FIntPoint> ConveyorDragCells;

public:
	UFUNCTION(BlueprintCallable, Category = "BuildController")
	void EnterBuildMode();

	UFUNCTION(BlueprintCallable, Category = "BuildController")
	void ExitBuildMode();

	UFUNCTION(BlueprintCallable, Category = "BuildController")
	bool IsInBuildMode() const { return bIsBuildMode; }

	UFUNCTION(BlueprintPure, Category = "BuildController")
	ADummyGrid* GetTargetGrid() const { return TargetGrid; }

	UFUNCTION(BlueprintCallable, Category = "BuildController")
	void SetPlacementMode(EDummyBuildPlacementMode NewMode);

	UFUNCTION(BlueprintPure, Category = "BuildController")
	EDummyBuildPlacementMode GetPlacementMode() const { return PlacementMode; }

	UFUNCTION(BlueprintCallable, Category = "BuildController")
	void UpdateMouseHover();

	UFUNCTION(BlueprintCallable, Category = "BuildController")
	void OnLeftClickPressed();

	UFUNCTION(BlueprintCallable, Category = "BuildController")
	void OnLeftClickReleased();

	UFUNCTION(BlueprintCallable, Category = "BuildController")
	void CancelConveyorDrag();

	UFUNCTION(BlueprintCallable, Category = "BuildController")
	void ToggleBuildMode();

	UFUNCTION(BlueprintCallable, Category = "BuildController")
	void RotateHoverClockwise();

private:
	bool GetCursorCell(FIntPoint& OutCell) const;
	FIntPoint ComputeOriginFromCursorCell(FIntPoint CursorCell, AMachineBase* Machine, int32 RotationSteps = 0) const;
	void UpdateMachineHover(FIntPoint CursorCell);
	void UpdateConveyorHover(FIntPoint CursorCell);
	void BeginConveyorDrag(FIntPoint StartCell);
	void UpdateConveyorDrag(FIntPoint CursorCell);
	void CommitConveyorDrag();
	void AppendConveyorPathTo(FIntPoint TargetCell);
	void AddConveyorPathCell(FIntPoint Cell);
	void PlaceMachineAtHover();
};
