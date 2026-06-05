// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OJJ_BuildController.generated.h"

class AOJJ_Grid;
class AMachineBase;
class AConveyor;
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
	Pump
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
	TSubclassOf<AMachineBase> GetActiveMachineClass() const;

	// 마우스 커서 아래 그리드 셀 조회(라인 트레이스 → WorldToGrid). 실패 시 false.
	bool GetCursorCell(FIntPoint& OutCell) const;

	// 컨베이어 드래그 상태머신(Conveyor 모드 전용).
	void BeginConveyorDrag(FIntPoint StartCell);
	void UpdateConveyorDrag(FIntPoint CursorCell);
	void CommitConveyorDrag();
	void AppendConveyorPathTo(FIntPoint TargetCell);
	void AddConveyorPathCell(FIntPoint Cell);

	// 컨베이어 호버 갱신(드래그 중이면 drag, 아니면 단일 셀 미리보기).
	void UpdateConveyorHover(FIntPoint CursorCell);
	AMachineBase* GetPowerLineEndpointUnderCursor() const;
	AMachineBase* FindPowerLineEndpointNearLocation(const FVector& Location) const;
	bool IsPowerLineEndpoint(const AMachineBase* Machine) const;
	void BeginPowerLineDrag(AMachineBase* StartMachine);
	void CommitPowerLineDrag();
};
