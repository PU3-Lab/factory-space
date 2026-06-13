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
	float ZOffset = 20.0f;

	// 파이프 반경(월드 uu). 지름 = 2×PipeRadius (기본 80). 메시 실측 치수로 환산하므로
	// 엔진 기본 실린더/구의 절대 크기와 무관 — 메시를 바꿔도 반경이 유지됨.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipe|Visual", meta = (ClampMin = "1.0"))
	float PipeRadius = 40.0f;

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

	// F4-3 오버패스/경사 대비 — 노드별 절대 SurfaceZ(PathCells와 1:1, 로컬 Z). 비면 ZOffset 균일.
	// FindBetweenNormals가 3D라 양끝 Z가 다르면 경사 실린더가 공짜로 생성됨(렌더는 이미 준비됨).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe|Path")
	TArray<float> PathCellZs;

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
