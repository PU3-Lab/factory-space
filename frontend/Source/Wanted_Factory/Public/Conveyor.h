// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Conveyor.generated.h"

class AMachineBase;
class UDataTable;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
class USceneComponent;
class UTextRenderComponent;
struct FResourceData;

struct FConveyorDepartingVisual
{
	int32 VisualId = INDEX_NONE;
	FName ItemId = NAME_None;
	FVector StartLocation = FVector::ZeroVector;
	FVector EndLocation = FVector::ZeroVector;
	float StartWorldTime = 0.0f;
	float Duration = 0.0f;
};

struct FConveyorVisualInstanceState
{
	TWeakObjectPtr<UStaticMeshComponent> Component;
	FName ItemId = NAME_None;
};

struct FConveyorDesiredVisual
{
	int32 VisualId = INDEX_NONE;
	FName ItemId = NAME_None;
	FTransform Transform = FTransform::Identity;
};

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
	TObjectPtr<UInstancedStaticMeshComponent> FlowArrowInstances;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Visual")
	bool bShowFlowArrows = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Visual", meta = (ClampMin = "0.01"))
	float FlowArrowScale = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Visual", meta = (ClampMin = "1.0"))
	float BuildModeFlowArrowScaleMultiplier = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Visual", meta = (ClampMin = "0.0"))
	float FlowArrowHeightOffset = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Visual", meta = (ClampMin = "1"))
	int32 FlowArrowSpacing = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Visual", meta = (ClampMin = "0.01"))
	float FlowArrowStepInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Visual")
	FLinearColor IdleFlowArrowColor = FLinearColor(0.65f, 0.65f, 0.65f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Visual")
	FLinearColor WorkingFlowArrowColor = FLinearColor(0.1f, 1.0f, 0.2f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Visual")
	FLinearColor NoPowerFlowArrowColor = FLinearColor(1.0f, 0.75f, 0.1f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Visual")
	FLinearColor BlockedFlowArrowColor = FLinearColor(1.0f, 0.15f, 0.1f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Visual")
	FLinearColor DisabledFlowArrowColor = FLinearColor(0.2f, 0.2f, 0.2f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Visual", meta = (ClampMin = "0.0"))
	float FlowArrowEmissiveStrength = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Items|Visual", meta = (ClampMin = "0.01"))
	float ItemVisualScaleRatio = 0.28f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Items|Visual")
	float ItemVisualZOffset = 28.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Items|Visual", meta = (ClampMin = "0.0"))
	float ItemVisualLerpExponent = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Items|Visual", meta = (ClampMin = "0.01"))
	float PowderVisualWidthMultiplier = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Items|Visual", meta = (ClampMin = "0.01"))
	float PowderVisualHeightMultiplier = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Items|Visual")
	float PowderVisualZOffsetAdjustment = -10.0f;

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

	// [OJJ F3.7-0, F3.8''' 노드화] 경사 경로 **셀 경계 노드** 로컬 Z(크기 = PathCells.Num()+1 —
	// 노드 i = 셀 i의 진입 경계, 마지막 노드 = 마지막 셀의 진출 경계. 액터 Z 기준 델타, 그리드가
	// OJJ_SetPathNodeLocalZs로 주입). 셀 중심 단위였던 이전 주입은 면의 꺾임점(항상 셀 경계 —
	// 램프 풋프린트 변이 셀 정렬)을 세그먼트가 현으로 가로질러 전환부가 ±행간/4 부유 — 경계
	// 샘플이면 세그먼트 체인이 면과 전 구간 정확 일치(분할 불필요, 세그먼트 수 불변).
	// **빈 배열 = 평면(기존 전 경로)** — 소비 수식이 전부 +0/pitch 0/스케일 ×1 항등(㊉).
	// Transient: 배치 시 주입되는 런타임 상태 — 직렬화 의도 없음(Codex F3.7-0 ⑤).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Conveyor|Path")
	TArray<float> PathNodeLocalZs;

	// [OJJ F3.9] 포트 꺾임 흐름 방향(그리드가 OJJ_SetPortFlowDirections로 주입). Start = 머신에서
	// 첫 셀로 들어오는 방향(소스 출력 포트 바깥 = BackStep), End = 마지막 셀에서 머신으로 나가는
	// 방향(타깃 입력 −FrontStep). **Zero = 미주입(기존 전 경로) — 끝 세그먼트 판정 완전 기존 동작.**
	// 경로 밖(머신 안) 꺾임을 코너 판정에 보충해, 옆 접근 시 끝 세그먼트가 직선 대신 코너가 된다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Conveyor|Path")
	FIntPoint OJJ_StartPortFlowDir = FIntPoint::ZeroValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Conveyor|Path")
	FIntPoint OJJ_EndPortFlowDir = FIntPoint::ZeroValue;

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
	bool bShowDebugStateText = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Debug")
	FVector DebugTextOffset = FVector(0.0f, 0.0f, 90.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor|Debug", meta = (ClampMin = "1.0"))
	float DebugTextWorldSize = 24.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Conveyor|Items")
	TWeakObjectPtr<AMachineBase> SourceMachine;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Conveyor|Items")
	TWeakObjectPtr<AMachineBase> TargetMachine;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> ResourceTable;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FlowArrowMaterialInstance;

	UPROPERTY(Transient)
	int32 LastFlowArrowPhase = INDEX_NONE;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> PowderVisualMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> PowderVisualMaterialBase;

	FTimerHandle ItemMoveTimerHandle;
	float LastItemMoveWorldTime = 0.0f;
	int32 NextItemVisualId = 1;
	TArray<FConveyorDepartingVisual> DepartingVisuals;
	TMap<int32, FConveyorVisualInstanceState> VisualInstanceStates;

public:
	UFUNCTION(BlueprintCallable, Category = "Conveyor|Path")
	void SetPath(const TArray<FIntPoint>& NewPathCells, float NewCellSize);

	// [OJJ F3.7-0, F3.8''' 노드화] 경사 경로 셀 경계 노드 Z 주입(OJJ_ 접두 = 그리드 측 침습 경계
	// 표식 — 벨트 자체 로직은 이 값을 생산하지 않음). SetPath 이후 호출 계약(SetPath가 배열을
	// 리셋 — stale Z 방어). 크기가 PathCells.Num()+1이 아니면 무시 + 경고(평면 유지). 빈 배열 = 평면 복귀.
	UFUNCTION(BlueprintCallable, Category = "Conveyor|Path")
	void OJJ_SetPathNodeLocalZs(const TArray<float>& NewNodeLocalZs);

	// [OJJ F3.9] 포트 꺾임 흐름 방향 주입(SetPath 이후 호출 계약 — SetPath가 리셋). Zero = 보충 없음.
	UFUNCTION(BlueprintCallable, Category = "Conveyor|Path")
	void OJJ_SetPortFlowDirections(FIntPoint StartFlowDir, FIntPoint EndFlowDir);

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

	void UpdateDebugTextFacingPlayer();

	UFUNCTION(BlueprintPure, Category = "Conveyor|Grid")
	float GetCellSize() const { return CellSize; }

	// PathCells 셀 중심들의 X·Y 평균(로컬, 그리드원점 기준). Z=0. PathCells 비면 ZeroVector.
	UFUNCTION(BlueprintPure, Category = "Conveyor|Path")
	FVector GetPathCentroidLocal() const;

	UFUNCTION(BlueprintPure, Category = "Conveyor|Items")
	AMachineBase* GetSourceMachine() const { return SourceMachine.Get(); }

	UFUNCTION(BlueprintPure, Category = "Conveyor|Items")
	AMachineBase* GetTargetMachine() const { return TargetMachine.Get(); }

	const TArray<FName>& GetItemSlotsForSave() const { return ItemSlots; }
	void ApplyItemSlotsForSave(const TArray<FName>& SavedItemSlots);
	bool RefundItemsToWarehouse();

private:
	void RebuildVisuals();
	void RefreshFlowArrowInstances();
	void ResetItemSlots();
	void RestartItemMoveTimer();
	void StopItemMoveTimer();
	void MoveItemsOneGrid();
	bool IsSolidItem(FName ItemID) const;
	void RefreshItemVisualInstances();
	void ClearAllItemVisualInstances();
	void SyncDesiredVisual(const FConveyorDesiredVisual& DesiredVisual);
	void RemoveVisualInstance(int32 VisualId);
	void PruneDepartingVisuals();
	void StartDepartingVisual(int32 VisualId, FName ItemId, int32 SlotIndex);
	UStaticMeshComponent* CreateVisualComponentForItem(int32 VisualId, FName ItemId);
	const FResourceData* FindResourceData(FName ItemId) const;
	bool IsPowderItem(FName ItemId) const;
	UStaticMesh* ResolveItemStaticMesh(FName ItemId) const;
	FTransform BuildItemVisualTransform(FName ItemId, const FVector& ItemLocation, const FRotator& ItemRotation, float BaseItemScale) const;
	FRotator ResolveItemVisualRotation(int32 SlotIndex, const FVector& StartLocation, const FVector& EndLocation, float MoveAlpha) const;
	FVector ResolveItemVisualTravelDirection(int32 SlotIndex, bool bUseIncomingSide) const;
	float GetCurrentMoveAlpha() const;
	FVector GetCellLocalCenter(FIntPoint Cell) const;
	// [OJJ F3.7-0, F3.8''' 노드화] 노드(셀 경계)/셀 중심 로컬 Z 조회 — 빈 배열/무효 인덱스는 0
	// (평면 항등). 셀 중심 = 양 경계 노드의 중점(셀 내 면이 선형이라 면의 중심값과 정확히 동일 —
	// 아이템 보간 무변경 수혜). ByCell은 경사 경로에서만 선형 탐색에 도달(평면은 즉시 0 — 비용 0).
	float OJJ_GetPathNodeLocalZ(int32 NodeIndex) const;
	float OJJ_GetPathCellLocalZByIndex(int32 PathIndex) const;
	float OJJ_GetPathCellLocalZByCell(FIntPoint Cell) const;
	FVector GetSlotLocalCenter(int32 SlotIndex) const;
	FVector GetIncomingItemLocalCenter() const;
	FVector GetOutgoingItemLocalCenter() const;
	FVector ResolveItemVisualStartLocation(int32 SlotIndex) const;
	FVector GetPathCellLocalCenterByIndex(int32 PathIndex) const;
	FVector GetFlowArrowLocationAtPathPosition(float PathPosition) const;
	FVector GetFlowArrowDirectionAtPathPosition(float PathPosition) const;
	int32 FindPreviousVisualSlotIndex(int32 VisualId) const;
	bool HasVisibleItems() const;
	bool ShouldAnimateFlowArrows() const;
	float GetFlowArrowPathOffset() const;
	FVector GetDebugTextLocalLocation() const;
	FString BuildMovingItemSummary() const;
	int32 GetFlowArrowPhase() const;
	void UpdateFlowArrowMaterial();
};
