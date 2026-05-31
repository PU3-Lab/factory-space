// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OJJ_Grid.generated.h"

class AMachineBase;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;

/**
 * AOJJ_Grid is the source of truth for grid occupancy.
 * Machines not registered via TryPlaceMachine or RegisterExistingMachine
 * are invisible to this grid. CanPlaceMachine may return true for cells
 * physically occupied by unregistered machines.
 *
 * KNOWN LIMITATION: No automatic registration of pre-placed machines
 * AOJJ_Grid does NOT auto-scan the world for existing AMachineBase actors.
 * Reasons:
 * - Multi-grid scenarios cause cross-grid occupancy contamination (every
 *   grid would register every machine into its own coordinate space)
 * - Grid ownership/bounds contract not yet defined with team
 *
 * Pre-placed machines must be registered explicitly via
 * RegisterExistingMachine() with the correct lower-left grid coordinate.
 *
 * Multi-cell machine anchor (resolved): AMachineBase mesh stays center-anchored
 * (agreed with machine team). The grid compensates at placement time by moving
 * the actor to the footprint center via GetMachinePlacementLocation. Occupancy
 * data (OccupiedCells / MachineToCells) is still keyed by lower-left Origin —
 * only the visual transform is offset. 1x1 case yields zero offset (no regression).
 *
 * To be revisited when team contracts are agreed:
 * - Grid ownership: AOJJ_Grid bounds (GridSizeX/Y) or AMachineBase OwningGrid
 */
UCLASS()
class WANTED_FACTORY_API AOJJ_Grid : public AActor
{
	GENERATED_BODY()

public:
	AOJJ_Grid();

protected:
	virtual void BeginPlay() override;

	// 그리드 한 칸의 월드 크기 (uu)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Settings", meta = (ClampMin = "1.0"))
	float CellSize;

	// 실제 placement 가능 영역 (X 칸 × Y 칸).
	// CanPlaceMachine / IsValidGridCell이 권위 있는 grid extent로 사용. 머신은 이 범위 내에서만 등록 가능.
	// VisualizationRange (시각화 한 변당 셀 수) 와 독립 — 디자이너가 두 값을 분리 가능.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Settings", meta = (ClampMin = "1"))
	FIntPoint GridSize = FIntPoint(20, 20);

	// 그리드 시각화용 바닥 평면 메시 (건설 모드 진입 시 표시)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid|Visualization")
	TObjectPtr<UStaticMeshComponent> GridFloorMesh;

	// 시각화 floor mesh와 호버 평면의 한 변당 셀 수. 렌더링 한정 — placement 검증에는 사용하지 않음.
	// 기본값으로 GridSize 한 변과 동일하게 시작하지만 분리 가능.
	// 예: GridSize=(20,20), VisualizationRange=30 → 30칸 floor 위에 20×20 placement 영역만 유효.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Visualization", meta = (ClampMin = "1"))
	int32 VisualizationRange;

	// 배치 가능 셀 호버 표시 (녹색)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid|Hover")
	TObjectPtr<UInstancedStaticMeshComponent> ValidHoverISM;

	// 배치 불가 셀 호버 표시 (빨강)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid|Hover")
	TObjectPtr<UInstancedStaticMeshComponent> InvalidHoverISM;

	// 점유된 셀 → 머신 (좌표로 머신 조회)
	UPROPERTY(Transient)
	TMap<FIntPoint, TWeakObjectPtr<AMachineBase>> OccupiedCells;

	// 머신 → 점유 셀 목록 (이미 배치 여부 판정, 제거 시 일괄 해제)
	TMap<TWeakObjectPtr<AMachineBase>, TArray<FIntPoint>> MachineToCells;

private:
	// Origin부터 머신 풋프린트가 차지하는 셀 좌표 목록. RotationSteps로 90° 회전 footprint 지원(기본 0).
	TArray<FIntPoint> CalculateFootprint(AMachineBase* Machine, FIntPoint Origin, int32 RotationSteps = 0) const;

	// GC/Destroy된 머신 엔트리를 양방향 맵에서 정리. write 경로 진입부에서 호출.
	void SweepStaleEntries();

	// 양방향 맵에 머신 등록. 위치 갱신은 호출자가 별도 처리. 모든 write 검증을 포함.
	// RotationSteps는 점유 footprint 계산(CanPlace/CalculateFootprint)에 전달(기본 0).
	bool RegisterMachineInternal(AMachineBase* Machine, FIntPoint Origin, FString& OutReason, int32 RotationSteps = 0);

public:
	virtual void Tick(float DeltaTime) override;

	// 월드 좌표 → 그리드 좌표 (X-Y 평면, Z 무시)
	UFUNCTION(BlueprintPure, Category = "Grid|Coordinate")
	FIntPoint WorldToGrid(FVector WorldPos) const;

	// 그리드 좌표 → 월드 좌표 (셀 중심 반환)
	UFUNCTION(BlueprintPure, Category = "Grid|Coordinate")
	FVector GridToWorld(FIntPoint Coord) const;

	// placement 범위(GridSize)의 중심 월드 좌표. 원점은 좌하단(액터 위치)이므로
	// ActorLoc + GridSize*CellSize/2. 빌드 카메라 자동 센터링 등에 사용.
	// Z는 그리드 평면(액터 Z) 반환. 그리드가 동적으로 커지면 호출 시점 GridSize를 반영.
	UFUNCTION(BlueprintPure, Category = "Grid|Coordinate")
	FVector GetGridCenter() const;

	// 머신 raw 치수(GetMachineSize)를 정수화(CeilToInt+Max 1)하고 90° 회전 step을 적용한
	// 유효 footprint 치수. step 짝수(0,2)→(X,Y), 홀수(1,3)→(Y,X). footprint/호버/배치/시각 보정이
	// 이 함수 하나로 회전·정수화 규칙을 공유 → 경로 간 어긋남 방지. step 0이면 기존 정수화와 동일.
	UFUNCTION(BlueprintPure, Category = "Grid|Coordinate")
	static FIntPoint EffectiveSize(FVector2D RawSize, int32 RotationSteps);

	// 머신 mesh는 center anchor (머신 팀과 합의된 contract). 그리드 lower-left 좌표계와
	// 정렬을 맞추기 위해 풋프린트 전체 center에 머신 액터 중심을 배치한다.
	// 1x1은 offset (0,0) → GridToWorld(Origin)과 동일하므로 회귀 없음.
	// 양쪽 placement 경로 공통 reference:
	//  - TryPlaceMachine: 이 값으로 spawn 액터 위치를 보정
	//  - RegisterExistingMachine: 이 값과 사전 배치 액터 위치(XY) 일치 검증
	UFUNCTION(BlueprintPure, Category = "Grid|Coordinate")
	FVector GetMachinePlacementLocation(AMachineBase* Machine, FIntPoint Origin, int32 RotationSteps = 0) const;

	// 셀이 그리드 유효 범위 ([0, VisualizationRange) × [0, VisualizationRange)) 내인지 검사
	UFUNCTION(BlueprintPure, Category = "Grid|Coordinate")
	bool IsValidGridCell(FIntPoint Cell) const;

	// Origin부터 머신 풋프린트만큼의 셀이 모두 비어있는지 검사. RotationSteps로 회전 footprint 검사(기본 0).
	UFUNCTION(BlueprintPure, Category = "Grid|Placement")
	bool CanPlaceMachine(AMachineBase* Machine, FIntPoint Origin, int32 RotationSteps = 0) const;

	// Origin에 머신 배치 시도. 실패 시 OutReason에 사유 기록 (서버 권위 전용). RotationSteps로 회전 배치(기본 0).
	UFUNCTION(BlueprintCallable, Category = "Grid|Placement")
	bool TryPlaceMachine(AMachineBase* Machine, FIntPoint Origin, FString& OutReason, int32 RotationSteps = 0);

	// 머신 인스턴스를 그리드에서 제거 (서버 권위 전용)
	UFUNCTION(BlueprintCallable, Category = "Grid|Placement")
	bool RemoveMachine(AMachineBase* Machine);

	// 좌표로 점유 머신을 찾아 제거 (내부적으로 RemoveMachine 호출, 서버 권위 전용)
	UFUNCTION(BlueprintCallable, Category = "Grid|Placement")
	bool RemoveMachineAt(FIntPoint Coord);

	// 사전 배치된 머신을 그리드에 등록 (lower-left cell 기준, 위치 변경 없음).
	// AOJJ_Grid는 자동 스캔하지 않으므로 사전 등록 경로는 이 함수가 유일.
	// 호출자 책임:
	//  - 올바른 그리드 좌표 (머신 풋프린트의 lower-left cell) 제공
	//  - 머신 액터의 XY 위치가 GetMachinePlacementLocation(Machine, Origin)과 일치 (Tolerance 내).
	//    어긋나면 데이터/시각 invariant 위반 → ensure + UE_LOG(Error) + OutReason + return false.
	//    디자이너 의도를 존중해 코드가 snap하지 않고 검증으로 처리한다.
	//  - 머신이 이 그리드 인스턴스에 속한다는 보장
	//  - 서버 (HasAuthority) 에서만 호출
	UFUNCTION(BlueprintCallable, Category = "Grid|Placement")
	bool RegisterExistingMachine(AMachineBase* Machine, FIntPoint Origin, FString& OutReason);

	// 그리드 시각화 평면 표시/숨김 (건설 모드 토글 등에 사용)
	UFUNCTION(BlueprintCallable, Category = "Grid|Visualization")
	void SetVisualizationVisible(bool bVisible);

	// 호버 라인 트레이스의 hit target 식별용 접근자 (cursor hit 컴포넌트와 비교).
	UFUNCTION(BlueprintPure, Category = "Grid|Visualization")
	UStaticMeshComponent* GetGridFloorMesh() const { return GridFloorMesh; }

	// Origin에 머신을 호버 시 셀별 가능/불가 미리보기 갱신 (호출 시 기존 미리보기 클리어). RotationSteps로 회전 미리보기(기본 0).
	UFUNCTION(BlueprintCallable, Category = "Grid|Hover")
	void UpdateHoverPreview(AMachineBase* Machine, FIntPoint Origin, int32 RotationSteps = 0);

	// 호버 미리보기 모두 제거 (머신 placement 완료 / 호버 해제 시 호출).
	UFUNCTION(BlueprintCallable, Category = "Grid|Hover")
	void ClearHoverPreview();
};
