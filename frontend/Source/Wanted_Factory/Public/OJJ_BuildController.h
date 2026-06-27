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
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
struct FHitResult;

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
	None,
	BaseCamp,
	SignalAmplifier
};

// 빌드 뷰 모드 — 빌드모드의 "관점"을 구분(상태/입력 골격, 1단계). 카메라/좌표 분기는 2·3단계.
//   None    = 빌드모드 아님(기존 bIsBuildMode == false 와 동치)
//   TopDown = 탑다운 빌드(기존 B키 동작). 별도 빌드 카메라 + 마우스 커서 레이.
//   TPS     = 3인칭 빌드(신규 V키). 1단계에선 진입 골격만 — 카메라/좌표는 아직 TopDown처럼 동작.
// ⚠️ None을 0으로 고정(IsInBuildMode = (Mode != None)). 신규 값은 끝에 append(BP 직렬화 호환).
UENUM(BlueprintType)
enum class EBuildViewMode : uint8
{
	None,
	TopDown,
	TPS
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildController")
	TSubclassOf<AMachineBase> BaseCampClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildController")
	TSubclassOf<AMachineBase> SignalAmplifierClass;

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

	// 빌드 뷰 모드(단일 진실원). None=빌드 아님, TopDown=B, TPS=V. 기존 bIsBuildMode를 대체.
	// IsInBuildMode()는 (BuildViewMode != None)이라 기존 호출처 전부 무변경 호환.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BuildController")
	EBuildViewMode BuildViewMode = EBuildViewMode::None;

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

	// [공중 Foundation] 빌드 높이 오프셋(층 수, 1층=OJJ_FoundationSnapStep=100uu). Foundation(Flat)에서 증감 — TPS=Q/E, 탑다운=↑↓.
	// 회전/Foundation종류와 동일 라이프사이클 — Enter/ExitBuildMode에서만 0 리셋(모드 전환·램프 전환 시 유지=세션 기억).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BuildController")
	int32 BuildHeightOffset = 0;

public:
	UFUNCTION(BlueprintCallable, Category = "BuildController")
	void EnterBuildMode();

	UFUNCTION(BlueprintCallable, Category = "BuildController")
	void ExitBuildMode();

	UFUNCTION(BlueprintCallable, Category = "BuildController")
	bool IsInBuildMode() const { return BuildViewMode != EBuildViewMode::None; }

	// 현재 빌드 뷰 모드(None/TopDown/TPS). 플레이어 입력 라우팅·2단계 카메라 분기에서 사용.
	UFUNCTION(BlueprintPure, Category = "BuildController")
	EBuildViewMode GetBuildViewMode() const { return BuildViewMode; }

	// [경로형 2클릭 공통 골격] 연결형 배치(전선/컨베이어/파이프)의 입력 정책. TPS=2클릭(앵커→커밋), 그 외=드래그.
	// 마우스가 카메라 Look에 묶이는 TPS에선 드래그 스윕이 불가하므로 누름 2회로 처리. 전선부터 적용, 컨/파이프 확장 예정.
	UFUNCTION(BlueprintPure, Category = "BuildController")
	bool IsTwoClickConnectMode() const { return BuildViewMode == EBuildViewMode::TPS; }

	// 빌드 뷰 모드 전환의 절대 세터(단일 진입점). None=해제(ExitBuildMode), TopDown/TPS=진입 또는 전환.
	//  - None→TopDown/TPS : EnterBuildMode 경유 진입(검증 실패 시 None 유지). TPS는 진입 후 모드만 갱신.
	//  - TopDown↔TPS      : 1단계 골격 — 그리드 상태는 그대로 두고 모드 플래그만 교체.
	// 토글 규칙(같은 키 재입력 시 해제)은 호출자(플레이어)가 담당.
	UFUNCTION(BlueprintCallable, Category = "BuildController")
	void SetBuildViewMode(EBuildViewMode NewMode);

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

	// [그리드 색상 2단계] 현재 PlacementMode(+머신 CDO 지형규칙)를 그리드 색상 규칙으로 주입. EnterBuildMode 진입·
	// SetPlacementMode 전환 시 호출. 비머신 모드(Foundation/Conveyor/Pipe/PowerLine/Demolish/None)는 명시 분기.
	void UpdateGridColorForCurrentMode();

	UFUNCTION(BlueprintPure, Category = "BuildController")
	EOJJ_BuildPlacementMode GetPlacementMode() const { return PlacementMode; }

	// 진행 중인 컨베이어 드래그 취소(좌클릭 취소 트리거 / 모드 전환 시).
	UFUNCTION(BlueprintCallable, Category = "BuildController")
	void CancelConveyorDrag();

	UFUNCTION(BlueprintCallable, Category = "BuildController")
	void CancelPowerLineDrag();

	// [경로형 2클릭 공통 골격] 진행 중인 연결 앵커(전선 등)를 무른다. 무른 게 있으면 true(호출부가 모드 유지 판단).
	// TPS 2클릭 중 Z 취소가 모드까지 None으로 나가지 않고 앵커만 취소해 재시도하도록 함. 컨/파이프 확장 예정.
	UFUNCTION(BlueprintCallable, Category = "BuildController")
	bool CancelPendingConnectAnchor();

	// [공중 Foundation] 빌드 높이 오프셋 층수 증감(Q/E). Delta>0=올림/Delta<0=내림, clamp(min 0).
	// ⚠️ TPS Foundation(Flat)일 때만 동작(램프 제외). 1단계는 값 증감 + 로그만 — 고스트/스폰 Z 반영은 2단계.
	UFUNCTION(BlueprintCallable, Category = "BuildController")
	void AdjustBuildHeight(int32 Delta);

	// [공중 Foundation] 이 풋프린트(Origin/EffSize)에 적용할 높이 오프셋 Z(uu) = BuildHeightOffset×100.
	// 게이트: TPS && Flat && offset>0 일 때만. ⚠️(b) 이웃 Foundation 접촉(상속) 시 0 — coplanar 확장(씨앗 배치에만 높이 적용).
	// 고스트(슬래브/타일)·스폰이 모두 이 값을 써 프리뷰=배치 일치. FoundationCDO는 Thickness(이웃 판정 기준)용.
	float GetFoundationHeightLiftZ(const class AOJJ_Foundation* FoundationCDO, FIntPoint Origin, FIntPoint EffSize) const;

	// [#B 묻힘 금지] 파운데이션 상면 Z 단일원 — 호버/타일색/배치가 동일 값을 쓰도록(정합 보장).
	//   TopZ = GetFoundationPlacementLocation().Z + OJJ_ComputeSnapLift + GetFoundationHeightLiftZ + Thickness.
	// 배치의 BaseSurfaceZ 산식과 동일. CDO는 비-const(OJJ_ComputeSnapLift가 비-const virtual, 읽기전용 안전).
	float OJJ_ComputeFoundationTopZ(class AOJJ_Foundation* FoundationCDO, FIntPoint Origin, FIntPoint EffSize, int32 RotationSteps) const;

	// [#3 점진 건설] 신규 배치된 건물에 홀로그램 빌드업 효과를 부여한다(동적 컴포넌트 — 건물 클래스 무수정).
	// 배치 함수에서만 호출 → 신규 전용(로드 경로는 안 거침). HologramBuildUpMaterial 미지정이면 무동작(배치 정상).
	void StartBuildUpEffect(class AActor* Building, class UStaticMeshComponent* Mesh);

	// 빌드업 머티리얼(M_Hologram_BuildUp). 미지정이면 효과 비활성 — 에디터 제작 후 이 슬롯에 지정.
	UPROPERTY(EditAnywhere, Category = "BuildController|Hologram")
	TObjectPtr<class UMaterialInterface> HologramBuildUpMaterial;

	UPROPERTY(EditAnywhere, Category = "BuildController|Hologram", meta = (ClampMin = "0.01"))
	float HologramBuildUpDuration = 1.0f;

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

	// [TPS 3단계] 빌드 트레이스 소스 추상화 — 현재 빌드 뷰 모드에 따라 히트(FHitResult) 산출.
	//  - TopDown/None : 기존 마우스 커서 트레이스(GetHitResultUnderCursorByChannel).
	//  - TPS          : 화면 중앙(크로스헤어) deproject → 전방 LineTrace(ECC_Visibility) + 10m 도달거리 클램프.
	// 전체 FHitResult를 돌려주므로 downstream(액터/컴포넌트 판별·Foundation/Ladder/PowerLine)이 모드 무관하게 재사용.
	// 고스트(UpdateMouseHover)·설치/철거(GetCursorCell)·파워라인 픽이 동일 소스를 쓰도록 세 경로 모두 이 함수 경유.
	bool ResolveBuildTraceHit(APlayerController* PC, FHitResult& OutHit) const;

	// [TPS 3단계] 도달거리(플레이어~히트 최대, uu). 초과 시 도달거리 링 방향 지면으로 클램프. PIE에서 UX 튜닝.
	// (private 멤버라 BlueprintReadWrite 불가 — EditAnywhere만으로 Details 패널 튜닝 가능, BP 접근 불필요)
	UPROPERTY(EditAnywhere, Category = "BuildController|TPS", meta = (ClampMin = "0.0"))
	float BuildTPSMaxReach = 1000.f;

	// [TPS 3단계] 화면중앙 전방 트레이스 최대 길이(uu). 이 안에 표면 없으면 고스트 없음(탑다운 !bHit과 동일).
	UPROPERTY(EditAnywhere, Category = "BuildController|TPS", meta = (ClampMin = "0.0"))
	float BuildTPSTraceLength = 100000.f;

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

	// [TPS 2클릭 오토라우터] 앵커(ConveyorDragCells[0]) ↔ TargetCell 사이를 연속 L경로로 재구성한다.
	// L자 = 가로 먼저(x1→x2) then 세로(y1→y2). 4방향 인접 셀만 생성 → OJJ_BuildConveyorPlacementPath 연속성 요건 충족.
	// TPS 컨베이어 2클릭에서 매 호버마다 호출(앵커→조준 경로 미리보기). 검증/색상은 기존 프리뷰가 처리.
	void OJJ_BuildConveyorAutoRoute(FIntPoint TargetCell);

	// 컨베이어 호버 갱신(드래그 중이면 drag, 아니면 단일 셀 미리보기). 파이프 모드도 공용(F4-1).
	void UpdateConveyorHover(FIntPoint CursorCell);

	// 경로 드래그 호버 디스패치(F4-1): 컨베이어/파이프가 드래그 상태머신을 공용하므로 프리뷰만 모드 분기.
	void UpdatePathDragHoverPreview(const TArray<FIntPoint>& Cells);
	AMachineBase* GetPowerLineEndpointUnderCursor() const;
	AMachineBase* FindPowerLineEndpointNearLocation(const FVector& Location) const;
	bool IsPowerLineEndpoint(const AMachineBase* Machine) const;
	void BeginPowerLineDrag(AMachineBase* StartMachine);
	void CommitPowerLineDrag();

	// [#5 전선 미리보기] 드래그 중 앵커↔조준을 실제 메시(실린더 세그먼트 + 동적 머티리얼 3색)로 그린다.
	// DrawDebugLine과 달리 출시 빌드에서도 보인다. sag/세그먼트/끝점Z 공식은 APowerLine 룩을 읽어 OJJ에
	// 재구현(Chan 코드 무수정·복사). 색: 노랑=드래그(타겟없음)/초록=연결가능/빨강=무효·범위밖.
	void UpdatePowerLinePreview(const FHitResult& Hit);
	// 미리보기 세그먼트 전부 숨김(드래그 취소/커밋/빌드해제 시). 매 프레임 spawn/destroy 금지 — 풀 재사용.
	void HidePowerLinePreview();
	// 미리보기 동적 머티리얼(M_Ghost_Preview 기반) 멱등 보장. 실패 시 PowerLinePreviewMID는 null 유지.
	void EnsurePowerLinePreviewMID();
	// APowerLine::GetSagPoint 공식 복제 — Lerp 후 Z를 SagDepth·4α(1-α)만큼 내림(포물선 처짐).
	static FVector ComputePowerLineSagPoint(const FVector& Start, const FVector& End, float Alpha, float SagDepth);

	// [#5 전선 미리보기] 세그먼트 메시 풀(재사용 — 매 프레임 생성/파괴 금지). 길이 따라 가변, 남는 건 SetVisibility(false).
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PowerLinePreviewSegments;

	// 세그먼트에 입힐 동적 머티리얼(색 3단계 TintColor). M_Ghost_Preview 기반, 런타임 생성.
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PowerLinePreviewMID;

	// 미리보기 머티리얼 베이스(기본 M_Ghost_Preview, 생성자 로드 — 에디터 재지정 가능). TintColor/Opacity 파라미터.
	UPROPERTY(EditAnywhere, Category = "BuildController|Power|Preview")
	TObjectPtr<UMaterialInterface> PowerLinePreviewMaterial;

	// 미리보기 실린더 메시(기본 /Engine/BasicShapes/Cylinder, 생성자 로드).
	UPROPERTY(EditAnywhere, Category = "BuildController|Power|Preview")
	TObjectPtr<UStaticMesh> PowerLinePreviewMesh;

	UPROPERTY(EditAnywhere, Category = "BuildController|Power|Preview")
	FLinearColor PowerLinePreviewColorDragging = FLinearColor(1.0f, 0.85f, 0.1f);   // 노랑: 드래그 중(타겟 없음)

	UPROPERTY(EditAnywhere, Category = "BuildController|Power|Preview")
	FLinearColor PowerLinePreviewColorValid = FLinearColor(0.15f, 0.9f, 0.25f);     // 초록: 연결 가능

	UPROPERTY(EditAnywhere, Category = "BuildController|Power|Preview")
	FLinearColor PowerLinePreviewColorInvalid = FLinearColor(0.95f, 0.15f, 0.15f);  // 빨강: 무효·범위밖

	UPROPERTY(EditAnywhere, Category = "BuildController|Power|Preview", meta = (ClampMin = "0.1"))
	float PowerLinePreviewThickness = 4.0f;

	UPROPERTY(EditAnywhere, Category = "BuildController|Power|Preview", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PowerLinePreviewOpacity = 0.6f;
};
