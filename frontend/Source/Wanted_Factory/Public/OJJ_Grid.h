// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/EngineTypes.h"
#include "OJJ_Grid.generated.h"

class AMachineBase;
class AConveyor;
class APipe;
class AResourceBase;
class AOJJ_Foundation;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * 그리드 셀 베이크 분류 (사전베이크 캐시 2bit 패킹용). 상호배타 4상태.
 * Water는 Blocked와 별개 상태지만 IsCellBuildable=false는 동일(건설금지 불변식 — §3).
 * 값(0~3)이 패킹 비트와 직결되므로 재정렬 금지(직렬화 캐시 호환 깨짐).
 */
UENUM()
enum class EOJJCellClass : uint8
{
	Buildable = 0,  // 건설 가능(초록)
	Blocked   = 1,  // 높이 단차 초과(빨강) — 건설 불가
	Void      = 2,  // 트레이스 미히트(바닥 없음) — 건설 불가, 오버레이 제외
	Water     = 3   // 물(셀 최저 Z 상대델타 < WaterSurfaceZ, 파랑) — 건설 불가 + 펌프 수원 후보(Phase B)
};

/**
 * Foundation 커버리지 셀 정보 (F1-a). 점유(OccupiedCells=차단)와 반대 의미의 "허가" 레이어 값.
 * Foundation 참조는 AActor 약참조로 일반화 — AOJJ_Foundation 클래스는 F1-b 신규라 그리드가 몰라도
 * 되게 한다(OJJ_RegisterActorCells(AActor*) 일반화와 동일 패턴). SurfaceZ는 상면 월드 Z — F1은
 * footprint 전 셀 동일값이지만 F2(높이 스냅/경사) 대비 셀별 저장.
 */
USTRUCT()
struct FOJJFoundationCellInfo
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AActor> Foundation;

	UPROPERTY()
	float SurfaceZ = 0.0f;
};

/**
 * 파이프 점유 셀 정보 (F4-0, f4_pipe_plan.md 결정 ㉠ — 파이프 전용 레이어). 머신/컨베이어
 * 점유(OccupiedCells)·Foundation 커버리지와 전부 직교하는 세 번째 레이어 — FoundationCells가
 * 선례인 독립 레이어 패턴 미러. 파이프 참조는 AActor 약참조 일반화(APipe는 타 소유(Chan) 클래스라
 * 그리드가 몰라도 되게 — Foundation F1-a와 동일 계약). CellZ = 파이프 중심 월드 Z,
 * bElevated = 오버패스 공중 셀(결정 ㉡ — 지상 셀만 컨베이어를 차단, 공중 셀 아래는 통과 허용.
 * F4-0은 보관만, 산출/소비는 F4-3).
 */
USTRUCT()
struct FOJJPipeCellInfo
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AActor> Pipe;

	UPROPERTY()
	float CellZ = 0.0f;

	UPROPERTY()
	bool bElevated = false;
};

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

	// 컨베이어 경사 게이트 한계(uu) — 접근은 OJJ_GetMaxConveyorStepZ(주석도 거기). F3.7' 개정:
	// 45 고정(보행 기준) → 프로퍼티(기본 100 = 램프 MaxRampStepPerRow 기본과 동기).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid|Conveyor", meta = (ClampMin = "1.0"))
	float OJJ_MaxConveyorStepZ = 100.0f;

	// F4-3 오버패스 클리어런스(uu) — 컨베이어 교차 셀에서 파이프가 경로 기준면 위로 상승하는 높이.
	// ㄷ자 상판 높이 + 수직 라이저 길이를 동시에 결정. 기본 100 = PIE 실측 확정(벨트 아이템 여유 + 비율).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid|Conveyor", meta = (ClampMin = "1.0"))
	float OJJ_PipeOverpassClearance = 100.0f;

	// ⭐ 컨베이어 raw-terrain 경사 주행 한계(uu) — 인접 경로 셀 간 |ΔZ|가 이 값 이하면 통과(자연 경사: 물가
	// 둑/언덕/완경사), 초과면 거부(수직 벽/절벽). 기본 300 = 물가 둑(실측 ~208uu)은 통과·수직 벽(500uu+)은 막는 값.
	// ※ 소비처는 **컨베이어 raw 경로 전용**(OJJ_ValidateConveyorSlopePath). 컨베이어는 중력 의존이라 절벽 거부 유지.
	// [파이프 경사 제한 제거] 파이프는 압송이라 수직 포함 임의 |ΔZ| 통과 — 이 상수를 더는 참조하지 않는다(과거 #182에서
	// 공유했으나 분리됨). Foundation/램프 컨베이어 경로는 OJJ_MaxConveyorStepZ(100) 별도(램프 MaxRampStepPerRow 가정 보존).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Conveyor", meta = (ClampMin = "1.0"))
	float OJJ_MaxSlopeStepZ = 300.0f;

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

	// 시각 타일 리프트(uu) — GroundZ 추종 셀에서 평탄 타일이 경사 지형면과 교차해 줄무늬로 썰리는 것 방지.
	// F2-1부터 GroundZ가 최고점 기준이라 셀 내 교차는 구조적으로 해소 — 단 5점 샘플이 ±0.4셀이라
	// 가장자리 미샘플 잔존 교차 가능. 리프트는 PIE 실측 후 0 축소 재검토(F2 계획 §1).
	// 판정 무관(시각 전용). 급경사 셀에서 오프셋으로도 부족한 잔존 교차는 F2(지형 스냅 — 셀 대표높이
	// 평균화/타일 기울임)에서 재검토.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Visualization", meta = (ClampMin = "0.0"))
	float VisualZLift = 20.0f;

	// 호버 계열(머신/Foundation/컨베이어/철거 하이라이트) 추가 리프트(uu) — 분류 오버레이 위에 떠서 식별.
	// 대안(호버 머티리얼 에미시브 강화) 중 에셋 작업이 없는 쪽 채택.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Visualization", meta = (ClampMin = "0.0"))
	float HoverExtraZLift = 30.0f;

	// === 시각 위계: 오버레이(정보, 차분) vs 호버(현재 액션, 주인공) ===
	// 문제(F1-c 후속): 오버레이와 호버가 같은 반투명 MI를 공유 → 호버가 빨강 오버레이 위에서 비쳐
	// 주황으로 합성됨. 해결: 둘을 전용 MID로 분리(OJJ_EnsureTileMIDs)하고 색/불투명을 아래 프로퍼티로
	// 구동. M_OJJ_GridFloor는 Unlit이라 색이 곧 발광(emissive) — 호버 색 채널을 1.0 초과로 주면 글로우.
	// 전부 EditAnywhere → PIE 디테일 패널에서 실시간 튜닝(PostEditChangeProperty가 즉시 재적용).

	// 바닥 분류 오버레이(빨강/초록/파랑) 공통 불투명도 — "정보는 주되 시끄럽지 않게". 낮게.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Visual Hierarchy", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OverlayOpacity = 0.1f; // #215 윤곽선 그리드: 선(분류색) + 아주 옅은 면(0.1)로 바닥 살짝 톤.

	// 오버레이 건설가능(초록) — 차분한 톤(저채도/저명도).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Visual Hierarchy")
	FLinearColor OverlayBuildableColor = FLinearColor(0.103158f, 0.288191f, 1.0f, 1.0f); // #215 윤곽선: 청색 선

	// 오버레이 건설불가/blocked(빨강) — 차분한 톤.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Visual Hierarchy")
	FLinearColor OverlayBlockedColor = FLinearColor(0.5f, 0.1f, 0.09f, 1.0f); // #215 윤곽선: 적색 선

	// 오버레이 물(파랑) — 차분한 톤.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Visual Hierarchy")
	FLinearColor OverlayWaterColor = FLinearColor(0.007655f, 0.030338f, 0.6f, 1.0f); // #215 윤곽선: 파랑 선

	// 캐릭터 점유 셀(노랑) — 빌드모드 중 플레이어 캡슐이 걸친 셀 표시(F2-4 후속 ② — 시각 전용, 점유 비등록).
	// 정보 계층이라 OverlayOpacity 공유, 색은 오버레이 3색과 구분되는 밝은 노랑.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Visual Hierarchy")
	FLinearColor CharacterCellColor = FLinearColor(0.95f, 0.80f, 0.10f);

	// 호버 공통 불투명도 — "지금 액션이 주인공". 높게(아래 오버레이를 거의 가림 → 색 섞임 제거).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Visual Hierarchy", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HoverOpacity = 0.1f; // #215 윤곽선: 호버도 옅게(채움 거의 없음).

	// 호버 가능(밝은 초록 + 살짝 에미시브) — Unlit이라 채널>1이 글로우.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Visual Hierarchy")
	FLinearColor HoverValidColor = FLinearColor(0.10f, 1.50f, 0.22f);

	// 호버 불가(밝은 빨강 + 살짝 에미시브).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Visual Hierarchy")
	FLinearColor HoverInvalidColor = FLinearColor(1.60f, 0.12f, 0.10f);

	// === 격자 선(셀 경계 = 스냅 기준선) — 채움과 독립. "선은 항상 선명." ===
	// 보이는 격자선은 별도 요소가 아니라 분류/호버 타일 머티리얼(M_OJJ_GridFloor)이 WorldPosition 기반으로
	// 그리는 셀 경계선이다(LineColor/LineOpacity). 채움(BaseColor/Opacity)을 0.30으로 낮춰도 선은 아래 값으로
	// 선명 유지 — 모든 타일(오버레이+호버)이 같은 선을 공유해 스냅 격자가 일관되게 또렷.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Visual Hierarchy")
	FLinearColor GridLineColor = FLinearColor(0.85f, 0.88f, 0.95f);

	// 격자 선 불투명도 — 채움(Overlay/Hover Opacity)과 독립. 스냅 기준선이라 높게 유지.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Visual Hierarchy", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GridLineOpacity = 0.6f; // #215 윤곽선: 선 불투명도.

	// 윤곽선 그리드 스타일(#215) — true면 셀 경계선을 **분류색(FillColor)** 으로 그린다
	// (buildable=초록선 / water=청선 / blocked=적선). false면 기존 동작(공유 GridLineColor 선 + 채움).
	// 채움(면) 제거는 별개 — Overlay/HoverOpacity를 0으로 내리면 순수 윤곽선(바닥 비침)이 된다.
	// 기본 false = 회귀 안전(기존 채움 그리드 유지). 레벨 인스턴스에서 켜고 PIE 비교/튜닝.
	// PostEditChangeProperty로 라이브 반영. 부수: 윤곽선만 그리면 면적이 줄어 지형 z-fighting 체감도 완화.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Visual Hierarchy")
	bool bOutlineGridStyle = true; // #215 확정: 윤곽선 그리드를 기본 스타일로(모든 그리드/레벨 일관).

	// 윤곽선 두께(#215) — M_OJJ_GridFloor의 LineWidth 파라미터를 라이브로 구동(슬라이더 노출). 윤곽선 모드일
	// 때만 주입 — 끄면 마스터 기본값 유지(회귀 0). 셀 경계 기준 폭(작을수록 얇은 선). PIE에서 슬라이더로 튜닝.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Visual Hierarchy", meta = (ClampMin = "0.001", ClampMax = "0.5"))
	float GridLineWidth = 0.06f; // #215 확정 윤곽 두께.

	// === 지형 높낮이 건설 제약 (정적 지형 — BeginPlay 1회 베이크) ===
	// BeginPlay에서 GridSize 전 셀 중심에서 ↓라인트레이스 → 지형 높이가 그리드 평면 Z와
	// BuildableHeightTolerance를 넘게 차이나면 UnbuildableCells에 마킹. CanPlaceMachine/컨베이어 경로가 게이트로 참조.
	// 100 = F2-2 확정(L_Planet 실측: 50→8,149셀 9.1% / 100→16,484셀 18.3%, PIE 뜸 체감 수용).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Terrain", meta = (ClampMin = "0.0"))
	float BuildableHeightTolerance = 100.0f;

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

	// === 물 자동 감지 (베이크 4번째 분류 — 시각화/건설금지까지. 펌프 연동은 Phase B) ===

	// 물 표면 고도 임계(그리드 평면 Z 상대, uu). 셀 최저 트레이스 hit의 평면대비 델타가 이 값 미만이면 water 분류.
	// 강바닥이 평면보다 파여 있으므로 음수. ⚠️ §0(BuildableReport) 미측정 — 에디터 Rebake+OJJ.Grid.ShowWater로
	// 분포 보며 튜닝(PIE 400줄 cap 회피). 변경 시 캐시 시그니처 불일치 → 재트레이스 폴백.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Water")
	float WaterSurfaceZ = -20.0f;

	// 최소 연속 물 영역(셀 수). flood-fill 4-연결 영역 크기 < 이 값이면 일반 지형으로 환원(잔웅덩이 무시).
	// 0/1 = 필터 없음(기본). Rebake 시 적용. 변경 시 캐시 무효화.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Water", meta = (ClampMin = "0"))
	int32 MinWaterCellCount = 0;

	// 셀별 지형 높이(평면 Z 상대 uu) 캐시 저장 여부. 높이추종 스폰(Phase B)용 — 기본 off(umap 비대 방지:
	// 셀당 int16 2바이트, 718²이면 ~1MB 추가). Phase B 진입 시 켜고 Rebake.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Water")
	bool bBakeGroundHeights = false;

	// 베이크 3단 분류 중 [2] blocked — 트레이스 hit이나 높이델타 초과(지형 단차). 빨강 오버레이 + 호버/배치 거부.
	UPROPERTY(Transient)
	TSet<FIntPoint> UnbuildableCells;

	// 베이크 3단 분류 중 [3] void — 트레이스 미히트(바닥 없음 = 그리드 외). 호버/배치 거부하되
	// blocked 오버레이는 안 그림. 향후 바닥/그리드라인 비주얼이 이 집합을 제외하면 바닥 모양을 자동 추종.
	UPROPERTY(Transient)
	TSet<FIntPoint> VoidCells;

	// 베이크 분류 [4] water — 별도 태그(파랑). blocked와 상호배타 상태지만 IsCellBuildable=false 유지(건설금지 불변식 §3).
	// 캐시 로드 또는 트레이스로 채워짐. 펌프 수원 인정(Phase B)은 미구현 — 현재는 분류+시각화+건설금지 전용.
	UPROPERTY(Transient)
	TSet<FIntPoint> WaterCells;

	// === 사전베이크 캐시 (직렬화 — 에디터 RebakeAndCache로 채우고 BeginPlay는 로드만, 런타임 트레이스 0) ===

	// 셀당 2bit EOJJCellClass 패킹. 인덱스 idx = X*CacheGridSize.Y + Y, byte = idx>>2, shift = (idx&3)*2.
	// 비-Transient → 레벨 저장 시 .umap에 직렬화. 718²면 ~129KB.
	UPROPERTY()
	TArray<uint8> PackedCellClasses;

	// 셀별 지형 높이(평면 Z 상대 uu, int16 양자화 클램프 ±32767). 대표값 = 5점 샘플 최고점(F2-1 결정 ① —
	// 직배치 안착 기준, 묻힘 0). 분류(blocked)는 별도로 최악점 유지. bBakeGroundHeights일 때만 채움. 빈 배열=미저장.
	UPROPERTY()
	TArray<int16> CellGroundZQuant;

	// 캐시 존재 여부 — false면 BeginPlay가 트레이스 폴백.
	UPROPERTY()
	bool bHasBakeCache = false;

	// 캐시 무효화 시그니처 — BeginPlay 로드 시 현재값과 불일치하면 경고 + 재트레이스(인덱싱/분류 정합 보장).
	UPROPERTY()
	FIntPoint CacheGridSize = FIntPoint(0, 0);
	UPROPERTY()
	float CacheCellSize = 0.0f;
	UPROPERTY()
	FVector CacheGridOrigin = FVector::ZeroVector;
	UPROPERTY()
	float CacheHeightTolerance = 0.0f;
	UPROPERTY()
	float CacheWaterSurfaceZ = 0.0f;
	UPROPERTY()
	int32 CacheMinWaterCellCount = 0;
	// 트레이스 파라미터도 분류(hit 여부=void/blocked/water)에 영향 → 시그니처 포함(변경 시 stale 캐시 차단).
	UPROPERTY()
	float CacheTraceStartHeight = 0.0f;
	UPROPERTY()
	float CacheTraceDepth = 0.0f;
	UPROPERTY()
	TEnumAsByte<ECollisionChannel> CacheTraceChannel = ECC_Visibility;
	// GroundZ 저장 토글 — off→on 시 캐시에 높이가 없으므로 무효화(Phase B 진입 시 자동 재트레이스 유도).
	UPROPERTY()
	bool bCacheBakeGroundHeights = false;
	// 베이크 산식 버전 — 파라미터가 같아도 산식 자체가 바뀌면 옛 캐시를 자동 무효화(F2-1 결정 ②).
	// 옛 맵은 필드 부재로 0 로드 → 불일치 → 재베이크 유도. 산식 변경 시 OJJ_CurrentBakeVersion 상향.
	UPROPERTY()
	int32 CacheBakeVersion = 0;
	// 현재 산식 버전: 2 = GroundZ 대표값 최고점(F2-1). 0/1(필드 도입 전 — 최악점 시절)은 자동 무효.
	static constexpr int32 OJJ_CurrentBakeVersion = 2;

	// 건설 가능(초록) 셀 per-cell 비주얼 ISM — 빌드모드 진입 시 표시. void 셀 제외 → 바닥 모양 자동 추종.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid|Terrain")
	TObjectPtr<UInstancedStaticMeshComponent> BuildableCellISM;

	// 건설 불가(빨강) 셀 per-cell 비주얼 ISM — blocked(높이초과)만. void는 제외(그리드 자체 없음).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid|Terrain")
	TObjectPtr<UInstancedStaticMeshComponent> BlockedCellISM;

	// Foundation 커버 셀(초록 — constructible 기준, F3.5') per-cell 비주얼 ISM. 오버레이 색의 의미를
	// 분류(지형)가 아니라 건설 가능성으로: blocked 셀이라도 커버되면 초록(깔면 초록, 철거하면 빨강 복귀).
	// 머티리얼은 BuildableCellMID 공유(의미 동일 — 같은 초록).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid|Terrain")
	TObjectPtr<UInstancedStaticMeshComponent> CoveredCellISM;

	// 물(파랑) 셀 per-cell 비주얼 ISM — water 분류만. ShowBlocked 패턴 미러. 머티리얼은 RefreshGridVisual에서 lazy MID(파랑).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid|Water")
	TObjectPtr<UInstancedStaticMeshComponent> WaterCellISM;

	// 캐릭터 점유 셀 비주얼 ISM(F2-4 후속 ②) — 빌드모드 중 BuildController가 OJJ_UpdateCharacterCellOverlay로
	// 구동. 시각 전용 — OccupiedCells 비등록(캐릭터 위치 Foundation 배치 허용, 깔리면 올려태우기가 받음).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid|Terrain")
	TObjectPtr<UInstancedStaticMeshComponent> CharacterCellISM;

	// water 오버레이 파랑 틴트용 동적 머티리얼. OJJ_EnsureTileMIDs에서 최초 1회 생성(에디터/PIE 공용).
	// 이제 다른 타일 MID와 동일하게 translucent M_OJJ_GridFloor(HoverValidBaseMaterial) 기반 — OverlayWaterColor/OverlayOpacity 구동.
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WaterCellMID;

	// === 타일 전용 동적 머티리얼(시각 위계 분리) ===
	// 오버레이와 호버가 같은 MI를 공유하던 구조(색 섞임 원인)를 끊고 각 용도별 MID로 분리.
	// 색/불투명은 위 Visual Hierarchy 프로퍼티로 구동. OJJ_EnsureTileMIDs에서 lazy 생성.
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ValidHoverMID;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> InvalidHoverMID;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BuildableCellMID;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BlockedCellMID;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CharacterCellMID;

	// MID 생성용 베이스 머티리얼(생성자에서 MI 에셋 캐싱). 호버/오버레이/물 MID가 이 둘에서 파생.
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> HoverValidBaseMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> HoverInvalidBaseMaterial;

	// 디버그 토글(OJJ.Grid.ShowBlocked) — 빌드모드 밖에서도 오버레이 강제 표시.
	UPROPERTY(Transient)
	bool bForceShowBlocked = false;

	// 디버그 토글(OJJ.Grid.ShowWater) — 빌드모드 밖에서도 물 오버레이 강제 표시. ShowBlocked와 독립.
	UPROPERTY(Transient)
	bool bForceShowWater = false;

	// 베이크 완료 여부 — "불가 0개"와 "아직 베이크 안 됨"을 구분(진단/콘솔 리포트용).
	UPROPERTY(Transient)
	bool bBuildableBaked = false;

	// 오버레이 부분 갱신 장부(F3.5' — 커버 전환을 90k 전체 리빌드 없이 셀 단위로): 셀→인스턴스 인덱스.
	// 인스턴스는 제거 대신 zero-scale 숨김(UpdateInstanceTransform — 인덱스 불변)이라 RemoveInstance의
	// 인덱스 시프트 시맨틱(Codex F3.5' ①) 의존이 없음. 빌드모드 오버레이(RefreshGridVisual의
	// bVisualizationActive 경로)가 재구축, ShowBlocked 디버그는 미사용(원 분류 표시).
	TMap<FIntPoint, int32> BlockedCellToInstance;
	TMap<FIntPoint, int32> CoveredCellToInstance;

	// 빌드모드 시각화 활성 상태 — bForceShowBlocked 토글이 빌드모드 오버레이를 끄지 않도록 참조.
	UPROPERTY(Transient)
	bool bVisualizationActive = false;

	// 배치 가능 셀 호버 표시 (녹색)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid|Hover")
	TObjectPtr<UInstancedStaticMeshComponent> ValidHoverISM;

	// 배치 불가 셀 호버 표시 (빨강)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid|Hover")
	TObjectPtr<UInstancedStaticMeshComponent> InvalidHoverISM;

	// === 고스트 프리뷰(#187) — 호버 셀을 따라다니는 반투명 미리보기 메시 ===
	// 액터 spawn 없이 단일 컴포넌트로 그린다(설계 원칙: 프리뷰용 머신/Foundation 액터 미스폰).
	// 머신(전 서브모드) + 평판 Foundation만 대상. 셀 변경 시에만 갱신(매 프레임 Tick 없음).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid|Ghost")
	TObjectPtr<UStaticMeshComponent> GhostMeshComp;

	// 고스트용 반투명 베이스 머티리얼(사용자가 에디터에서 지정 — 벡터 파라미터 TintColor, 스칼라 Opacity 가정).
	// 미지정이면 고스트 비활성(안전한 no-op) — OJJ_EnsureGhostMIDs가 1회 경고.
	UPROPERTY(EditAnywhere, Category = "Grid|Ghost")
	TObjectPtr<UMaterialInterface> GhostBaseMaterial;

	// 고스트 틴트(#187) — PIE 디테일 슬라이더 튜닝용(그리드 Visual Hierarchy 패턴). OJJ_EnsureGhostMIDs가
	// GhostValidMID/InvalidMID의 TintColor/Opacity에 주입(PostEditChangeProperty로 라이브 반영).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Ghost", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GhostOpacity = 0.1f; // #187 확정 — 텍스처 거의 그대로, 아주 옅은 틴트.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Ghost")
	FLinearColor GhostValidTint = FLinearColor(0.2f, 0.9f, 0.3f);   // 배치 가능(부드러운 초록)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Ghost")
	FLinearColor GhostInvalidTint = FLinearColor(1.0f, 0.2f, 0.2f); // 배치 불가(부드러운 빨강)

	// 배치 가능(초록 틴트) / 불가(빨강 틴트) 고스트 MID. OJJ_EnsureGhostMIDs에서 lazy 생성.
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GhostValidMID;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GhostInvalidMID;

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

	// #182 자원 전용 셀 레이어 — 셀 → 자원 액터(AResourceBase: WaterArea/광맥). OccupiedCells와 독립.
	// 이유: OccupiedCells는 셀당 단일 슬롯이라 펌프(CanStandOnWater)가 물 위에 등록되면 그 셀의 WaterArea
	// 참조를 덮어써(스택 아님) 잃는다 → GetWaterSurfaceZAtCell/펌프 수원조회가 WaterArea를 못 찾고 펌프가
	// 지형바닥(-997)으로 폴백. 머신은 이 맵을 절대 쓰지 않으므로(OJJ_RegisterActorCells에서 AResourceBase만
	// 기록) 펌프가 위에 점유해도 발밑 WaterArea가 보존된다. 등록/해제는 OJJ_RegisterActorCells/OJJ_RemoveActorAt
	// 대칭 경로에 얹어 OJJ_ActorToCells와 같은 수명. weak ptr이라 stale은 조회 시 null로 자연 무효화.
	TMap<FIntPoint, TWeakObjectPtr<AResourceBase>> OJJ_ResourceCellToActor;

	// === Foundation 커버리지 레이어 (F1-a — 데이터/질의만. 소비처 연결은 F1-c 게이트 교체) ===

	// 셀 → 커버 Foundation + 상면 Z. OccupiedCells와 완전 독립(점유=차단 / 커버리지=허가 — 의미 반대).
	// 불변식: F1-a 동안 기존 어떤 read/write 경로도 이 맵을 참조하지 않는다(회귀 0 보장).
	UPROPERTY(Transient)
	TMap<FIntPoint, FOJJFoundationCellInfo> FoundationCells;

	// Foundation → 커버 셀 목록 (RemoveFoundation 일괄 해제용). OJJ_ActorToCells 패턴 미러(비-UPROPERTY, weak 키).
	TMap<TWeakObjectPtr<AActor>, TArray<FIntPoint>> OJJ_FoundationToCells;

	// === 파이프 레이어 (F4-0 — 데이터/질의만. 배치/게이트 소비처 연결은 F4-1, 오버패스 산출은 F4-3) ===

	// 셀 → 파이프 + 셀 Z/공중 여부. 점유·커버리지와 완전 독립 — 교차 규칙(파이프↔컨베이어)은
	// 레이어 간 명시 규칙으로 표현(결정 ㉠(b)). 불변식: F4-0 동안 기존 어떤 read/write 경로도
	// 이 맵을 참조하지 않는다(회귀 0 — FoundationCells F1-a와 동일 도입 방식).
	UPROPERTY(Transient)
	TMap<FIntPoint, FOJJPipeCellInfo> OJJ_PipeCells;

	// 파이프 → 점유 셀 목록 (일괄 해제용) — OJJ_FoundationToCells 패턴 미러(비-UPROPERTY, weak 키).
	TMap<TWeakObjectPtr<AActor>, TArray<FIntPoint>> OJJ_PipeToCells;

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

	// GC/Destroy된 Foundation 엔트리를 커버리지 양방향 맵에서 정리. Foundation write 경로 진입부에서 호출.
	// SweepStaleEntries의 커버리지판 — 점유 맵과 레이어 독립이라 별도 함수(서로 호출하지 않음).
	void SweepStaleFoundationEntries();

	// GC/Destroy된 파이프 엔트리를 레이어 양방향 맵에서 정리 — SweepStaleFoundationEntries 미러
	// (레이어 독립이라 별도 함수). 오버레이 복원 없음(F4-0 파이프 레이어는 시각 표현 0 — F4-1 호버에서 재검토).
	void SweepStalePipeEntries();

	// 파이프 배치 검증 단일원(F4-1 — 호버/클릭 공유): 정규화·수집(포트/건설 게이트/점유/연속)은
	// 컨베이어 체인 위임, 그 위에 파이프 게이트 ① 균일 SurfaceZ 한정(수집기의 경사 폴백 불채택 —
	// 평면 파이프, 오버패스/경사는 F4-3) ② ㉤ 액체 끝점(Pump→LiquidTank) ③ ㉥ 파이프 레이어 겹침.
	bool OJJ_ValidatePipePlacement(const TArray<FIntPoint>& PathCells, TArray<FIntPoint>& OutPlacementCells,
		TArray<FIntPoint>& OutReservedCells, AMachineBase*& OutSourceMachine, AMachineBase*& OutTargetMachine,
		float& OutPathSurfaceZ, FString& OutReason) const;

	// Foundation 등록의 검증/커밋 단일원(F3-1) — 단일값(TryPlaceFoundation)과 셀별(PerCell) 공용.
	// SurfaceZForCell은 검증 통과 셀에만 호출(배열 없는 단일값 경로의 할당 0 유지).
	bool OJJ_TryPlaceFoundationInternal(AActor* Foundation, FIntPoint Origin, FIntPoint Size,
		TFunctionRef<float(FIntPoint Cell)> SurfaceZForCell, FString& OutReason);

	// 영역 셀들의 Foundation SurfaceZ 집계(IsNearlyEqual 그룹 누산) — 지배 SurfaceZ 조회 2종의 공용
	// 누산기. SnapGridOriginZ 기준 단 격자(±0.1uu) 위 후보만 누산(램프 중간 행 제외 — Codex F3.5 C).
	// 그리드 교집합만 순회(int64 끝좌표 — 거대 입력 방어 미러).
	void OJJ_AccumulateFoundationSurfaceZ(FIntPoint RectOrigin, FIntPoint RectSize,
		float SnapGridOriginZ, TArray<TPair<float, int32>>& InOutGroups) const;

	// 오버레이 셀 트랜스폼(기준면 +3 — RefreshGridVisual과 부분 갱신이 공유하는 단일원).
	FTransform OJJ_MakeOverlayCellTransform(FIntPoint Cell, bool bGroundZValid) const;

	// 장부 동반 표시/숨김(F3.5' 부분 갱신). 숨김 = zero-scale 트랜스폼(UpdateInstanceTransform, O(1)) —
	// 인덱스가 불변이라 RemoveInstance의 시프트/스왑 시맨틱에 비의존(Codex F3.5' ① 해소). 표시는
	// 장부에 있으면 트랜스폼 복원, 없으면 append(인덱스 안정). 숨김 잔존분은 다음 RefreshGridVisual의
	// ClearInstances가 정리(세션 내 상한 = blocked 셀 수).
	void OJJ_ShowOverlayInstance(UInstancedStaticMeshComponent* ISM, TMap<FIntPoint, int32>& CellToInstance,
		FIntPoint Cell, bool bGroundZValid);
	void OJJ_HideOverlayInstance(UInstancedStaticMeshComponent* ISM,
		const TMap<FIntPoint, int32>& CellToInstance, FIntPoint Cell);

	// Foundation 커버 변경의 오버레이 부분 갱신(F3.5' — 이벤트 구동, 전체 RefreshGridVisual 금지).
	// bCovered=true: 커버된 blocked 셀 빨강→초록(슬래브 상면 Z). false: 초록→빨강(지형 Z) 복귀.
	// 커버 상태 반영 "후" 호출 전제(Z 단일원이 올바른 면을 읽도록). 빌드모드 오버레이 비활성이면 no-op
	// (다음 RefreshGridVisual 전체 재적재가 정합 복원).
	void OJJ_OnFoundationCoverageVisualChanged(const TArray<FIntPoint>& Cells, bool bCovered);

	// 양방향 맵에 머신 등록. 위치 갱신은 호출자가 별도 처리. 모든 write 검증을 포함.
	// RotationSteps는 점유 footprint 계산(CanPlace/CalculateFootprint)에 전달(기본 0).
	bool RegisterMachineInternal(AMachineBase* Machine, FIntPoint Origin, FString& OutReason, int32 RotationSteps = 0);

	// === 사전베이크 캐시 직렬화 헬퍼 ===

	// 시그니처 일치 시 PackedCellClasses를 풀어 UnbuildableCells/VoidCells/WaterCells 재구성. 성공 시 true(트레이스 생략).
	// 불일치/크기오류면 경고 후 false → 호출자(BeginPlay)가 트레이스 폴백.
	bool TryLoadBakeCache();

	// 셀 → 선형 인덱스(패킹/언패킹 공통 규약). X*GridSz.Y + Y. 음수/범위 밖은 호출자가 보장.
	static int32 OJJ_CellLinearIndex(FIntPoint Cell, FIntPoint GridSz);

	// PackedCellClasses 2bit 접근자(인덱스 범위는 호출자 보장). Get은 범위 밖이면 Buildable 반환(안전 기본).
	EOJJCellClass OJJ_GetPackedClass(int32 LinearIdx) const;
	void OJJ_SetPackedClass(int32 LinearIdx, EOJJCellClass CellClass);

	// 베이크 캐시 시그니처 비교 단일원 — TryLoadBakeCache(분류 로드)와 GroundZ 유효성 검사가 공유.
	// struct=패킹 인덱싱 정확성(GridSize/CellSize/Origin), param=분류 결과 정확성(tol/waterZ/트레이스 등).
	void OJJ_GetBakeCacheSignatureMatch(bool& bOutStructMatch, bool& bOutParamMatch) const;

	// GroundZ 데이터 유효성 — 크기 정합 + 캐시 존재 + GroundZ 포함 + 시그니처 일치.
	// 크기 정합만으론 시그니처 불일치 폴백 후 남는 같은 크기의 stale 직렬화 배열을 못 거름(Codex F1-a #4-1).
	bool OJJ_HasValidGroundZData() const;

	// 셀 비주얼 기준 Z(F1-c d): Foundation 커버 → 상면 SurfaceZ / 지형 → 평면 + GroundZ 델타(유효 시) /
	// 폴백 → 평면(기존 동작 — 회귀 0). 오버레이·호버·철거 하이라이트·포트 화살표와 머신 Z 안착이
	// 같은 높이 데이터를 소비(단일원) — 굴곡 지형 묻힘/뜸 해결.
	float OJJ_GetCellVisualBaseZ(FIntPoint Cell) const;

	// 호이스팅 버전 — GroundZ 유효성(시그니처 비교)은 셀 불변이라 호출자가 1회 계산해 전달.
	// RefreshGridVisual의 90k셀 루프가 사용(Codex F1-c #5 — 셀당 시그니처 재검사 제거).
	float OJJ_GetCellVisualBaseZInternal(FIntPoint Cell, bool bGroundZValid) const;

	// === 타일 MID(시각 위계) 헬퍼 ===
	// 호버/오버레이/물 ISM에 전용 MID를 lazy 생성·할당하고 현재 프로퍼티 값을 적용. 호버 진입점
	// (ClearHoverPreview)·오버레이 갱신(RefreshGridVisual)·BeginPlay에서 호출 — 멱등(이미 있으면 재적용만).
	void OJJ_EnsureTileMIDs();

	// 현재 Visual Hierarchy 프로퍼티 값을 존재하는 MID들에 (재)적용. PIE 실시간 튜닝 경로 공용.
	void OJJ_ApplyTileMIDParams();

	// 단일 MID에 채움/선을 독립 세팅 — 채움(BaseColor/Opacity)=인자(분류색·연하게), 선(LineColor/LineOpacity)=
	// 공유 GridLineColor/GridLineOpacity(스냅 기준선·선명). 채움 투명도가 선을 흐리지 않게 분리. MID==null/없는 파라미터는 무시.
	void OJJ_SetTileParams(UMaterialInstanceDynamic* MID, const FLinearColor& FillColor, float FillOpacity) const;

	// 고스트 프리뷰(#187) MID lazy 생성. GhostBaseMaterial 미지정이면 1회 경고 후 비활성(크래시 금지).
	// 생성 시 Valid=HoverValidColor / Invalid=HoverInvalidColor 틴트, 둘 다 Opacity=HoverOpacity 세팅.
	void OJJ_EnsureGhostMIDs();

#if WITH_EDITOR
	// 디테일 패널/PIE에서 Visual Hierarchy 값 변경 시 MID에 즉시 재적용 + 오버레이 갱신(실시간 튜닝).
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

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

	// cursor cell → lower-left origin 공통 수식("마우스 = 풋프린트 중심" — (Size-1)/2 정수 나눗셈
	// lower-left bias). F3.6-0(결정 ㉽): BuildController 정적에서 이관 — 머신/Foundation 호버·배치
	// (컨트롤러 위임)와 Foundation 풋프린트 훅(OJJ_ComputeHoverFootprint 베이스)이 한 수식을 공유.
	UFUNCTION(BlueprintPure, Category = "Grid|Coordinate")
	static FIntPoint OJJ_OriginFromCursorCellForSize(FIntPoint CursorCell, FIntPoint EffSize);

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

	// 셀이 건설 가능한지(blocked·void 둘 다 아님). 베이크 전이면 항상 true.
	// F1-c부터 "지형 분류 질의"로 의미 한정 — 배치 게이트는 IsCellConstructible(OR)이 담당.
	UFUNCTION(BlueprintPure, Category = "Grid|Terrain")
	bool IsCellBuildable(FIntPoint Cell) const;

	// 건설 허용 게이트(F1-c §7-3) — 지형 가능(buildable) OR Foundation 커버. 머신/컨베이어 배치 소비처가
	// IsCellBuildable 대신 이 함수를 사용. OR은 허용 집합을 넓히기만 — 기존 지형 직배치(추출기 포함)는
	// 전부 그대로 통과(§5-2 F1~F2 직배치 유지).
	UFUNCTION(BlueprintPure, Category = "Grid|Terrain")
	bool IsCellConstructible(FIntPoint Cell) const;

	// 단일 건설면 규칙(F1-c §7-3): 셀 집합이 전부 같은 SurfaceZ의 Foundation 위(→OutZ=SurfaceZ)거나
	// 전부 비-Foundation(→OutZ=그리드 평면 Z — F1은 지형=평면이라 항상 균일)이면 true.
	// 혼합/이높이(Foundation 경계 걸침)는 false — Z 안착 모호성 제거. 머신 풋프린트/컨베이어 경로 공용.
	bool OJJ_GetUniformSurfaceZ(const TArray<FIntPoint>& Cells, float& OutZ) const;

	// 셀이 그리드 외(void = 바닥 없음/트레이스 미히트)인지. 그리드 비주얼 셀 제외 판정용.
	UFUNCTION(BlueprintPure, Category = "Grid|Terrain")
	bool IsCellVoid(FIntPoint Cell) const;

	// 셀이 물(water 분류)인지. 건설은 IsCellBuildable이 별도로 막음 — 이 질의는 수원/시각화 판정용(Phase B 펌프).
	UFUNCTION(BlueprintPure, Category = "Grid|Water")
	bool IsCellWater(FIntPoint Cell) const;

	// 셀이 blocked(건설 불가 지형 — 굴곡/경사/절벽/장애물) 분류인지. void(바닥 없음)·water와 배타(베이크 단일 분류).
	// #182 파이프 굴곡 통과 게이트(bAllowBlockedCells)가 사용 — blocked만 통과 허용하고 void는 계속 거부.
	UFUNCTION(BlueprintPure, Category = "Grid|Terrain")
	bool IsCellBlocked(FIntPoint Cell) const;

	// 셀을 덮는 점유 액체 자원(WaterArea, form=liquid)의 수면 Z를 반환. 그런 액터가 없으면 false.
	// #182 펌프 물위 배치·파이프 수면 Z의 공용 단일원 — per-puddle(WaterArea별 액터 Z) 자동(WA1/WA2 다른 수면).
	// ⚠️ 수면 = WaterArea 액터 Z(GetActorLocation().Z). plane 메시 시각 오프셋과 별개 — 게이트/안착 기준은 액터 Z.
	// "교집합" 정책: 분류 water 셀(IsCellWater)이라도 WaterArea가 안 덮으면 false → 호출자가 거부(펌프=교집합만 허용).
	UFUNCTION(BlueprintPure, Category = "Grid|Water")
	bool GetWaterSurfaceZAtCell(FIntPoint Cell, float& OutSurfaceZ) const;

	// 셀을 덮는 액체 자원(form=liquid)을 자원 전용 레이어(OJJ_ResourceCellToActor)에서 조회. 없으면 null.
	// #182 펌프 발밑 수원 연결·수면 Z 공용 단일원 — OccupiedCells(머신 점유에 덮어쓰임)와 분리돼
	// 펌프가 같은 셀을 점유해도 발밑 WaterArea를 찾는다. GetWaterSurfaceZAtCell도 이 함수로 위임.
	AResourceBase* GetLiquidResourceAtCell(FIntPoint Cell) const;

	// #182 파이프 셀별 안착 Z의 단일원 — 우선순위: 물(수면 Z) > Foundation(상면) > 지형(GroundZ 추종) > 평면.
	// 물·땅·Foundation·굴곡(blocked) 어디든 지형을 따라가는 파이프 Z를 셀 단위로 산출(경사 게이트/등록/비주얼 공용).
	float OJJ_GetPipeCellSurfaceZ(FIntPoint Cell) const;

	// #249 raw 지형 컨베이어 셀별 안착 Z = 베이크 GroundZ. 평면 폴백 금지(무효 GroundZ/off-grid면 false) —
	// 검증·배치가 "유효 지형 높이 없으면 거부"를 단일원으로 공유(회귀가드: 장부/평면 의미 불변).
	bool OJJ_GetRawTerrainSurfaceZ(FIntPoint Cell, float& OutSurfaceZ) const;

	// #249 경로가 raw-terrain 지형추종 대상인지 판정(검증 OJJ_CollectConveyorReservedCells와 배치 OJJ_TryPlaceConveyor가
	// 동일 소스 참조 → 분기 불일치 방지). 조건: ① 베이크 GroundZ 유효 ② 전 셀 valid grid ③ Foundation 셀 0개
	// (all-raw — 혼합은 이번 패스 거부) ④ 셀 간 GroundZ 변화 존재(평탄 raw는 기존 uniform 평면 경로 유지 → 회귀 0).
	// water 셀은 호출 게이트(bAllowWaterCells=false)가 이미 거부하므로 여기 도달 안 함.
	bool OJJ_IsRawTerrainFollowPath(const TArray<FIntPoint>& PathCells) const;

	// #182 커서 레이 ∩ 수면. 마우스 레이(RayOrigin/Dir)를 각 액체자원(WaterArea)의 수면 Z 평면과 교차시켜,
	// 교차점이 그 WaterArea가 덮는 셀이면 그 셀을 OutCell로 반환(MaxDistance보다 가까운 가장 앞 수면). 깊은 물에서
	// 커서 레이가 물을 통과해 물 밖 지형을 맞아 셀이 빗나가는 패럴랙스를 보이는 수면 기준으로 교정한다. C++ 전용.
	bool OJJ_TraceCursorToWaterSurface(const FVector& RayOrigin, const FVector& RayDir, float MaxDistance, FIntPoint& OutCell) const;

	// #182 파이프 시작 스냅 — 클릭 셀이 액체 출력 머신(펌프/탱크) 풋프린트 위거나 그 출력 포트 셀 MaxSnap칸
	// 이내면 등록된 출력 포트 셀을 OutStartCell로 반환(없으면 false). 3×3 펌프 바깥 한 칸을 픽셀단위로 집는
	// 비현실적 조준을 제거 — 위치만 보정하고 방향/물류는 등록 포트 그대로. 파이프 전용(컨베이어 미호출). C++ 전용.
	bool OJJ_GetPipeOutputStartCell(FIntPoint ClickedCell, int32 MaxSnap, FIntPoint& OutStartCell) const;

	// #182 ⭐ 스크린 공간 출력 포트 스냅 — 등록된 액체 출력 포트 셀을 화면에 투영해, 커서에서 MaxScreenDist
	// 픽셀 이내 가장 가까운 포트 셀을 반환(없으면 false). 월드 Z 패럴랙스(물 통과 레이가 깊은 지형을 맞아
	// 셀이 빗나감)와 완전 무관 — 화면에서 보이는 포트 박스 근처를 클릭하면 그 포트로 스냅. 펌프 큐브 위 클릭도 OK.
	bool OJJ_FindLiquidOutputPortUnderCursorScreen(class APlayerController* PC, float MaxScreenDist, FIntPoint& OutPortCell) const;

	// 지형 높이 베이크 — GridSize 전 셀 ↓트레이스로 buildable/blocked/void/water 재계산. BeginPlay 폴백 + 콘솔 재호출.
	// bVerbose: 평탄(바닥)이 아닌 셀마다 (좌표/hit/Z/부호델타/분류)를 로그(캡 있음) — 큐브 등 베이크 진단용.
	// bWriteCache: 분류 결과를 PackedCellClasses(+선택 GroundZ)로 패킹하고 시그니처 기록(에디터 RebakeAndCache 경로).
	UFUNCTION(BlueprintCallable, Category = "Grid|Terrain")
	void BakeBuildableCells(bool bVerbose = false, bool bWriteCache = false);

	// [에디터 전용 버튼] 트레이스 1회 → 패킹 캐시 저장 + 맵 dirty + 즉시 오버레이(blocked+water) 표시.
	// ⚠️ "빈 부지"(머신/컨베이어 제거)에서 실행할 것 — 베이크는 월드의 머신/컨베이어/자원을 ignore-list로 순회하므로
	//    채워진 상태로 구우면 시간만 늘고(결과는 동일) 불필요. 실행 후 반드시 레벨 저장해야 캐시가 .umap에 영속.
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Grid|Terrain")
	void RebakeAndCache();

	// 그리드 셀 비주얼 갱신 — 현재 상태(bVisualizationActive/bForceShowBlocked) 기준으로 초록(가능)/빨강(blocked)
	// per-cell ISM 재적재. void 셀은 양쪽 다 제외. 클리어 후 재적재라 진입/퇴장 반복에도 중복·잔존 없음.
	void RefreshGridVisual();

	// 디버그(OJJ.Grid.ShowBlocked) — 빌드모드와 무관하게 오버레이 강제 표시 토글.
	void SetForceShowBlocked(bool bShow);

	// 디버그(OJJ.Grid.ShowWater) — 빌드모드와 무관하게 물 오버레이 강제 표시 토글.
	void SetForceShowWater(bool bShow);

	// === Foundation 커버리지 (F1-a — 데이터 레이어/질의 전용. 게이트 연결은 F1-c, 액터 스폰/위치는 F1-b) ===

	// Origin부터 Size(X×Y) 풋프린트가 Foundation 배치 가능한지. 전 셀을 끝까지 순회해 사유별 셀 수를
	// 집계하고 OutReason에 기록(예: "water 9 / occupied 3") — water 43% 지형에서 분포 실측이
	// F1-b 디버깅·§5-3 waterZ 재검토의 근거가 되도록 조기 종료 없이 센다.
	// 게이트: off-grid·겹침(기존 Foundation)·void·water(§5-3 미결 — F1 기본 금지)·점유(머신/컨베이어/자원) 금지.
	// blocked(높이 단차)는 의도적으로 허용 — 단차 흡수가 Foundation의 존재 이유.
	// ※ 자원 점유 셀 거부는 F1 보수 기본값 — "광맥 위 Foundation+추출기" 시나리오는 §5-2 결정 후 재검토(F1-c).
	UFUNCTION(BlueprintPure, Category = "Grid|Foundation")
	bool CanPlaceFoundation(FIntPoint Origin, FIntPoint Size, FString& OutReason) const;

	// Foundation 커버리지 등록 — 검증(CanPlaceFoundation) + 양방향 맵 등록만 수행. 액터 위치/비주얼은
	// 건드리지 않음(F1-b BuildController 책임 — 그리드는 Foundation 메시/Thickness를 모름).
	// SurfaceZ는 호출자 전달(F2-4: 평면 + Thickness + 스냅 리프트 — 컨트롤러가 OJJ_ComputeFoundationSnapLift로
	// 산출). 전 셀 동일값 — 셀별은 OJJ_TryPlaceFoundationPerCell(F3-1). 서버 권위 전용, 중복 등록 거부.
	UFUNCTION(BlueprintCallable, Category = "Grid|Foundation")
	bool TryPlaceFoundation(AActor* Foundation, FIntPoint Origin, FIntPoint Size, float SurfaceZ, FString& OutReason);

	// 셀별 SurfaceZ 등록(F3-1, 결정 ㉲ — 램프 등 비평탄 Foundation용). CellSurfaceZs 인덱싱:
	// (X−Origin.X)×Size.Y + (Y−Origin.Y) (OJJ_CellLinearIndex와 동일 row-major). 산식은 액터(클래스) 책임,
	// 그리드는 배열 불변식 검증(액터 신뢰 금지): ① 크기 = Size.X×Size.Y ② 전 값 유한 ③ (max−min)이
	// 단 간격(OJJ_FoundationSnapStep)의 정수배 — 절대 단 격자 정합은 그리드가 Thickness를 모르는 계약상
	// 상대 검증(턱 0 산식의 양 끝 정합이 이를 보장). 위반 시 거부 + OutReason. UFUNCTION 오버로드 불가(UHT)라 별도 이름.
	bool OJJ_TryPlaceFoundationPerCell(AActor* Foundation, FIntPoint Origin, FIntPoint Size,
		const TArray<float>& CellSurfaceZs, FString& OutReason);

	// Foundation 커버리지 해제. 커버 셀 위에 유효 점유(머신/컨베이어)가 하나라도 있으면 거부 + 점유 셀 수
	// 기록 — F1은 연쇄 철거 대신 거부가 안전. 서버 권위 전용.
	UFUNCTION(BlueprintCallable, Category = "Grid|Foundation")
	bool RemoveFoundation(AActor* Foundation, FString& OutReason);

	// 셀이 유효한 Foundation에 커버되는지. 파괴된(stale) Foundation의 셀은 false(점유 stale 처리와 일관).
	UFUNCTION(BlueprintPure, Category = "Grid|Foundation")
	bool IsCellOnFoundation(FIntPoint Cell) const;

	// 커버 셀의 Foundation 상면 월드 Z. 비커버/stale 셀이면 false + OutSurfaceZ=0.
	UFUNCTION(BlueprintPure, Category = "Grid|Foundation")
	bool GetFoundationSurfaceZ(FIntPoint Cell, float& OutSurfaceZ) const;

	// 셀을 커버하는 Foundation 액터(비커버/stale이면 nullptr) — Demolish(F1-b')의 셀→Foundation 역조회.
	// Foundation은 점유(OccupiedCells)와 별개 레이어라 GetActorAtCell로는 안 보인다.
	UFUNCTION(BlueprintPure, Category = "Grid|Foundation")
	AActor* GetFoundationAtCell(FIntPoint Cell) const;

	// Foundation의 커버 셀 목록(미등록이면 nullptr) — GetActorCells의 Foundation판(철거 호버 하이라이트용).
	const TArray<FIntPoint>* GetFoundationCells(AActor* Foundation) const;

	// Foundation 커버 셀 중 유효 점유(머신/컨베이어) 셀 수 — 철거 가능 판정의 read-only 단일원(F2-0,
	// Codex F1-b' #4): RemoveFoundation(클릭 거부)과 철거 호버(UpdateDemolishHover)가 같은 식을 공유해
	// "호버 빨강인데 클릭 거부" 불일치 차단. 미등록 Foundation이면 INDEX_NONE.
	int32 OJJ_CountOccupiedFoundationCells(AActor* Foundation) const;

	// 사각 영역 내 셀들의 지배 Foundation SurfaceZ(F3.5) — 접촉 셀 수 최다, 동률이면 낮은 단(결정 ㉷).
	// stale Foundation은 GetFoundationSurfaceZ의 weak IsValid가 걸러 비후보(유령 단 상속 차단).
	// SnapGridOriginZ(호출 클래스가 평면+Thickness로 산출 — 그리드는 Thickness를 모름): 이 원점 기준
	// 단 간격(100) 정수배가 아닌 SurfaceZ(램프 중간 행 등)는 집계에서 제외 — 비격자 단 상속 차단
	// (Codex F3.5 C). off-grid 셀 스킵, 후보 없으면 false. 램프 엣지 스냅(F3.5 ③)이 직접 소비.
	bool OJJ_GetDominantFoundationSurfaceZInRect(FIntPoint RectOrigin, FIntPoint RectSize,
		float SnapGridOriginZ, float& OutSurfaceZ, int32& OutContactCells) const;

	// 풋프린트 둘레(면접촉 4방 변 — 결정 ㉶, 대각 모서리 제외)의 지배 이웃 SurfaceZ — F3.5 ① 높이
	// 상속(평면 확장)용. 4변 라인을 합산 집계 후 ㉷ 규칙으로 선출. 비격자 단 필터는 위와 동일.
	// 이웃 없으면 false(씨앗 경로).
	bool OJJ_GetNeighborFoundationSurfaceZ(FIntPoint Origin, FIntPoint Size,
		float SnapGridOriginZ, float& OutSurfaceZ, int32& OutContactCells) const;

	// Foundation 풋프린트 중심 월드 좌표(F1-b 결정점 ② — 그리드는 좌표만, 액터 이동은 호출자).
	// 머신 GetMachinePlacementLocation과 동형 수식(lower-left 셀 중심 + (Size-1)/2)이되 메시 AABB Z 보정은
	// 없음 — Foundation이 자체 메시 오프셋(상면=액터Z+Thickness)을 책임진다. Z = 그리드 평면 —
	// 높이 스냅(F2-4)은 호출자가 OJJ_ComputeFoundationSnapLift 반환값을 Z에 더해 적용.
	UFUNCTION(BlueprintPure, Category = "Grid|Foundation")
	FVector GetFoundationPlacementLocation(FIntPoint Origin, FIntPoint Size) const;

	// Foundation 높이 스냅 리프트(F2-4 §5-4): 풋프린트 GroundZ 최고점에 맞춘 단(段) 오프셋 N×100uu 반환.
	// N = ceil((max GroundZ − Thickness) / 100) clamp ≥0 → 상면(평면+Thickness+반환값)이 항상 지형 최고점
	// 이상(묻힘 0)이고 초과 < 100. 평탄 지대 N=0 = F1 동작 그대로. GroundZ 무효(미베이크/시그니처 불일치)
	// 또는 풋프린트가 그리드 밖이면 0 — 평면 폴백(회귀 0). N 상한 없음(결정 ⑤) — 분포는 배치 로그 N으로 실측.
	float OJJ_ComputeFoundationSnapLift(FIntPoint Origin, FIntPoint Size, float Thickness) const;

	// 단 간격(1m). 고정 상수 — "같은 단 = 같은 SurfaceZ"가 걸침 허용/거부(OJJ_GetUniformSurfaceZ)의
	// 기준이라 디자이너 튜닝 비노출(단 간격이 갈라지면 동일성 판정이 무의미해짐).
	static constexpr float OJJ_FoundationSnapStep = 100.0f;

	// 컨베이어 경사 게이트 한계(F3.7-1 ㊆, F3.7' 개정): 인접 경로 셀 간 허용 |ΔZ|(uu).
	// 기본 100 = 램프 MaxRampStepPerRow 기본과 동기 — 급경사 램프(행당 ≤100, 보행 불가) 위
	// 컨베이어 허용. 보행 불가 경고는 램프 배치 로그가 담당(컨베이어는 어차피 비보행).
	UFUNCTION(BlueprintPure, Category = "Grid|Conveyor")
	float OJJ_GetMaxConveyorStepZ() const { return OJJ_MaxConveyorStepZ; }

	// #249 컨베이어 raw-terrain 경사 게이트 한계(uu) 접근자 — **컨베이어 raw 경로 전용**(파이프는 경사 제한 제거됨).
	// raw 경로 인접 셀 |ΔZ| 판정에 쓰여 자연 경사(물가 둑 등)는 통과·수직 벽은 거부. Foundation/램프 컨베이어
	// 경로는 OJJ_GetMaxConveyorStepZ(100) 별도(램프 MaxRampStepPerRow 가정 보존).
	UFUNCTION(BlueprintPure, Category = "Grid|Conveyor")
	float OJJ_GetMaxSlopeStepZ() const { return OJJ_MaxSlopeStepZ; }

	// Foundation 배치 호버 미리보기 — 풋프린트 전체를 단일색 녹(가능)/적(불가)으로 표시.
	// 단일 진실원: 색 판정 = 클릭 시 판정(CanPlaceFoundation) — 머신 UpdateHoverPreview와 동일 정책
	// (셀별 색은 실제 배치 판정과 어긋나는 "거짓말"이라 과거 회귀로 제거된 방식 — 재도입 금지).
	// bForceInvalid(F3.6-1): 풋프린트 자체가 구성 불가(자동 맞춤 경사 한계 — 클릭도 같은 훅 bValid로
	// 거부되므로 단일 진실원 유지)일 때 호출자가 빨강을 강제.
	UFUNCTION(BlueprintCallable, Category = "Grid|Hover")
	void OJJ_UpdateFoundationHoverPreview(FIntPoint Origin, FIntPoint Size, bool bForceInvalid = false);

	// 캐릭터 점유 셀 표시 갱신(F2-4 후속 ② — 시각 전용). 빈 배열 = 클리어. 셀 변경 시에만 호출하는
	// 책임은 호출자(BuildController가 이전 셀 비교) — 여기는 ClearInstances+재적재만. Z는
	// OJJ_GetCellVisualBaseZ 단일원(Foundation 위에 서 있으면 상면에 표시) + 호버보다 1uu 위(겹침 시 식별).
	void OJJ_UpdateCharacterCellOverlay(const TArray<FIntPoint>& Cells);

	// === 파이프 레이어 API (F4-0, f4_pipe_plan.md — 등록/해제/질의만. 경로 검증·포트 정합은 F4-1) ===

	// 파이프 셀 등록 — 유효 셀·배열 1:1(액터 신뢰 금지)·파이프 간 겹침 거부(결정 ㉥) + 양방향 맵
	// 등록만 수행. 액터 위치/비주얼 불관여(Foundation 계약 미러 — 그리드는 파이프 메시를 모름).
	// 머신/컨베이어 점유 셀과의 공존 규칙은 검사하지 않음 — OJJ_RegisterActorCells와 동일 경계
	// ("점유 데이터 등록" 전용, placement 유효성은 F4-1 수집기 소관). 서버 권위 전용, 중복 등록 거부.
	bool OJJ_TryRegisterPipeCells(AActor* Pipe, const TArray<FIntPoint>& Cells,
		const TArray<float>& CellZs, const TArray<bool>& ElevatedFlags, FString& OutReason);

	// 파이프 셀 일괄 해제(철거) — RemoveFoundation 미러. 파이프는 위 건물 게이트가 없음(레이어 위에
	// 아무것도 안 올라감 — 거부 사유 없이 항상 해제). 미등록이면 false. 서버 권위 전용.
	bool OJJ_UnregisterPipeCells(AActor* Pipe, FString& OutReason);

	// 셀의 파이프 조회(없거나 stale이면 nullptr). 점유(GetActorAtCell)와 별개 레이어 — 그쪽에 안 보임.
	AActor* OJJ_GetPipeAtCell(FIntPoint Cell) const;

	// 셀이 "지상" 파이프에 점유됐는지 — 컨베이어 역방향 게이트 입력(결정 ㉡: 지상 셀만 차단,
	// 공중(bElevated) 셀 아래는 통과 허용). stale은 false. 소비처 연결은 F4-3(F4-0은 질의만 제공).
	bool OJJ_IsCellBlockedByGroundPipe(FIntPoint Cell) const;

	// 파이프의 점유 셀 목록(미등록이면 nullptr) — GetFoundationCells 미러(F4-1 철거 호버용).
	const TArray<FIntPoint>* OJJ_GetPipeCells(AActor* Pipe) const;

	// === 파이프 배치 (F4-1 — 컨베이어 배치 체인 위임 + 파이프 전용 게이트) ===

	// 파이프 경로 정규화 — 컨베이어 정규화(OJJ_BuildConveyorPlacementPath)에 위임: 끝점 포트 규칙이
	// 동일(펌프 back-output 출발 / 탱크 front-input 도착 — MachineTable In/Out 1 SSOT, f4 §4).
	// OutReason의 "Conveyor" 명칭은 위임 수용(문구 일반화 = 컨베이어 거동 영역 변경이라 기각).
	bool OJJ_BuildPipePlacementPath(const TArray<FIntPoint>& DragCells, TArray<FIntPoint>& OutPathCells,
		FString& OutReason) const;

	// 파이프 배치 — 검증(OJJ_ValidatePipePlacement) → 파이프 레이어 등록(평면: bElevated=false) →
	// SetPath/앵커·Z 안착/ConfigureTransport. OJJ_TryPlaceConveyor 미러(점유 대신 레이어 등록).
	bool OJJ_TryPlacePipe(APipe* Pipe, const TArray<FIntPoint>& PathCells, FString& OutReason);

	// 파이프 경로 호버 — 검증 = 클릭 판정 전체 공유(호버 색 = 클릭 가부 단일 진실원, F2-0 계약).
	// 컨베이어 프리뷰(정규화만 검사)보다 강함 — 파이프는 게이트가 많아(액체 끝점/균일/레이어 겹침)
	// 정규화만으론 호버≠클릭 불일치가 잦다.
	void OJJ_UpdatePipePathHoverPreview(const TArray<FIntPoint>& PathCells);

	// 머신을 끝점으로 갖는 등록 파이프 수집(철거 캐스케이드 — CollectConveyorsConnectedToMachine판).
	// 컨베이어판(둘레 셀 스캔)과 달리 레이어 역방향 맵 순회 + Source/Target 직접 대조(파이프가 끝점 보관).
	void OJJ_GetPipesConnectedToMachine(AMachineBase* Machine, TArray<APipe*>& OutPipes) const;

	// === 지형 높이 캐시 접근 (F0 갭 해소 — CellGroundZQuant 소비처 첫 도입) ===

	// 셀 지형 높이(그리드 평면 Z 상대 부호 델타, uu — 베이크 "최악점" 기준 저장값 그대로). 월드 Z는
	// 호출자가 액터 Z를 더한다. 높이 캐시 미존재(GroundZ off 베이크/GridSize 불일치 잔존 배열)면 false.
	UFUNCTION(BlueprintPure, Category = "Grid|Terrain")
	bool GetCellGroundZ(FIntPoint Cell, float& OutGroundZDelta) const;

	// GroundZ 캐시 리포트 — 요약(min/max/평균/비제로 + 10버킷 히스토그램) + Center/Radius 지정 시
	// 셀별 덤프(캡 400줄 — 베이크 verbose와 동일). 콘솔 OJJ.Grid.GroundZReport가 호출. C++ 전용.
	void DumpGroundZReport(FIntPoint Center = FIntPoint(-1, -1), int32 Radius = 0) const;

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
	// bAllowConveyorOverpass: 파이프 정규화(OJJ_BuildPipePlacementPath)가 true 전달 — 내부 수집 검증이
	// 컨베이어 점유 셀을 타넘기로 허용. 컨베이어 호출(기본 false)은 기존 동작 그대로.
	// bAllowBlockedCells: blocked(굴곡/경사/절벽) 셀 통과 허용(절벽 거부는 경사 게이트가 |ΔZ|로 별도 판정).
	// #249부터 컨베이어 기본값 true — raw-terrain 지형추종이 가파른 자연 둑(blocked)을 GroundZ 따라 타고 넘되
	// |ΔZ|>OJJ_MaxSlopeStepZ(300)인 수직 벽은 경사 게이트가 거부. water 거부는 bAllowWaterCells=false가 유지.
	// (호버/빌드/드래그/CanPlace 전 컨베이어 호출이 이 기본값을 공유 → 셀 확정 소스 일치, 분기 divergence 0.)
bool OJJ_BuildConveyorPlacementPath(
	const TArray<FIntPoint>& DragCells,
	TArray<FIntPoint>& OutPathCells,
	FString& OutReason,
	bool bAllowConveyorOverpass = false,
	bool bAllowLiquidMachines = false,
	bool bAllowWaterCells = false,
	bool bAllowBlockedCells = true) const;

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

	// === 고스트 프리뷰(#187) — 호버 셀 위 반투명 미리보기 메시 ===

	// 머신 CDO의 메시를 호버 셀(Origin/회전)에 반투명으로 그린다. bValid=배치 가능(초록)/불가(빨강) 틴트.
	// 메시 fit·XY 중심·BaseZ는 GetMachinePlacementLocation 산식 재사용, ZOffset은 CDO-safe(라이브 트랜스폼 무의존).
	// MachineCDO/메시/MID 미비 시 안전하게 고스트 숨김.
	void OJJ_ShowGhostForMachine(AMachineBase* MachineCDO, FIntPoint Origin, int32 RotationSteps, bool bValid);

	// 평판 Foundation CDO의 슬래브(엔진 Cube)를 호버 풋프린트에 반투명으로 그린다. UpdateSlabVisual 산식 재현.
	// 평판 전용(램프는 호출하지 않음). MID 미비 시 안전하게 고스트 숨김.
	void OJJ_ShowGhostForFoundation(AOJJ_Foundation* FoundationCDO, FIntPoint Origin, FIntPoint EffSize, bool bValid);

	// 고스트 숨김(ClearHoverPreview / 램프 선택 / 미지정 머티리얼 등).
	void OJJ_HideGhost();

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
