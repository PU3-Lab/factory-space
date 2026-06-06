// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Conveyor.generated.h"

class AMachineBase;
class UInstancedStaticMeshComponent;
class USceneComponent;
class UTextRenderComponent;

UCLASS()
class WANTED_FACTORY_API AConveyor : public AActor
{
	GENERATED_BODY()

public:
	AConveyor();
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Conveyor|Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Conveyor|Components")
	TObjectPtr<UInstancedStaticMeshComponent> StraightSegmentInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Conveyor|Components")
	TObjectPtr<UInstancedStaticMeshComponent> CornerSegmentInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Conveyor|Components")
	TObjectPtr<UInstancedStaticMeshComponent> ItemVisualInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Conveyor|Components")
	TObjectPtr<UTextRenderComponent> DebugStateText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Grid", meta = (ClampMin = "1.0"))
	float CellSize = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Visual", meta = (ClampMin = "0.0"))
	float SegmentWidthRatio = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Visual", meta = (ClampMin = "0.0"))
	float SegmentHeight = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Visual")
	float ZOffset = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Items|Visual", meta = (ClampMin = "0.01"))
	float ItemVisualScaleRatio = 0.28f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Items|Visual")
	float ItemVisualZOffset = 28.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Items|Visual", meta = (ClampMin = "0.0"))
	float ItemVisualLerpExponent = 1.0f;

	// 코너 ㄱ메시 캐논 방향 정렬 오프셋(0/90/180/270 중 PIE/BP에서 튜닝). ㄱ메시 기준 자세가 180° 돌아가 있어 기본 180.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Corner")
	float CornerBaseYaw = 180.0f;

	// 코너 ㄱ메시 균일 스케일 배율(메시 네이티브 크기 보정용, 1.0 기준).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Corner", meta = (ClampMin = "0.01"))
	float CornerScaleMultiplier = 1.0f;

	// 직선 메시 균일 스케일 배율(메시 네이티브 크기 보정용, 1.0 기준).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Straight", meta = (ClampMin = "0.01"))
	float StraightScaleMultiplier = 1.0f;

	// 직선 메시 캐논 방향 정렬 오프셋(0/90/180/270 중 PIE/BP에서 튜닝). PIE 확정 90°.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Straight")
	float StraightBaseYaw = 90.0f;

	// 세워진 직선 메시 눕히기(XZ벽 → XY바닥). PIE에서 조정 가능하게 노출.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Straight")
	float StraightRoll = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Conveyor|Path")
	TArray<FIntPoint> PathCells;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Conveyor|Path")
	TArray<FIntPoint> OccupiedGridCells;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Conveyor|Items")
	TArray<FName> ItemSlots;

	UPROPERTY(Transient)
	TArray<FName> PreviousItemSlots;

	UPROPERTY(Transient)
	TArray<int32> ItemVisualIds;

	UPROPERTY(Transient)
	TArray<int32> PreviousItemVisualIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Items", meta = (ClampMin = "0.01"))
	float SecondsPerGrid = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Items")
	bool bAutoMoveItems = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Debug")
	bool bShowDebugStateText = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Debug")
	FVector DebugTextOffset = FVector(0.0f, 0.0f, 90.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Debug", meta = (ClampMin = "1.0"))
	float DebugTextWorldSize = 24.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Conveyor|Items")
	TWeakObjectPtr<AMachineBase> SourceMachine;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Conveyor|Items")
	TWeakObjectPtr<AMachineBase> TargetMachine;

	FTimerHandle ItemMoveTimerHandle;
	float LastItemMoveWorldTime = 0.0f;
	int32 NextItemVisualId = 1;

public:
	UFUNCTION(BlueprintCallable, Category = "Conveyor|Path")
	void SetPath(const TArray<FIntPoint>& NewPathCells, float NewCellSize);

	UFUNCTION(BlueprintCallable, Category = "Conveyor|Path")
	void ConfigureTransport(
		const TArray<FIntPoint>& NewOccupiedGridCells,
		AMachineBase* NewSourceMachine,
		AMachineBase* NewTargetMachine);

	UFUNCTION(BlueprintCallable, Category = "Conveyor|Path")
	void ClearPath();

	UFUNCTION(BlueprintPure, Category = "Conveyor|Path")
	TArray<FIntPoint> GetPathCells() const { return PathCells; }

	UFUNCTION(BlueprintPure, Category = "Conveyor|Path")
	TArray<FIntPoint> GetOccupiedGridCells() const { return OccupiedGridCells; }

	UFUNCTION(BlueprintPure, Category = "Conveyor|Path")
	int32 GetOccupiedGridCount() const { return OccupiedGridCells.Num(); }

	UFUNCTION(BlueprintPure, Category = "Conveyor|Items")
	float GetTravelTimePerItem() const { return OccupiedGridCells.Num() * SecondsPerGrid; }

	UFUNCTION(BlueprintPure, Category = "Conveyor|Items")
	bool IsOutputBlocked() const;

	UFUNCTION(BlueprintCallable, Category = "Conveyor|Debug")
	void UpdateDebugStateText();

	UFUNCTION(BlueprintPure, Category = "Conveyor|Grid")
	float GetCellSize() const { return CellSize; }

	// PathCells 셀 중심들의 X·Y 평균(로컬, 그리드원점 기준). Z=0. PathCells 비면 ZeroVector.
	UFUNCTION(BlueprintPure, Category = "Conveyor|Path")
	FVector GetPathCentroidLocal() const;

	UFUNCTION(BlueprintPure, Category = "Conveyor|Items")
	AMachineBase* GetSourceMachine() const { return SourceMachine.Get(); }

	UFUNCTION(BlueprintPure, Category = "Conveyor|Items")
	AMachineBase* GetTargetMachine() const { return TargetMachine.Get(); }

private:
	void RebuildVisuals();
	void ResetItemSlots();
	void RestartItemMoveTimer();
	void StopItemMoveTimer();
	void MoveItemsOneGrid();
	void RefreshItemVisualInstances();
	float GetCurrentMoveAlpha() const;
	FVector GetCellLocalCenter(FIntPoint Cell) const;
	FVector GetSlotLocalCenter(int32 SlotIndex) const;
	FVector GetIncomingItemLocalCenter() const;
	FVector ResolveItemVisualStartLocation(int32 SlotIndex) const;
	int32 FindPreviousVisualSlotIndex(int32 VisualId) const;
	bool HasVisibleItems() const;
	FVector GetDebugTextLocalLocation() const;
	FString BuildMovingItemSummary() const;
};
