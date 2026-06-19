// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OJJ_BuildController.generated.h"

class AOJJ_Grid;
class AOJJ_Foundation;
class AOJJ_Ladder;
class AMachineBase;
class AConveyor;
class APipe;
class APowerGridNode;
class APowerLine;

// 빌드 배치 모드 — 머신(기본) / 컨베이어 드래그.
UENUM(BlueprintType)
enum class EOJJ_BuildPlacementMode : uint8
{
	Machine,
	Conveyor,
	PowerNode,
	PowerLine,
	Shield,
	PowerPlant,
	Grinder,
	Miner,
	Pump,
	Smelter,
	// 창고(Warehouse) 모드 — 1번 키(generic Machine 진입 키 대체)로 진입. WarehousePort 배치.
	Warehouse,
	// 철거(Demolish) 모드 — X키. 호버 대상 빨강 하이라이트 + 좌클릭 제거(머신/컨베이어/Foundation —
	// 위 건물 있으면 거부, F1-b'). 광맥/WaterArea 제외.
	Demolish,
	// Foundation(기초) 모드 — G키. 커버리지(허가) 배치 — 머신 경로와 독립 분기(F1-b).
	// ⚠️ 신규 모드는 항상 맨 끝에 append — BP가 enum 값을 직렬화하므로 중간 삽입(값 시프트) 금지.
	Foundation,
	// 파이프 모드(F4-1) — 컨베이어와 드래그 상태머신 공용(프리뷰/커밋만 분기). 펌프→물탱크 액체 라인.
	Pipe,
	// 물탱크 모드(F4-1') — 기존 머신 서브모드 패턴(발전소/펌프와 동일). 파이프 도착 끝점용.
	LiquidTank,
	MoldingMachine,
	Synthesizer,
	TeleCommunicationTower,
	// [#184] 사다리 모드 — C키. 커서로 Foundation 변 조준 → 그 변 바깥 지면에 사다리 배치(자유 배치, 그리드
	// 장부 미등록). ⚠️ 맨 끝 append 유지(BP enum 값 직렬화 — 중간 삽입 금지).
	Ladder,
	// [공용키 Z] 마우스 초기화 — 들고 있던 placement 고스트 취소, 빌드모드 유지. 호버/클릭 무동작.
	// ⚠️ 맨 끝 append 유지(BP enum 값 직렬화 — 중간 삽입 금지).
	None
};

/**
 * 건설 모드 컨트롤러. 호버 갱신, 미리보기, 클릭 배치 라우팅을 담당.
 * 빌드모드 진입/종료 토글과 클릭 배치는 외부(플레이어 Pawn)가 입력에서
 * ToggleBuildMode/OnLeftClickPressed로 위임 호출한다.
 * 호버 갱신(UpdateMouseHover)은 이 액터가 자체 Tick으로 구동한다 — Tick은
 * 기본 비활성, EnterBuildMode에서 켜지고 ExitBuildMode에서 꺼진다(빌드모드 밖 0비용).
 *
 * 풋프린트 조회는 MachineClass의 CDO(GetDefaultObject)에서 GetMachineSize만 읽음.
 * 미리보기 전용으로 머신 액터를 spawn하지 않으므로 부작용(BeginPlay/tick/collision/
 * 서브클래스 게임플레이 로직)에서 자유로움.
 *
 * ⚠️ MULTIPLAYER LIMITATION: Single-player only
 *
 * This controller currently spawns and registers machines locally on the calling client.
 * AOJJ_Grid::TryPlaceMachine requires HasAuthority(), so in multiplayer:
 * - Clients calling OnLeftClickPressed will spawn a local throwaway machine
 * - The grid registration will silently fail (ensure warning + false return)
 * - Machine becomes orphaned and not visible to other players
 *
 * For multiplayer support, refactor to:
 * 1. OnLeftClickPressed → Server RPC with requested cell
 * 2. Server validates MachineClass/grid/cell
 * 3. Server spawns and registers the real AMachineBase
 * 4. Replication handles client visibility
 *
 * Tracked for future Phase (MP support).
 */
UCLASS()
class WANTED_FACTORY_API AOJJ_BuildController : public AActor
{
	GENERATED_BODY()

public:
	AOJJ_BuildController();

	// 빌드모드 동안만 활성화되는 자체 Tick — UpdateMouseHover 구동.
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "BuildController")
	TObjectPtr<AOJJ_Grid> TargetGrid;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildController")
	TSubclassOf<AMachineBase> MachineClass;

	// 컨베이어 모드에서 spawn할 클래스(기본 AConveyor, 생성자에서 설정). BP 파생 지정 가능.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildController")
	TSubclassOf<AConveyor> ConveyorClass;

	// 파이프 모드에서 spawn할 클래스(F4-1 — 기본 APipe, 생성자에서 설정). BP 파생(Chan BP_Pipe) 지정 가능.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildController")
	TSubclassOf<APipe> PipeClass;

	// 물탱크 모드 클래스(F4-1' — 기본 ALiquidTank, 생성자에서 설정). 머신 서브모드 패턴 미러.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildController")
	TSubclassOf<AMachineBase> LiquidTankClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildController")
	TSubclassOf<APowerLine> PowerLineClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildController")
	TSubclassOf<APowerGridNode> PowerGridNodeClass;

	// 차폐장(Shield) 모드에서 배치할 머신 클래스(AOJJ_ProtectionTower 등). 머신 배치 경로 재사용 — PowerNode와 동일.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildController")
	TSubclassOf<AMachineBase> ShieldClass;

	// 발전소(PowerPlant) 모드에서 배치할 머신 클래스(APowerPlant 등). 머신 배치 경로 재사용 — Shield와 동일.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildController")
	TSubclassOf<AMachineBase> PowerPlantClass;

	// 그라인더(Grinder) 모드에서 배치할 머신 클래스(AGrinder 등). 머신 배치 경로 재사용 — PowerPlant와 동일.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildController")
	TSubclassOf<AMachineBase> GrinderClass;

	// 채굴기(Miner) 모드에서 배치할 머신 클래스(AMinerMachine 등). 머신 배치 경로 재사용 — PowerPlant와 동일.
	// ※ 인접 자원/선점 배치 제약([2])은 도메인 소유자(SSR) 조율 후 별도 진행 — 현재는 일반 머신 배치.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildController")
	TSubclassOf<AMachineBase> MinerClass;

	// 펌프(Pump) 모드에서 배치할 머신 클래스(APump 등). 머신 배치 경로 재사용 — PowerPlant와 동일.
	// ※ 인접 자원/선점 배치 제약([2])은 도메인 소유자(SSR) 조율 후 별도 진행 — 현재는 일반 머신 배치.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildController")
	TSubclassOf<AMachineBase> PumpClass;

	// 스멜터(Smelter) 모드에서 배치할 머신 클래스(ASmelter 등). 머신 배치 경로 재사용 — PowerPlant와 동일.
	// ※ Smelter는 generic Machine 모드로도 배치 가능하며, 전용 키는 추가 경로(두 경로 공존).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildController")
	TSubclassOf<AMachineBase> SmelterClass;

	// 창고(Warehouse) 모드에서 배치할 머신 클래스(AWarehousePort 등). 머신 배치 경로 재사용 — Smelter와 동일.
	// 1번 키(generic Machine 진입 키 대체)로 진입. WarehousePort C++/저장(PlayerWarehouse) 로직은 Chan 소유 — 무수정.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildController")
	TSubclassOf<AMachineBase> WarehouseClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildController")
	TSubclassOf<AMachineBase> MoldingMachineClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildController")
	TSubclassOf<AMachineBase> SynthesizerClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildController")
	TSubclassOf<AMachineBase> TeleCommunicationTowerClass;

	// Foundation 모드에서 배치할 평판 클래스(AOJJ_Foundation 파생 BP 지정 가능). 머신이 아니므로
	// GetActiveMachineClass/머신 배치 경로 비경유 — Conveyor/Demolish처럼 독립 분기(F1-b).
	// F3-2.5: 구 FoundationClass에서 개명 — 기존 BP/레벨 인스턴스 지정은 DefaultEngine.ini
	// [CoreRedirects] PropertyRedirects가 이관(미적용 에셋은 에디터 재저장 시 확정).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildController")
	TSubclassOf<AOJJ_Foundation> FlatFoundationClass;

	// Foundation 모드에서 배치할 램프 클래스(F3-2.5 — AOJJ_RampFoundation 또는 파생 BP).
	// 미지정이면 램프 선택(OJJ_SelectFoundationKind)이 거부되고 평판만 사용 가능.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildController")
	TSubclassOf<AOJJ_Foundation> RampFoundationClass;

	// [#184] 사다리 모드에서 스폰할 클래스(AOJJ_Ladder 파생 BP 지정 가능). 머신/그리드 장부 미경유 —
	// Foundation처럼 독립 분기(자유 배치). 기본값 = C++ AOJJ_Ladder(생성자에서 — BP 없이도 동작).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildController")
	TSubclassOf<AOJJ_Ladder> LadderClass;

	// 현재 배치 모드. Machine(기본)/Conveyor. 플레이어가 SetPlacementMode로 전환.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BuildController")
	EOJJ_BuildPlacementMode PlacementMode = EOJJ_BuildPlacementMode::Machine;

	// 컨베이어 드래그 진행 여부(좌클릭 누름~뗌).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BuildController|Conveyor")
	bool bIsDraggingConveyor = false;

	// 드래그 중 누적된 경로 셀(연속/되돌림 처리). 커밋 시 그리드 검증 경로의 입력.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BuildController|Conveyor")
	TArray<FIntPoint> ConveyorDragCells;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BuildController|Power")
	bool bIsDraggingPowerLine = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BuildController|Power")
	TWeakObjectPtr<AMachineBase> PowerLineStartMachine;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BuildController")
	bool bIsBuildMode = false;

	// 마우스 아래 cursor cell (WorldToGrid 결과). 머신의 lower-left origin은
	// ComputeOriginFromCursorCell로 변환 — 짝수 머신은 lower-left bias 정책.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BuildController")
	FIntPoint CurrentHoverCell = FIntPoint::ZeroValue;

	// 호버 머신의 90° 회전 step(0~3, 시계방향). 호버/배치 footprint·메시에 반영(단계 3·4).
	// CDO에 못 담으므로 회전 상태는 이 컨트롤러가 소유. EnterBuildMode에서 0으로 초기화.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BuildController")
	int32 HoverRotationSteps = 0;

	// Foundation 모드의 현재 종류(F3-2.5): false=평판(Flat), true=램프(Ramp). 회전 상태와 동일
	// 라이프사이클 — Enter/ExitBuildMode에서 평판으로 리셋, 모드 전환(SetPlacementMode)은 유지.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BuildController")
	bool bRampFoundationSelected = false;

public:
	UFUNCTION(BlueprintCallable, Category = "BuildController")
	void EnterBuildMode();

	UFUNCTION(BlueprintCallable, Category = "BuildController")
	void ExitBuildMode();

	UFUNCTION(BlueprintCallable, Category = "BuildController")
	bool IsInBuildMode() const { return bIsBuildMode; }

	// 연동된 그리드 접근자 (빌드 카메라 자동 센터링 등 외부에서 그리드 중심을 얻기 위함).
	UFUNCTION(BlueprintPure, Category = "BuildController")
	AOJJ_Grid* GetTargetGrid() const { return TargetGrid; }

	// PlayerController가 Tick에서 호출. 마우스 위치 → 그리드 셀 → 호버 미리보기 갱신.
	UFUNCTION(BlueprintCallable, Category = "BuildController")
	void UpdateMouseHover();

	// [철거 모드] 커서 셀의 제거 가능 대상(머신/컨베이어)을 점유 셀 전체 빨강 하이라이트. 빈 셀/광맥/WaterArea는 제외.
	void UpdateDemolishHover();

	// [철거 모드] 커서 셀의 대상 제거(머신=RemoveMachineAt 훅 연쇄, 컨베이어=OJJ_RemoveActorAt). 직후 호버 갱신(연속 철거).
	void DemolishUnderCursor();

	// [철거 모드] 철거 머신을 끝점(Source/Target)으로 갖는 컨베이어 라인을 수집. footprint 전 둘레 4방향 셀을
	// 스캔(포트 셀만이 아니라 둘레 전체 — 포트 규칙 변경에도 견딤) + GetSource/TargetMachine==Machine 검증(오삭제 방지).
	TArray<class AConveyor*> CollectConveyorsConnectedToMachine(AMachineBase* Machine) const;
	TArray<class APowerLine*> CollectPowerLinesConnectedToMachine(AMachineBase* Machine) const;

	// PlayerController가 입력에서 호출. Machine 모드: 머신 배치. Conveyor 모드: 드래그 시작.
	UFUNCTION(BlueprintCallable, Category = "BuildController")
	void OnLeftClickPressed();

	// PlayerController가 좌클릭 뗌에서 호출. Conveyor 모드: 드래그 커밋(배치). Machine 모드: no-op.
	UFUNCTION(BlueprintCallable, Category = "BuildController")
	void OnLeftClickReleased();

	// 배치 모드 전환. 전환 시 진행 중 컨베이어 드래그를 취소하고 호버 갱신.
	UFUNCTION(BlueprintCallable, Category = "BuildController")
	void SetPlacementMode(EOJJ_BuildPlacementMode NewMode);

	UFUNCTION(BlueprintPure, Category = "BuildController")
	EOJJ_BuildPlacementMode GetPlacementMode() const { return PlacementMode; }

	// 진행 중인 컨베이어 드래그 취소(좌클릭 취소 트리거 / 모드 전환 시).
	UFUNCTION(BlueprintCallable, Category = "BuildController")
	void CancelConveyorDrag();

	UFUNCTION(BlueprintCallable, Category = "BuildController")
	void CancelPowerLineDrag();

	// 빌드 모드 상태 토글. Enter/Exit의 자체 가드(이미 같은 상태면 no-op) 덕분에 안전.
	UFUNCTION(BlueprintCallable, Category = "BuildController")
	void ToggleBuildMode();

	// 호버 머신을 시계방향 90° 회전(step +1, mod 4). 빌드모드에서만 동작. 플레이어 R키가 위임.
	UFUNCTION(BlueprintCallable, Category = "BuildController")
	void RotateHoverClockwise();

	// Foundation 종류 직행 선택(F3.7' 키 개편: G=평판/H=램프 — F3-2.5 T 토글 대체, 상태 기억이
	// 필요 없는 직행 키). Foundation 모드가 아니면 진입까지 수행. 종류 변경 시 호버 즉시 갱신
	// (CDO 풋프린트 차이 반영 + 이전 종류 타일 잔존 금지). 램프 미지정 시 선택 거부 + 경고.
	UFUNCTION(BlueprintCallable, Category = "BuildController")
	void OJJ_SelectFoundationKind(bool bSelectRamp);

private:
	// cursor cell → lower-left origin 변환. 마우스 = 풋프린트 중심 정책.
	// (Size-1)/2 정수 나눗셈 → lower-left bias:
	//  - 1x1: offset 0 (회귀 없음)
	//  - 2x2: offset 0 → 마우스가 머신 좌하단 셀
	//  - 3x3: offset 1 → 마우스가 정중앙 셀
	//  - 4x4: offset 1 → 마우스가 중앙 좌하단 셀
	// ⚠️ 입력 방향(cursor → origin). 시각 보정인 AOJJ_Grid::GetMachinePlacementLocation
	// (origin → footprint center 액터 위치)과 반대 방향이지만 같은 size 정수화 규칙
	// (CeilToInt + Max(1))을 따라야 호버/배치와 occupancy/시각이 어긋나지 않는다.
	FIntPoint ComputeOriginFromCursorCell(FIntPoint CursorCell, AMachineBase* Machine, int32 RotationSteps = 0) const;

	// cursor cell → lower-left origin의 size 기반 공통 수식 — 머신(ComputeOriginFromCursorCell이 위임)과
	// Foundation(F1-b)이 "마우스 = 풋프린트 중심" 정책을 공유. (Size-1)/2 정수 나눗셈 lower-left bias.
	static FIntPoint ComputeOriginFromCursorCellForSize(FIntPoint CursorCell, FIntPoint EffSize);

	TSubclassOf<AMachineBase> GetActiveMachineClass() const;

	// Foundation 모드의 활성 클래스(F3-2.5) — 종류 상태에 따라 Flat/Ramp 선택. 호버/배치가
	// 같은 함수를 쓰므로 "미리보기 = 실제 배치" 클래스 정합이 한 곳에서 보장된다.
	TSubclassOf<AOJJ_Foundation> GetActiveFoundationClass() const;

	// Foundation 모드 호버/배치(F1-b) — 머신 경로와 독립. CDO FoundationSize 풋프린트,
	// 판정은 그리드 CanPlaceFoundation 단일 진실원, spawn-validate-destroy 패턴(머신 배치 미러).
	void UpdateFoundationHover(FIntPoint CursorCell, const FHitResult& Hit);
	void PlaceFoundationAtCursor();

	// [#184] 사다리 모드 호버/배치 — Foundation 경로 미러(머신/그리드 장부 미경유, 자유 배치).
	// 커서 셀을 덮는 Foundation의 가장 가까운 변을 찾아 그 바깥 지면에 사다리 고스트/스폰. 높이 = 상면 − 지면.
	void UpdateLadderHover(FIntPoint CursorCell, const FHitResult& Hit);
	void PlaceLadderAtCursor();
	// 변 산출(호버=클릭 공용, 셀 단위 결정적). 유효 변이면 true + 바닥 월드위치/등반높이/내향 자세(Yaw) 산출.
	bool ComputeLadderPlacement(FIntPoint CursorCell, FVector& OutBottomLocation, float& OutClimbHeight, FRotator& OutRotation) const;

	// [F2-4 후속 ①] 배치 성공 직후 풋프린트 XY + 슬래브 높이 구간의 Pawn을 상면 위로 올림("깔면 올라탐").
	// 서버 권위 전용(배치와 같은 흐름), 모든 Pawn 대상(멀티 대비). 위 공간 막힘 감지는 백로그 — 일단 올리고 로그.
	// F3-2(㉳): 셀별 SurfaceZ는 그리드 등록 데이터가 진실원 — 배치 성공 후 호출 전제. 평판/램프 공용.
	void OJJ_LiftPawnsOntoFoundation(FIntPoint Origin, FIntPoint Size, float SlabThickness);

	// [F2-4 후속 ②] 빌드모드 Tick — 로컬 플레이어 캡슐이 걸친 셀 표시 갱신(셀 변경 시에만 그리드 호출).
	void UpdateCharacterCellOverlay();

	// 직전 표시 셀 캐시(틱마다 ISM 재빌드 방지). Foundation 배치 직후 Reset — 같은 셀이라도 비주얼 Z가
	// 상면으로 바뀌므로 강제 재적재 유도.
	TArray<FIntPoint> CharacterOverlayCells;

	// 마우스 커서 아래 그리드 셀 조회(라인 트레이스 → WorldToGrid). 실패 시 false.
	bool GetCursorCell(FIntPoint& OutCell) const;

	// #182 패럴랙스 보정 셀 변환 — 지형 히트 위치로 셀을 구하되, 그 셀이 물이면 커서 레이를 수면 Z 평면과
	// 교차시킨 XY로 재계산한다(WaterArea가 Visibility Ignore라 레이가 물 밑 지형을 맞아 생기는 호버-그리드
	// 어긋남 해소). 육지/Foundation은 기존 지형 히트 XY 그대로(회귀 0).
	FIntPoint ResolveCursorCellOverWater(const FVector& TerrainHitLocation) const;

	// 컨베이어 드래그 상태머신(Conveyor 모드 전용).
	void BeginConveyorDrag(FIntPoint StartCell);
	void UpdateConveyorDrag(FIntPoint CursorCell);
	void CommitConveyorDrag();
	void AppendConveyorPathTo(FIntPoint TargetCell);
	void AddConveyorPathCell(FIntPoint Cell);

	// 컨베이어 호버 갱신(드래그 중이면 drag, 아니면 단일 셀 미리보기). 파이프 모드도 공용(F4-1).
	void UpdateConveyorHover(FIntPoint CursorCell);

	// 경로 드래그 호버 디스패치(F4-1): 컨베이어/파이프가 드래그 상태머신을 공용하므로 프리뷰만 모드 분기.
	void UpdatePathDragHoverPreview(const TArray<FIntPoint>& Cells);
	AMachineBase* GetPowerLineEndpointUnderCursor() const;
	AMachineBase* FindPowerLineEndpointNearLocation(const FVector& Location) const;
	bool IsPowerLineEndpoint(const AMachineBase* Machine) const;
	void BeginPowerLineDrag(AMachineBase* StartMachine);
	void CommitPowerLineDrag();
};
