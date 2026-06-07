// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/EngineTypes.h"
#include "OJJ_Grid.generated.h"

class AMachineBase;
class AConveyor;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

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
 * data (OccupiedCells / OJJ_ActorToCells) is still keyed by lower-left Origin —
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

	// 그리드 한 칸의 월드 크기 (uu).
	// ★ 100에서 바꾸면 머신 메시 자동 스케일이 어긋난다 — AMachineBase::MeshFitCellWorld(=100)가
	//   이 값을 가정해 메시 바운즈를 footprint에 정규화하기 때문. 변경 시 양쪽을 함께 동기화할 것.
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

	// === 지형 높낮이 건설 제약 (정적 지형 — BeginPlay 1회 베이크) ===
	// BeginPlay에서 GridSize 전 셀 중심에서 ↓라인트레이스 → 지형 높이가 그리드 평면 Z와
	// BuildableHeightTolerance를 넘게 차이나면 UnbuildableCells에 마킹. CanPlaceMachine/컨베이어 경로가 게이트로 참조.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Terrain", meta = (ClampMin = "0.0"))
	float BuildableHeightTolerance = 50.0f;

	// 베이크 ↓트레이스 시작 높이(그리드 평면 Z 상대, uu). 예상 지형 최고점보다 높게.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Terrain", meta = (ClampMin = "1.0"))
	float BuildableTraceStartHeight = 1000.0f;

	// 베이크 ↓트레이스 깊이(시작점 아래로, uu). 예상 지형 최저점보다 깊게.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Terrain", meta = (ClampMin = "1.0"))
	float BuildableTraceDepth = 2000.0f;

	// 베이크 트레이스 채널(지형 메시가 Block하는 채널). 기본 Visibility(빌드모드 커서 트레이스와 동일).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Terrain")
	TEnumAsByte<ECollisionChannel> BuildableTraceChannel = ECC_Visibility;

	// [예약·미구현] 경사도 게이트 임계(도). hit.Normal 각도가 이 값 초과 시 불가 — 차후 구현(현재 무시).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Terrain", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float BuildableSlopeThresholdDeg = 90.0f;

	// 베이크 결과 — 건설 불가 셀만 희소 저장(가능 셀 미저장). 베이크 전이면 비어 전부 가능.
	UPROPERTY(Transient)
	TSet<FIntPoint> UnbuildableCells;

	// 불가 셀 상시 오버레이 ISM(빌드모드 진입 시 표시). InvalidHover와 동일 빨강 재사용, 수명 독립.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid|Terrain")
	TObjectPtr<UInstancedStaticMeshComponent> BlockedCellISM;

	// 디버그 토글(OJJ.Grid.ShowBlocked) — 빌드모드 밖에서도 오버레이 강제 표시.
	UPROPERTY(Transient)
	bool bForceShowBlocked = false;

	// 베이크 완료 여부 — "불가 0개"와 "아직 베이크 안 됨"을 구분(진단/콘솔 리포트용).
	UPROPERTY(Transient)
	bool bBuildableBaked = false;

	// 빌드모드 시각화 활성 상태 — bForceShowBlocked 토글이 빌드모드 오버레이를 끄지 않도록 참조.
	UPROPERTY(Transient)
	bool bVisualizationActive = false;

	// 배치 가능 셀 호버 표시 (녹색)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid|Hover")
	TObjectPtr<UInstancedStaticMeshComponent> ValidHoverISM;

	// 배치 불가 셀 호버 표시 (빨강)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid|Hover")
	TObjectPtr<UInstancedStaticMeshComponent> InvalidHoverISM;

	// === 포트 방향 화살표 (빌드모드 전용 시각화) ===
	// 입력=파랑 계열 / 출력=주황 계열. 배치 머신용(Placed*)과 호버 프리뷰용(Hover*)을 분리해
	// 수명주기를 독립시킨다: Placed는 진입~퇴장 상시(커서 무관), Hover는 커서 프리뷰와 동반 생멸.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid|PortArrow")
	TObjectPtr<UInstancedStaticMeshComponent> PlacedInputArrowISM;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid|PortArrow")
	TObjectPtr<UInstancedStaticMeshComponent> PlacedOutputArrowISM;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid|PortArrow")
	TObjectPtr<UInstancedStaticMeshComponent> HoverInputArrowISM;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid|PortArrow")
	TObjectPtr<UInstancedStaticMeshComponent> HoverOutputArrowISM;

	// 배치 머신 화살표가 현재 표시 중인지(=빌드모드 활성) — RefreshPlacedMachineArrows에서 true,
	// ClearPlacedMachineArrows에서 false. 그리드는 BuildController의 bIsBuildMode를 모르므로,
	// 머신 제거(RemoveMachine) 시 이 플래그로 가드해 빌드모드 중일 때만 화살표를 재적재한다
	// (빌드모드 밖 제거가 화살표를 띄우는 회귀 방지).
	UPROPERTY(Transient)
	bool bPlacedArrowsVisible = false;

	// 포트 화살표 시각 튜닝 — 에디터/PIE에서 리컴파일 없이 조정. 콘 인스턴스 균일 스케일.
	// 디테일 패널/레벨 인스턴스에서 바로 만지도록 EditAnywhere + BlueprintReadWrite.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|PortArrow", meta = (ClampMin = "0.01"))
	float PortArrowScale = 0.5f;

	// 포트 화살표를 셀 평면 위로 띄우는 높이(uu) — 너무 낮으면 머신 메쉬에 가려짐.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|PortArrow", meta = (ClampMin = "0.0"))
	float PortArrowHeightOffset = 25.0f;

	// 화살표 틴트용 베이스/동적 머티리얼 (BeginPlay에서 베이스로부터 MID 생성).
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ArrowBaseMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> InputArrowMID;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OutputArrowMID;

	// 점유된 셀 → 점유 액터 (좌표로 조회). 머신/컨베이어 모두 수용하도록 AActor로 일반화.
	// 머신 조회는 GetMachineAtCell이 Cast<AMachineBase>로 좁힘.
	UPROPERTY(Transient)
	TMap<FIntPoint, TWeakObjectPtr<AActor>> OccupiedCells;

	// 액터 → 점유 셀 목록 (이미 배치 여부 판정, 제거 시 일괄 해제). AActor로 일반화(컨베이어 포함).
	TMap<TWeakObjectPtr<AActor>, TArray<FIntPoint>> OJJ_ActorToCells;

	// 액터 → 등록 시점 origin (lower-left). min-recompute 대신 명시 저장 →
	// 비직사각형/등록 후 이동·회전에도 origin 식별 안정. GetMachineOrigin이 이 맵을 조회.
	TMap<TWeakObjectPtr<AActor>, FIntPoint> OJJ_ActorToOrigin;

private:
	// Origin부터 머신 풋프린트가 차지하는 셀 좌표 목록. RotationSteps로 90° 회전 footprint 지원(기본 0).
	TArray<FIntPoint> CalculateFootprint(AMachineBase* Machine, FIntPoint Origin, int32 RotationSteps = 0) const;

	// 포트 셀 공유 헬퍼: footprint 셀 C 중 (C+Dir)이 footprint 밖이면 C는 Dir쪽 모서리 → 그 이웃(C+Dir)이 포트 셀.
	// 출력(GetMachineOutputCells)·입력(OJJ_GetMachineInputCells)이 방향만 바꿔 공유. Dir==(0,0)/무효 머신이면 빈 배열.
	// footprint 모양/회전 무관(EffectiveSize가 X/Y만 swap하므로 모서리 판정 동일).
	// PortCount로 대칭 배치 규칙 적용(GetMachineOutputCells/InputCells가 각 포트수를 전달).
	TArray<FIntPoint> OJJ_GetMachinePortCells(AMachineBase* Machine, FIntPoint Dir, int32 PortCount) const;

	// 추출 머신(채굴기/펌프/공압) 판정 — 입력이 자원 노드라 입력 화살표를 생략한다.
	// TODO(SSR 협의): MachineType 문자열 비교 대신 AMachineBase 가상 predicate로 대체.
	static bool OJJ_IsExtractionMachine(const AMachineBase* Machine);

	// 입력/출력 포트 셀 목록을 화살표 인스턴스로 ISM에 적재. 입력=−InputDir(머신 향함),
	// 출력=+OutputDir(나감). 콘 메시 apex(+Z)를 수평 방향에 정렬, 셀 중심에 배치.
	void OJJ_EmitPortArrows(
		UInstancedStaticMeshComponent* InputISM, bool bDrawInput, const TArray<FIntPoint>& InputCells, FIntPoint InputDir,
		UInstancedStaticMeshComponent* OutputISM, bool bDrawOutput, const TArray<FIntPoint>& OutputCells, FIntPoint OutputDir) const;

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

	// 셀이 그리드 유효 범위 ([0, GridSize.X) × [0, GridSize.Y)) 내인지 검사
	UFUNCTION(BlueprintPure, Category = "Grid|Coordinate")
	bool IsValidGridCell(FIntPoint Cell) const;

	// === 지형 높낮이 건설 제약 ===

	// 셀이 지형 높이상 건설 가능한지(=UnbuildableCells에 없음). 베이크 전이면 항상 true.
	UFUNCTION(BlueprintPure, Category = "Grid|Terrain")
	bool IsCellBuildable(FIntPoint Cell) const;

	// 지형 높이 베이크 — GridSize 전 셀 ↓트레이스로 UnbuildableCells 재계산. BeginPlay 1회 + 콘솔 재호출.
	UFUNCTION(BlueprintCallable, Category = "Grid|Terrain")
	void BakeBuildableCells();

	// 불가 셀 오버레이 갱신(클리어 후 재적재 — 중복/잔존 방지). bShow=false면 클리어만.
	void RefreshBlockedOverlay(bool bShow);

	// 디버그(OJJ.Grid.ShowBlocked) — 빌드모드와 무관하게 오버레이 강제 표시 토글.
	void SetForceShowBlocked(bool bShow);

	// === Grid Query (GridManager/컨베이어용 읽기 전용 조회) ===
	// OccupiedCells / OJJ_ActorToCells를 노출만 함 — write 경로/데이터는 건드리지 않음.

	// 셀에 등록된 머신 반환. 비점유/GC된 머신이면 nullptr.
	// const라 SweepStaleEntries는 못 부르지만 weak ptr Get()으로 stale을 nullptr 처리.
	UFUNCTION(BlueprintPure, Category = "Grid|Query")
	AMachineBase* GetMachineAtCell(FIntPoint Cell) const;

	// 셀에 등록된 임의 점유 액터 반환(머신·컨베이어·자원 등 무관). 비점유/GC 셀이면 nullptr.
	// GetMachineAtCell이 Cast<AMachineBase>로 좁히는 것과 달리, 비머신 점유 액터(AResourceBase 등)를
	// 그대로 얻기 위한 접근자. 배치 제약(채굴기 인접 광맥 탐색 등)에서 셀의 자원 노드를 찾는 데 사용.
	UFUNCTION(BlueprintPure, Category = "Grid|Query")
	AActor* GetActorAtCell(FIntPoint Cell) const;

	// AActor 점유 여부 (머신 존재와 무관 — 컨베이어 등 비머신 점유 셀도 true,
	// GetMachineAtCell은 그 셀에 null 반환). stale(파괴된) 액터 셀은 weak IsValid()로 false.
	UFUNCTION(BlueprintPure, Category = "Grid|Query")
	bool IsCellOccupied(FIntPoint Cell) const;

	// 머신 풋프린트의 lower-left(=등록 시 Origin). 미등록 머신이면 (INT_MIN, INT_MIN) 센티넬.
	// 풋프린트 셀은 Origin부터 비음수 offset이라 min(X),min(Y) == Origin (회전 무관 — EffectiveSize가 X/Y만 swap).
	UFUNCTION(BlueprintPure, Category = "Grid|Query")
	FIntPoint GetMachineOrigin(AMachineBase* Machine) const;

	// 머신 점유 셀 목록(footprint) 포인터. 미등록/무효(IsValid 실패) 머신이면 nullptr. C++ 전용(BP 비호환 반환형).
	// ⚠️ 수명: 반환 포인터는 OJJ_ActorToCells 내부를 가리킴 — 다음 grid 변경(TryPlace/Remove/stale sweep, rehash)
	//    시 무효화됨. 즉시(같은 프레임) 읽기 전용으로만 사용하고 절대 캐싱하지 말 것. 보관이 필요하면 값 복사.
	const TArray<FIntPoint>* GetMachineCells(AMachineBase* Machine) const;

	// [철거 모드] 임의 점유 액터(머신/컨베이어)의 등록 셀 목록. 미등록/무효면 nullptr.
	// GetMachineCells의 비머신 포함 버전 — 즉시 읽기 전용(동일 수명 주의: grid 변경 시 무효화).
	const TArray<FIntPoint>* GetActorCells(AActor* Actor) const;

	// [철거 모드] 주어진 셀들을 InvalidHover(빨강)로 하이라이트. 기존 호버 프리뷰는 먼저 비움.
	// 철거 대상 표시 전용 — 배치 호버(UpdateHoverPreview)와 동일 ISM/스케일/Z오프셋 규칙 재사용.
	void OJJ_HighlightCellsInvalid(const TArray<FIntPoint>& Cells);

	// === Grid Conveyor (출력포트 자급 판별 — ssr 포트 시스템 미변경) ===
	// 컨벤션: 출력 = 머신 뒤(-Front). 액터 transform(yaw) 기준이라 메시 art와 무관하게 일관.
	// 메시 art-front의 +X 시각 정합은 별도(리임포트) 작업 — 로직 정확성과 무관.

	// 벡터(XY)를 가장 우세한 단일 축의 카디널 grid offset((±1,0)/(0,±1))으로 스냅. 대각선 방지.
	// tie(|X|==|Y|, 예: 정확히 45°)는 결정적으로 X축 선택. 비유한/거의 0인 입력은 (0,0) 반환.
	// Codex 검증: 90° 배수 정확, 임의 각도도 우세축 스냅으로 대각선 아티팩트 차단.
	UFUNCTION(BlueprintPure, Category = "Grid|Conveyor")
	static FIntPoint CardinalFromVector(FVector V);

	// footprint 셀 집합에서 Dir 쪽 모서리 이웃(포트 셀)을 산출 + PortCount 대칭 배치 규칙 적용.
	// 화살표·컨베이어 도킹 판정·머신 출력 타깃 그래프가 모두 이 함수를 경유 → 셀 집합 완전 일치(단일 진실원).
	//  - PortCount<=0 또는 >=면길이 → 전부 (현행 동일, 리그레션 0)
	//  - PortCount==1 → 면 중앙(홀수 면만), 짝수 면이면 대칭 불가
	//  - PortCount>=2 → 양끝 포함 중심축 대칭 균등 분산
	//  - 대칭 불가 조합(짝수면 1포트 등) → (면길이,포트수)당 경고 1회 + 전부 반환 폴백(크래시/임의 배치 금지)
	// 면 축(Dir 수직)으로 정렬 후 선택하므로 회전 시 footprint/forward가 함께 돌아 상대 위치 유지.
	static TArray<FIntPoint> OJJ_PortCellsFromFootprint(const TArray<FIntPoint>& Cells, FIntPoint Dir, int32 PortCount);

	// 머신 출력이 향하는 월드 grid 방향 (= -Front 카디널). 무효 머신이면 (0,0).
	UFUNCTION(BlueprintPure, Category = "Grid|Conveyor")
	FIntPoint GetMachineOutputDir(AMachineBase* Machine) const;

	// 머신이 아이템을 내보내는 타깃 셀 목록 = footprint의 OutputDir쪽 모서리 셀들의 +OutputDir 이웃.
	// footprint 모양/회전 무관. 무효/미등록이면 빈 배열. 타깃 셀은 off-grid/미점유일 수 있음(호출자 판단).
	UFUNCTION(BlueprintCallable, Category = "Grid|Conveyor")
	TArray<FIntPoint> GetMachineOutputCells(AMachineBase* Machine) const;

	// 출력 타깃 셀에 등록된 머신들 (유효만, self 제외, 중복 제거). 다운스트림 연결 후보.
	UFUNCTION(BlueprintCallable, Category = "Grid|Conveyor")
	TArray<AMachineBase*> GetMachineOutputTargets(AMachineBase* Machine) const;

	// 머신 입력이 향하는 월드 grid 방향 (= +Front 카디널). 출력(-Front)의 부호 반전.
	// 무효 머신이면 (0,0) (출력이 (0,0)이면 반전해도 (0,0)). 컨베이어 끝단이 머신 입력 포트에 닿는지 판정에 사용.
	UFUNCTION(BlueprintPure, Category = "Grid|Conveyor")
	FIntPoint OJJ_GetMachineInputDir(AMachineBase* Machine) const;

	// 머신이 아이템을 받는 입력 셀 목록 = footprint의 InputDir쪽 모서리 셀들의 +InputDir 이웃.
	// GetMachineOutputCells의 입력 대칭(같은 헬퍼 OJJ_GetMachinePortCells 공유). footprint 모양/회전 무관.
	// 무효/미등록이면 빈 배열. 입력 셀은 off-grid/미점유일 수 있음(호출자 판단).
	UFUNCTION(BlueprintCallable, Category = "Grid|Conveyor")
	TArray<FIntPoint> OJJ_GetMachineInputCells(AMachineBase* Machine) const;

	// === Conveyor 인지 (Step 3-a — 셀 등록/조회만, 경로·포트 유효성은 3-c) ===

	// 셀에 등록된 컨베이어 반환. 비컨베이어(머신)/비점유/GC 셀이면 nullptr.
	// GetMachineAtCell의 컨베이어판 — 같은 OccupiedCells를 Cast<AConveyor>로 좁힘.
	UFUNCTION(BlueprintPure, Category = "Grid|Conveyor")
	AConveyor* OJJ_GetConveyorAtCell(FIntPoint Cell) const;

	// 임의 actor(컨베이어)를 명시 셀 목록으로 등록 — OccupiedCells + OJJ_ActorToCells + OJJ_ActorToOrigin 동기.
	// 머신 등록(RegisterMachineInternal/footprint) 경로와 독립한 컨베이어 전용 등록.
	// 가드: 서버 권위, 유효 actor, 비어있지 않은 셀, 중복 등록 금지, 다른 actor 점유 셀 충돌 거부(데이터 무결성).
	// ※ 경로 연속성/포트 정합 등 placement 유효성은 3-c. 여기선 점유 충돌만.
	// ※ 지형 건설 게이트(IsCellBuildable)는 검사하지 않음 — 이 API는 "점유 데이터 등록" 전용.
	//   건설 제약은 호출자 책임(머신=CanPlaceMachine, 컨베이어=OJJ_CollectConveyorReservedCells에서 사전 차단).
	//   자원(광맥/Water)은 디자이너 사전배치라 의도적으로 지형 게이트 면제(점유로만 건설 차단).
	UFUNCTION(BlueprintCallable, Category = "Grid|Conveyor")
	bool OJJ_RegisterActorCells(AActor* Actor, const TArray<FIntPoint>& Cells);

	// 셀을 점유한 actor(컨베이어 포함)를 양방향 맵에서 제거. 머신이면 머신도 제거됨(범용).
	// 서버 권위 전용. 비점유/GC 셀이면 false.
	UFUNCTION(BlueprintCallable, Category = "Grid|Conveyor")
	bool OJJ_RemoveActorAt(FIntPoint Cell);

	// === Conveyor 경로/배치 (Step 3-b — Dummy 모델 A 이식, parity) ===

	// 드래그 셀 목록을 정규 컨베이어 경로로 보정 — 시작이 머신 출력 셀 위/인접인지 검사해 출력셀 포함 경로 생성.
	// 실패 시 OutReason에 사유. (포트 판정/연속성/충돌은 내부 OJJ_CollectConveyorReservedCells로 검증.) C++ 전용.
	bool OJJ_BuildConveyorPlacementPath(
		const TArray<FIntPoint>& DragCells,
		TArray<FIntPoint>& OutPathCells,
		FString& OutReason) const;

	// 주어진 경로가 배치 가능한지만 판정(예약셀 산출 없이 OJJ_CollectConveyorReservedCells 래핑). C++ 전용.
	bool OJJ_CanPlaceConveyorPath(const TArray<FIntPoint>& PathCells) const;

	// 컨베이어 배치 종합 — 경로 보정→예약셀/source/target 산출→OJJ_RegisterActorCells 등록→
	// Conveyor의 SetActorLocation/SetPath/ConfigureTransport(AMachineBase* 엔드포인트) 호출.
	// 실패 시 OutReason 기록 + false(등록 전 실패는 부작용 없음). 서버 권위 전용.
	UFUNCTION(BlueprintCallable, Category = "Grid|Conveyor")
	bool OJJ_TryPlaceConveyor(AConveyor* Conveyor, const TArray<FIntPoint>& PathCells, FString& OutReason);

	// 컨베이어 드래그 경로 호버 미리보기 — 경로 보정 성공이면 ValidHoverISM(녹색), 실패면 InvalidHoverISM(빨강)에
	// 셀별 인스턴스 표시. 호출 시 기존 미리보기 클리어. (Step 6 입력 드래그 UX용.)
	UFUNCTION(BlueprintCallable, Category = "Grid|Hover")
	void OJJ_UpdateConveyorPathHoverPreview(const TArray<FIntPoint>& PathCells);

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

	// 호버 미리보기 모두 제거 (머신 placement 완료 / 호버 해제 시 호출). 호버 포트 화살표도 함께 제거.
	UFUNCTION(BlueprintCallable, Category = "Grid|Hover")
	void ClearHoverPreview();

	// === 포트 방향 화살표 (빌드모드 전용) ===

	// 배치된 모든 머신의 입출력 포트 화살표 갱신(클리어 후 재적재). 빌드모드 진입 / 배치·제거 후 호출.
	UFUNCTION(BlueprintCallable, Category = "Grid|PortArrow")
	void RefreshPlacedMachineArrows();

	// 호버 프리뷰 머신(CDO, 미스폰)의 포트 화살표를 Origin/회전 step으로부터 산출해 표시.
	// 액터가 없으므로 forward는 배치 컨벤션(yaw=90*step)으로 재구성. C++ 전용(BP 비호환 인자).
	void DrawHoverMachineArrows(AMachineBase* Machine, FIntPoint Origin, int32 RotationSteps);

	// 배치 머신 화살표 제거 (빌드모드 퇴장 시).
	UFUNCTION(BlueprintCallable, Category = "Grid|PortArrow")
	void ClearPlacedMachineArrows();

	// 호버 프리뷰 화살표 제거 (ClearHoverPreview에서 호출 — 호버 셀 ISM과 동반 생멸).
	UFUNCTION(BlueprintCallable, Category = "Grid|PortArrow")
	void ClearHoverMachineArrows();
};
