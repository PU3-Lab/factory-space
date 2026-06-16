#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Pipe.generated.h"

class AMachineBase;
class UDataTable;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
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

	// ㄱ자 코너 이음새 마감용 조인트 구(반경 = PipeRadius). 방향 전환 노드마다 1개.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe|Components")
	TObjectPtr<UInstancedStaticMeshComponent> JoinInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe|Components")
	TObjectPtr<UInstancedStaticMeshComponent> LiquidVisualInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe|Components")
	TObjectPtr<UTextRenderComponent> DebugStateText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Grid", meta = (ClampMin = "1.0"))
	float CellSize = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Visual")
	float ZOffset = 50.0f;

	// 파이프 반경(월드 uu). 지름 = 2×PipeRadius (기본 60 = PIE 실측 확정). 메시 실측 치수로 환산하므로
	// 엔진 기본 실린더/구의 절대 크기와 무관 — 메시를 바꿔도 반경이 유지됨. 직선/코너/라이저 전부 추종.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Visual", meta = (ClampMin = "1.0"))
	float PipeRadius = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Visual", meta = (ClampMin = "0.01"))
	float LiquidVisualScaleRatio = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Visual")
	float LiquidVisualZOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Material")
	TObjectPtr<UMaterialInterface> PipeMaterialBase;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Material")
	TObjectPtr<UMaterialInterface> EmptyPipeMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Material")
	TObjectPtr<UMaterialInterface> FilledPipeMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Material")
	FLinearColor EmptyPipeColor = FLinearColor(0.85f, 0.95f, 1.0f, 0.12f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Material")
	FLinearColor FilledPipeColor = FLinearColor(0.2f, 0.7f, 1.0f, 0.45f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Material", meta = (ClampMin = "0.0"))
	float FilledPipeEmissiveStrength = 0.0f;

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

	// F4-3 오버패스 — 셀별 상승 lift 프로파일(ZOffset 위 델타, PathCells와 1:1). 비거나 길이 불일치면 0(평면 항등).
	// 다리 셀(교차 ∪ 양옆)은 lift=H. RebuildVisuals가 이 프로파일에서 0↔H 경계마다 수직 라이저(같은 XY에
	// base/top 2노드)를 즉석 삽입해 ㄷ자 다리를 구성하고, 액체 블롭도 셀 lift만큼 따라 오른다.
	// 그리드가 OJJ_SetPathCellLocalZs로 주입(빈 배열 = 기존 평면 동작 그대로).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe|Path")
	TArray<float> PathCellZs;

	// #257 탱크 진입 포트 방향(그리드가 OJJ_SetEndPortFlowDir로 주입, entry→tank로 부호 결정된 그리드 스텝).
	// RebuildVisuals가 마지막 셀 중심을 bend 조인트로 두고 이 방향으로 half-cell 스텁을 꺾어 "물려 들어가는" 연결을 만든다.
	// 시각 전용 — 경로 판정/예약셀/액체 슬롯과 무관. Zero면 기존 직선 돌출(항등).
	FIntPoint OJJ_EndPortFlowDir = FIntPoint::ZeroValue;

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

	// F4-3 오버패스 비주얼 주입 — 노드별 lift(ZOffset 위 델타, PathCells와 1:1). SetPath 이후 호출.
	// 길이가 PathCells와 안 맞으면 무시(평면 fallback) — 빈 배열이면 기존 평면 동작 그대로(항등).
	UFUNCTION(BlueprintCallable, Category = "Pipe|Path")
	void OJJ_SetPathCellLocalZs(const TArray<float>& InCellLifts);

	// #257 탱크 진입 포트 방향 주입(시각 전용). SetPath 전에 호출 — RebuildVisuals가 마지막 스텁을 이 방향으로 꺾는다.
	void OJJ_SetEndPortFlowDir(FIntPoint InEndPortFlowDir) { OJJ_EndPortFlowDir = InEndPortFlowDir; }

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
	void UpdateMaterialState();
	bool TryPullLiquidFromSource(FPipeLiquidSlot& OutSlot);
	bool IsLiquidItem(FName ItemID) const;
	bool HasAnyLiquid() const;
	FName GetPrimaryLiquidID() const;
	FLinearColor GetFilledPipeColor() const;
	UMaterialInterface* GetPipeMaterial(bool bHasLiquid);
	void ConfigureMaterialInstance(UMaterialInstanceDynamic* MaterialInstance, bool bHasLiquid) const;
	FVector GetPathCentroidLocal() const;
	FVector GetCellLocalCenter(FIntPoint Cell) const;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> EmptyPipeMaterialInstance;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FilledPipeMaterialInstance;
};
