#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Pipe.generated.h"

class AMachineBase;
class UDataTable;
class UInstancedStaticMeshComponent;
class USceneComponent;
class UTextRenderComponent;

USTRUCT()
struct FPipeLiquidSlot
{
	GENERATED_BODY()

	UPROPERTY()
	FName LiquidID = NAME_None;

	UPROPERTY()
	int32 Amount = 0;

	bool IsEmpty() const
	{
		return LiquidID.IsNone() || Amount <= 0;
	}

	void Reset()
	{
		LiquidID = NAME_None;
		Amount = 0;
	}
};

UCLASS()
class WANTED_FACTORY_API APipe : public AActor
{
	GENERATED_BODY()

public:
	APipe();
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe|Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe|Components")
	TObjectPtr<UInstancedStaticMeshComponent> SegmentInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe|Components")
	TObjectPtr<UInstancedStaticMeshComponent> LiquidVisualInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe|Components")
	TObjectPtr<UTextRenderComponent> DebugStateText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Grid", meta = (ClampMin = "1.0"))
	float CellSize = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Visual")
	float ZOffset = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Visual", meta = (ClampMin = "0.01"))
	float SegmentRadiusScale = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Visual", meta = (ClampMin = "0.01"))
	float LiquidVisualScaleRatio = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Visual")
	float LiquidVisualZOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Flow", meta = (ClampMin = "0.01"))
	float SecondsPerSegment = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Flow", meta = (ClampMin = "1"))
	int32 MaxUnitsPerSegment = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Flow")
	bool bAutoMoveLiquids = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Debug")
	bool bShowDebugStateText = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Debug")
	FVector DebugTextOffset = FVector(0.0f, 0.0f, 90.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Debug", meta = (ClampMin = "1.0"))
	float DebugTextWorldSize = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pipe|Path")
	TArray<FIntPoint> PathCells;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe|Path")
	TArray<FIntPoint> OccupiedGridCells;

	UPROPERTY(VisibleAnywhere, Category = "Pipe|Flow")
	TArray<FPipeLiquidSlot> LiquidSlots;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Pipe|Flow")
	TWeakObjectPtr<AMachineBase> SourceMachine;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Pipe|Flow")
	TWeakObjectPtr<AMachineBase> TargetMachine;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> ResourceTable;

	FTimerHandle LiquidMoveTimerHandle;

public:
	UFUNCTION(BlueprintCallable, Category = "Pipe|Path")
	void SetPath(const TArray<FIntPoint>& NewPathCells, float NewCellSize);

	UFUNCTION(BlueprintCallable, Category = "Pipe|Path")
	void ConfigureTransport(
		const TArray<FIntPoint>& NewOccupiedGridCells,
		AMachineBase* NewSourceMachine,
		AMachineBase* NewTargetMachine);

	UFUNCTION(BlueprintCallable, Category = "Pipe|Path")
	void ClearPath();

	UFUNCTION(BlueprintPure, Category = "Pipe|Path")
	TArray<FIntPoint> GetPathCells() const { return PathCells; }

	UFUNCTION(BlueprintPure, Category = "Pipe|Path")
	TArray<FIntPoint> GetOccupiedGridCells() const { return OccupiedGridCells; }

	UFUNCTION(BlueprintPure, Category = "Pipe|Flow")
	AMachineBase* GetSourceMachine() const { return SourceMachine.Get(); }

	UFUNCTION(BlueprintPure, Category = "Pipe|Flow")
	AMachineBase* GetTargetMachine() const { return TargetMachine.Get(); }

	UFUNCTION(BlueprintPure, Category = "Pipe|Flow")
	bool IsOutputBlocked() const;

	UFUNCTION(BlueprintCallable, Category = "Pipe|Debug")
	void UpdateDebugStateText();

private:
	void RebuildVisuals();
	void ResetLiquidSlots();
	void RestartLiquidMoveTimer();
	void StopLiquidMoveTimer();
	void MoveLiquidsOneSegment();
	void RefreshLiquidVisualInstances();
	void UpdateDebugTextFacingPlayer();
	bool TryPullLiquidFromSource(FPipeLiquidSlot& OutSlot);
	bool IsLiquidItem(FName ItemID) const;
	FVector GetPathCentroidLocal() const;
	FVector GetCellLocalCenter(FIntPoint Cell) const;
};
