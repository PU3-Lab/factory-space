// Fill out your copyright notice in the Description page of Project Settings.


#include "OJJ_BuildController.h"

#include "DrawDebugHelpers.h"
#include "Engine/GameInstance.h"
#include "Engine/HitResult.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "FactoryManagerSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "LandscapeProxy.h"
#include "MachineBase.h"
#include "OJJ_Grid.h"
#include "Conveyor.h"
#include "Machines/PowerGridNode.h"
#include "Machines/PowerLine.h"
#include "Machines/PowerPlant.h"
#include "Machines/Grinder.h"
#include "Machines/MachineSubsystem.h"
#include "Machines/MinerMachine.h"
#include "Machines/Pump.h"
#include "Machines/Smelter.h"
#include "Machines/WarehousePort.h"
#include "OJJ_Foundation.h"
#include "OJJ_ProtectionTower.h"
#include "QuestManagerSubsystem.h"
#include "Resource/ResourceBase.h"

namespace
{
	void ApplyMachineDataToDefault(UObject* Context, AMachineBase* DefaultMachine)
	{
		if (!Context || !DefaultMachine)
		{
			return;
		}

		UGameInstance* GameInstance = Context->GetWorld()
			? Context->GetWorld()->GetGameInstance()
			: nullptr;
		if (!GameInstance)
		{
			return;
		}

		UMachineSubsystem* MachineSubsystem = GameInstance->GetSubsystem<UMachineSubsystem>();
		if (!MachineSubsystem)
		{
			return;
		}

		FMachineTableRow MachineData;
		if (MachineSubsystem->FindMachineData(DefaultMachine->GetMachineType(), MachineData))
		{
			DefaultMachine->ApplyMachineData(MachineData);
		}
	}

	FName GetQuestPlacementTargetId(EOJJ_BuildPlacementMode PlacementMode)
	{
		switch (PlacementMode)
		{
		case EOJJ_BuildPlacementMode::Miner:
			return TEXT("MinerMachine");
		case EOJJ_BuildPlacementMode::PowerPlant:
			return TEXT("PowerPlant");
		case EOJJ_BuildPlacementMode::PowerNode:
			return TEXT("PowerGridNode");
		case EOJJ_BuildPlacementMode::Smelter:
			return TEXT("Smelter");
		case EOJJ_BuildPlacementMode::Warehouse:
			return TEXT("WarehousePort");
		case EOJJ_BuildPlacementMode::Conveyor:
			return TEXT("Conveyor");
		default:
			return NAME_None;
		}
	}

	void NotifyMainQuestMachinePlaced(UObject* Context, FName MachineType)
	{
		if (!Context || MachineType.IsNone())
		{
			return;
		}

		UGameInstance* GameInstance = Context->GetWorld()
			? Context->GetWorld()->GetGameInstance()
			: nullptr;
		if (!GameInstance)
		{
			return;
		}

		if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
		{
			QuestManager->NotifyMainQuestMachinePlaced(MachineType);
		}
	}
}

AOJJ_BuildController::AOJJ_BuildController()
{
	// 빌드모드 동안만 호버를 갱신하면 되므로 Tick은 켜두되 기본 비활성.
	// Enter/ExitBuildMode에서 SetActorTickEnabled로 on/off → 빌드모드 밖 0비용.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 컨베이어 모드 기본 클래스(BP 미지정 시). Dummy와 동일 패턴.
	ConveyorClass = AConveyor::StaticClass();
	PowerLineClass = APowerLine::StaticClass();
	PowerGridNodeClass = APowerGridNode::StaticClass();
	ShieldClass = AOJJ_ProtectionTower::StaticClass();
	PowerPlantClass = APowerPlant::StaticClass();
	GrinderClass = AGrinder::StaticClass();
	MinerClass = AMinerMachine::StaticClass();
	PumpClass = APump::StaticClass();
	SmelterClass = ASmelter::StaticClass();
	WarehouseClass = AWarehousePort::StaticClass();
}

void AOJJ_BuildController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Enter/Exit에서 Tick을 on/off하지만, 방어적으로 모드 가드도 유지(UpdateMouseHover 내부에도 가드 있음).
	if (bIsBuildMode)
	{
		UpdateMouseHover();
	}
}

void AOJJ_BuildController::EnterBuildMode()
{
	if (bIsBuildMode)
	{
		return;
	}

	if (!TargetGrid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] TargetGrid 미설정 — EnterBuildMode 중단"));
		return;
	}

	// 모드별 클래스 미설정 가드 — 머신 모드는 MachineClass, 컨베이어 모드는 ConveyorClass 필요.
	if (PlacementMode == EOJJ_BuildPlacementMode::Machine && !MachineClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] MachineClass 미설정 — EnterBuildMode 중단"));
		return;
	}
	if (PlacementMode == EOJJ_BuildPlacementMode::PowerNode && !PowerGridNodeClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] PowerGridNodeClass missing. EnterBuildMode stopped."));
		return;
	}
	if (PlacementMode == EOJJ_BuildPlacementMode::Shield && !ShieldClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] ShieldClass missing. EnterBuildMode stopped."));
		return;
	}
	if (PlacementMode == EOJJ_BuildPlacementMode::PowerPlant && !PowerPlantClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] PowerPlantClass missing. EnterBuildMode stopped."));
		return;
	}
	if (PlacementMode == EOJJ_BuildPlacementMode::Grinder && !GrinderClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] GrinderClass missing. EnterBuildMode stopped."));
		return;
	}
	if (PlacementMode == EOJJ_BuildPlacementMode::Miner && !MinerClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] MinerClass missing. EnterBuildMode stopped."));
		return;
	}
	if (PlacementMode == EOJJ_BuildPlacementMode::Pump && !PumpClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] PumpClass missing. EnterBuildMode stopped."));
		return;
	}
	if (PlacementMode == EOJJ_BuildPlacementMode::Smelter && !SmelterClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] SmelterClass missing. EnterBuildMode stopped."));
		return;
	}
	if (PlacementMode == EOJJ_BuildPlacementMode::Conveyor && !ConveyorClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] ConveyorClass 미설정 — EnterBuildMode 중단"));
		return;
	}
	if (PlacementMode == EOJJ_BuildPlacementMode::PowerLine && !PowerLineClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] PowerLineClass missing. EnterBuildMode stopped."));
		return;
	}

	TargetGrid->SetVisualizationVisible(true);

	// 진입 즉시 배치 머신 포트 화살표 표시(첫 호버 전이라도 보이도록). 호버 화살표는 첫 UpdateMouseHover에서.
	TargetGrid->RefreshPlacedMachineArrows();

	bIsBuildMode = true;

	// 빌드 세션은 항상 회전 0(미회전)으로 시작 — 예측 가능한 기본 방향.
	HoverRotationSteps = 0;

	// 컨베이어 드래그 상태 초기화(이전 세션 잔여 방지).
	bIsDraggingConveyor = false;
	ConveyorDragCells.Reset();
	bIsDraggingPowerLine = false;
	PowerLineStartMachine.Reset();

	// 빌드모드 동안에만 호버 Tick 가동
	SetActorTickEnabled(true);

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->bShowMouseCursor = true;
		PC->bEnableClickEvents = true;
		PC->bEnableMouseOverEvents = true;
	}

	// 첫 UpdateMouseHover 호출이 무조건 갱신을 트리거하도록 sentinel로 초기화
	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
}

void AOJJ_BuildController::ExitBuildMode()
{
	if (!bIsBuildMode)
	{
		return;
	}

	if (TargetGrid)
	{
		TargetGrid->SetVisualizationVisible(false);
		TargetGrid->ClearHoverPreview();         // 호버 셀 + 호버 화살표 제거
		TargetGrid->ClearPlacedMachineArrows();  // 배치 머신 화살표 제거 (진입 RefreshPlacedMachineArrows와 대칭)
	}

	bIsBuildMode = false;

	// 컨베이어 드래그 상태 정리.
	bIsDraggingConveyor = false;
	ConveyorDragCells.Reset();
	bIsDraggingPowerLine = false;
	PowerLineStartMachine.Reset();

	// 호버 Tick 정지 (빌드모드 밖 0비용)
	SetActorTickEnabled(false);

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->bShowMouseCursor = false;
	}

	// 재진입 시 같은 셀에 정지해 있어도 첫 갱신이 동작하도록 sentinel로 리셋
	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);

	// 회전 상태도 리셋 — 다음 진입은 미회전(0)으로 시작(EnterBuildMode 초기화와 일관).
	HoverRotationSteps = 0;

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFactoryManagerSubsystem* FactoryManager = GameInstance->GetSubsystem<UFactoryManagerSubsystem>())
		{
			FactoryManager->UpdatePowerGrid();
		}
	}
}

void AOJJ_BuildController::ToggleBuildMode()
{
	if (bIsBuildMode)
	{
		ExitBuildMode();
	}
	else
	{
		EnterBuildMode();
	}
}

void AOJJ_BuildController::RotateHoverClockwise()
{
	// R은 IMC_Build 전용이라 빌드모드에서만 발동하지만, 방어적으로 가드.
	// 회전은 머신 호버 전용 — 컨베이어 모드에서는 무시(Dummy parity).
	if (!bIsBuildMode
		|| (PlacementMode != EOJJ_BuildPlacementMode::Machine
			&& PlacementMode != EOJJ_BuildPlacementMode::PowerNode
			&& PlacementMode != EOJJ_BuildPlacementMode::Shield
			&& PlacementMode != EOJJ_BuildPlacementMode::PowerPlant
			&& PlacementMode != EOJJ_BuildPlacementMode::Grinder
			&& PlacementMode != EOJJ_BuildPlacementMode::Miner
			&& PlacementMode != EOJJ_BuildPlacementMode::Pump
			&& PlacementMode != EOJJ_BuildPlacementMode::Smelter
			&& PlacementMode != EOJJ_BuildPlacementMode::Warehouse))
	{
		return;
	}

	HoverRotationSteps = (HoverRotationSteps + 1) % 4;

	// 마우스가 같은 셀에 멈춰 있어도 회전이 즉시 미리보기에 반영되도록 sentinel 리셋 후 강제 갱신.
	// (UpdateMouseHover는 CursorCell==CurrentHoverCell이면 rebuild를 스킵하므로 sentinel이 필요.)
	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
	UpdateMouseHover();
}

FIntPoint AOJJ_BuildController::ComputeOriginFromCursorCell(FIntPoint CursorCell, AMachineBase* Machine, int32 RotationSteps) const
{
	if (!Machine)
	{
		return CursorCell;
	}

	// AOJJ_Grid::CalculateFootprint / GetMachinePlacementLocation과 동일한 정수화·회전 규칙(EffectiveSize).
	// 입력(cursor → origin)과 시각 보정(origin → footprint center)이 반대 방향이지만
	// 같은 size 가정에서 동작해야 호버/배치와 occupancy/메시 위치가 어긋나지 않음. step 0이면 기존과 동일.
	const FIntPoint Size = AOJJ_Grid::EffectiveSize(Machine->GetMachineSize(), RotationSteps);

	return ComputeOriginFromCursorCellForSize(CursorCell, Size);
}

FIntPoint AOJJ_BuildController::ComputeOriginFromCursorCellForSize(FIntPoint CursorCell, FIntPoint EffSize)
{
	// (Size-1)/2 정수 나눗셈 → lower-left bias. 1x1 offset 0 (회귀 없음). 머신/Foundation 공통 수식 —
	// 두 경로의 "마우스 = 풋프린트 중심" 정책이 갈라지지 않게 단일원으로 유지.
	return FIntPoint(CursorCell.X - (EffSize.X - 1) / 2, CursorCell.Y - (EffSize.Y - 1) / 2);
}

TSubclassOf<AMachineBase> AOJJ_BuildController::GetActiveMachineClass() const
{
	if (PlacementMode == EOJJ_BuildPlacementMode::PowerNode)
	{
		return PowerGridNodeClass;
	}

	if (PlacementMode == EOJJ_BuildPlacementMode::Shield)
	{
		return ShieldClass;
	}

	if (PlacementMode == EOJJ_BuildPlacementMode::PowerPlant)
	{
		return PowerPlantClass;
	}

	if (PlacementMode == EOJJ_BuildPlacementMode::Grinder)
	{
		return GrinderClass;
	}

	if (PlacementMode == EOJJ_BuildPlacementMode::Miner)
	{
		return MinerClass;
	}

	if (PlacementMode == EOJJ_BuildPlacementMode::Pump)
	{
		return PumpClass;
	}

	if (PlacementMode == EOJJ_BuildPlacementMode::Smelter)
	{
		return SmelterClass;
	}

	if (PlacementMode == EOJJ_BuildPlacementMode::Warehouse)
	{
		return WarehouseClass;
	}

	return MachineClass;
}

void AOJJ_BuildController::UpdateMouseHover()
{
	if (!bIsBuildMode)
	{
		return;
	}

	if (!TargetGrid)
	{
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return;
	}

	// === Demolish 모드 — 커서 대상 빨강 하이라이트(배치 트레이스/풋프린트 경로와 분리) ===
	if (PlacementMode == EOJJ_BuildPlacementMode::Demolish)
	{
		UpdateDemolishHover();
		return;
	}

	FHitResult Hit;
	const bool bHit = PC->GetHitResultUnderCursorByChannel(
		UEngineTypes::ConvertToTraceType(ECC_Visibility),
		/*bTraceComplex=*/ false,
		Hit);

	if (!bHit)
	{
		// 트레이스 실패 → stale 미리보기/캐시가 다음 클릭에 잘못 적용되지 않도록 명시적 리셋
		TargetGrid->ClearHoverPreview();
		CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
		return;
	}

	const FIntPoint CursorCell = TargetGrid->WorldToGrid(Hit.Location);

	// Conveyor 모드: 드래그/단일 셀 미리보기로 분기 (머신 경로와 독립).
	if (PlacementMode == EOJJ_BuildPlacementMode::Conveyor)
	{
		UpdateConveyorHover(CursorCell);
		return;
	}

	if (PlacementMode == EOJJ_BuildPlacementMode::PowerLine)
	{
		TargetGrid->ClearHoverPreview();
		CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);

#if ENABLE_DRAW_DEBUG
		// 전선 드래그 미리보기 — 읽기 전용 시각화(팀원 연결 로직 비침범, 호버는 BuildController 영역).
		// Shipping 등 ENABLE_DRAW_DEBUG=0 빌드에선 블록 전체가 컴파일 아웃 → 런타임 비용 0.
		// 여기 도달 시점엔 위 (!bHit) 가드를 이미 통과한 상태이므로 Hit / Hit.Location 유효.
		if (bIsDraggingPowerLine)
		{
			if (UWorld* World = GetWorld())
			{
				if (AMachineBase* StartMachine = PowerLineStartMachine.Get())
				{
					// 완성선(APowerLine::LineHeightOffset 기본값 350)과 높이를 맞춤. LineHeightOffset이
					// protected·게터 없음 → 상수 사용. 팀원이 그 기본값을 바꾸면 여기도 동기화 필요.
					constexpr float PreviewEndpointHeightOffset = 20.0f;
					const FVector StartLoc = APowerLine::GetEndpointLocationForActor(StartMachine, PreviewEndpointHeightOffset);

					// 시작 노드(StartLoc) → 커서(CursorLoc)로 미리보기 선(매 프레임 비영속).
					// 커서 아래 노드가 연결 가능하면 초록, 아니면 빨강. HoverNode가 non-null일 때만
					// CanConnect 평가(단락 평가) → 노드 위가 아니면 매 프레임 그래프 순회 비용 없음.
					AMachineBase* HoverMachine = Cast<AMachineBase>(Hit.GetActor());
					if (!IsPowerLineEndpoint(HoverMachine))
					{
						HoverMachine = FindPowerLineEndpointNearLocation(Hit.Location);
					}
					const FVector CursorLoc = HoverMachine
						? APowerLine::GetEndpointLocationForActor(HoverMachine, PreviewEndpointHeightOffset)
						: Hit.Location + FVector(0.0f, 0.0f, PreviewEndpointHeightOffset);
					UGameInstance* GameInstance = GetGameInstance();
					UFactoryManagerSubsystem* FactoryManager = GameInstance
						? GameInstance->GetSubsystem<UFactoryManagerSubsystem>()
						: nullptr;
					const bool bCanConnect = HoverMachine && FactoryManager
						&& FactoryManager->CanConnectPowerLineEndpoints(StartMachine, HoverMachine);
					DrawDebugLine(World, StartLoc, CursorLoc,
						bCanConnect ? FColor::Green : FColor::Red, /*bPersistent=*/ false, /*LifeTime=*/ -1.0f, 0, 4.0f);
				}
			}
		}
		// [옵션·미구현] 비드래그 상태에서 커서 아래 노드를 스피어로 강조하면 "선택 가능" 힌트가 되지만,
		// 요청 범위(드래그 중 피드백)를 넘어 생략. 필요 시 위 if 바깥에 HoverNode 강조를 추가.
#endif
		return;
	}

	// Foundation 모드(F1-b): 머신 경로와 독립 분기(Conveyor/Demolish 패턴) — CDO FoundationSize 풋프린트 호버.
	if (PlacementMode == EOJJ_BuildPlacementMode::Foundation)
	{
		UpdateFoundationHover(CursorCell, Hit);
		return;
	}

	// === Machine 모드 (기존 동작 무변경) ===
	TSubclassOf<AMachineBase> ActiveMachineClass = GetActiveMachineClass();
	if (!ActiveMachineClass)
	{
		return;
	}

	// floor 또는 이미 배치된 머신 위에서 hover를 유지.
	// 머신 Cube mesh가 Visibility 채널을 Block해서 trace를 가로채도, 머신 위 XY는
	// 점유된 셀에 정확히 매핑되므로 CanPlaceMachine 검증을 거치게 그대로 통과시킨다
	// → 점유 셀과 겹친 풋프린트가 빨강으로 표시됨. 그 외 표면(캐릭터/벽 등)은
	// off-grid이므로 기존처럼 ClearHoverPreview로 차단.
	UPrimitiveComponent* HitComp = Hit.GetComponent();
	AActor* HitActor = Hit.GetActor();
	const bool bHitFloor = (HitComp == TargetGrid->GetGridFloorMesh());
	const bool bHitMachine = HitActor && HitActor->IsA<AMachineBase>();
	// F1-c: Foundation 슬래브 상면(Visibility Block)도 유효 호버 표면 — 없으면 Foundation 위 머신 배치 불가.
	const bool bHitFoundation = HitActor && HitActor->IsA<AOJJ_Foundation>();
	// F2-1' 사각지대 해소: 평면 위(+델타) 지형은 Landscape가 커서 플레인보다 먼저 히트 — buildable 셀인데
	// 호버 사망. 셀 매핑은 WorldToGrid가 XY만 쓰므로(Z 무관) 플로어 히트와 동일하게 통과시킨다.
	// 배치 가능 여부는 기존처럼 CanPlace 경로가 판정(게이트는 표면 식별만).
	const bool bHitLandscape = HitActor && HitActor->IsA<ALandscapeProxy>();
	if (!bHitFloor && !bHitMachine && !bHitFoundation && !bHitLandscape)
	{
		TargetGrid->ClearHoverPreview();
		CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
		return;
	}

	// Tick마다 호출되는 경로라 동일 셀이면 ISM 리빌드 스킵 (CursorCell은 위에서 계산됨)
	if (CursorCell == CurrentHoverCell)
	{
		return;
	}

	AMachineBase* DefaultMachine = ActiveMachineClass.GetDefaultObject();
	if (!DefaultMachine)
	{
		return;
	}
	ApplyMachineDataToDefault(this, DefaultMachine);

	// cursor cell → lower-left origin (마우스 = 풋프린트 중심 정책).
	// 예전엔 IsValidGridCell(cursor)로 anchor 음수/초과를 사전 차단했으나, 이 차단이
	// 왼쪽/위 경계 비대칭을 만들었음 (오른쪽/아래는 anchor가 valid한 상태에서 풋프린트가
	// +X,+Y로 새서 빨강 표시되는데, 왼쪽/위는 anchor 자체가 음수가 되어 hover 사라짐).
	// 이제 origin이 그리드 음수/초과여도 그대로 넘김 → CanPlaceMachine이 풋프린트 셀별
	// IsValidGridCell 검사로 false → UpdateHoverPreview가 풋프린트 전체 빨강 (대칭).
	const FIntPoint Origin = ComputeOriginFromCursorCell(CursorCell, DefaultMachine, HoverRotationSteps);

	TargetGrid->UpdateHoverPreview(DefaultMachine, Origin, HoverRotationSteps);
	CurrentHoverCell = CursorCell;
}

void AOJJ_BuildController::UpdateDemolishHover()
{
	if (!TargetGrid)
	{
		return;
	}

	FIntPoint CursorCell;
	if (!GetCursorCell(CursorCell))
	{
		// 커서가 그리드 밖 → 하이라이트 제거.
		TargetGrid->ClearHoverPreview();
		CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
		return;
	}

	// 동일 셀이면 리빌드 스킵(Tick 경로). 철거 직후엔 DemolishUnderCursor가 sentinel을 리셋해 강제 갱신.
	if (CursorCell == CurrentHoverCell)
	{
		return;
	}
	CurrentHoverCell = CursorCell;

	AActor* Target = TargetGrid->GetActorAtCell(CursorCell);

	// F1-b': 점유(머신/컨베이어)가 없는 셀은 Foundation 역조회 — 건물이 있으면 건물 우선(위→아래 철거 순서 유도).
	if (!Target)
	{
		Target = TargetGrid->GetFoundationAtCell(CursorCell);
	}

	// 빈 셀 또는 맵 고정물(광맥/WaterArea = AResourceBase)은 철거 대상 아님 → 하이라이트 없음.
	if (!Target || Target->IsA<AResourceBase>())
	{
		TargetGrid->ClearHoverPreview();
		return;
	}

	// 머신/컨베이어는 점유 맵, Foundation은 커버리지 맵 — 어느 쪽이든 대상 셀 전체 빨강.
	const TArray<FIntPoint>* Cells = TargetGrid->GetActorCells(Target);
	if (!Cells)
	{
		// F2-0(Codex F1-b' #4): 위 건물이 있는 Foundation은 클릭(RemoveFoundation)이 거부하므로 호버도
		// 표시 생략 — 단일 진실원(호버 = 클릭 판정)을 철거 모드에도 적용. 거부 사유 화면 표시는 UI 백로그.
		if (TargetGrid->OJJ_CountOccupiedFoundationCells(Target) > 0)
		{
			TargetGrid->ClearHoverPreview();
			return;
		}
		Cells = TargetGrid->GetFoundationCells(Target);
	}
	if (Cells)
	{
		TargetGrid->OJJ_HighlightCellsInvalid(*Cells);
	}
	else
	{
		TargetGrid->ClearHoverPreview();
	}
}

void AOJJ_BuildController::DemolishUnderCursor()
{
	if (!TargetGrid)
	{
		return;
	}

	FIntPoint CursorCell;
	if (!GetCursorCell(CursorCell))
	{
		return; // 그리드 밖 클릭 무시.
	}

	AActor* Target = TargetGrid->GetActorAtCell(CursorCell);
	// F1-b': 점유가 없으면 Foundation 역조회 — 호버(UpdateDemolishHover)와 동일 우선순위.
	if (!Target)
	{
		Target = TargetGrid->GetFoundationAtCell(CursorCell);
	}
	if (!Target)
	{
		return; // 빈 셀 무시.
	}

	// 광맥/WaterArea(AResourceBase)는 맵 고정물 — 철거 금지.
	if (Target->IsA<AResourceBase>())
	{
		UE_LOG(LogTemp, Log, TEXT("[BuildController] 광맥/Water(AResourceBase)는 철거 대상이 아님 — 무시. Cell=%s"),
			*CursorCell.ToString());
		return;
	}

	bool bRemoved = false;

	if (AMachineBase* Machine = Cast<AMachineBase>(Target))
	{
		// 1) 이 머신을 끝점(Source/Target)으로 갖는 컨베이어 라인을 먼저 삭제(고아 방지). 두-머신 전제상 한쪽이
		//    사라지면 라인은 존재 조건을 잃는다. 컨베이어는 라인 단위(1액터=다중셀)라 점유 셀 하나로
		//    OJJ_RemoveActorAt 호출 시 라인 전체가 정리되고, 내부 UnregisterConveyor(그래프 엣지 제거) +
		//    RefreshPlacedMachineArrows로 반대편(살아있는) 머신의 빈 포트 화살표가 복귀한다.
		for (AConveyor* Conveyor : CollectConveyorsConnectedToMachine(Machine))
		{
			if (!Conveyor)
			{
				continue;
			}
			if (const TArray<FIntPoint>* ConvCells = TargetGrid->GetActorCells(Conveyor))
			{
				if (ConvCells->Num() > 0)
				{
					TargetGrid->OJJ_RemoveActorAt((*ConvCells)[0]); // 라인 전체 그리드 점유 해제.
				}
			}
			Conveyor->Destroy(); // 액터/비주얼 실제 제거 — 그리드 함수는 점유 해제만, Destroy는 호출자 책임(기존 854 패턴).
		}

		// 2) 머신 본체: RemoveMachineAt → RemoveMachine → OnRemovedFromGrid 훅(자원 Release/Claim 정리) +
		//    FactoryManager Unregister + 화살표 재적재. 그 후 액터 Destroy.
		if (TargetGrid->RemoveMachineAt(CursorCell))
		{
			Machine->Destroy();
			bRemoved = true;
		}
	}
	else if (AConveyor* Conveyor = Cast<AConveyor>(Target))
	{
		// 컨베이어 직접 철거: 라인 단위(액터 다중셀) 전체 그리드 해제 + Destroy. 반대편 머신 화살표는 내부 RefreshArrows로 복귀.
		if (TargetGrid->OJJ_RemoveActorAt(CursorCell))
		{
			Conveyor->Destroy();
			bRemoved = true;
		}
	}
	else if (AOJJ_Foundation* Foundation = Cast<AOJJ_Foundation>(Target))
	{
		// Foundation 철거(F1-b'): RemoveFoundation이 커버 셀 위 건물(점유)을 검사해 거부 + 사유 반환 —
		// 성공 시에만 Destroy(머신의 RemoveMachineAt→Destroy 순서와 동일). Destroy 후 EndPlay의
		// RemoveFoundation 재호출은 "not registered"로 끝나 이중 해제 안전(EndPlay 주석의 대칭 계약).
		FString OutReason;
		if (TargetGrid->RemoveFoundation(Foundation, OutReason))
		{
			Foundation->Destroy();
			bRemoved = true;
		}
		else
		{
			// 거부 사유 표시 — 배치 거부(TryPlaceFoundation 실패)와 동일 채널(로그). 예: 위 건물 N셀.
			UE_LOG(LogTemp, Warning, TEXT("[BuildController] Foundation 철거 거부: %s"), *OutReason);
		}
	}

	if (bRemoved)
	{
		// 연속 철거: 셀이 비었으니 호버 즉시 갱신(sentinel 리셋 → 다음 UpdateMouseHover에서 빈 셀로 리빌드).
		CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
		UpdateMouseHover();
	}
}

TArray<AConveyor*> AOJJ_BuildController::CollectConveyorsConnectedToMachine(AMachineBase* Machine) const
{
	TArray<AConveyor*> Result;
	if (!TargetGrid || !Machine)
	{
		return Result;
	}

	const TArray<FIntPoint>* MachineCells = TargetGrid->GetMachineCells(Machine);
	if (!MachineCells || MachineCells->Num() == 0)
	{
		return Result;
	}

	const TSet<FIntPoint> Footprint(*MachineCells);
	static const FIntPoint Dirs[] = {
		FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1)
	};

	// footprint 전 둘레의 4방향 인접 셀을 스캔(2x2/3x3는 면이 여러 셀 — 포트 셀만이 아니라 둘레 전체).
	TSet<AConveyor*> Seen;
	int32 AdjacentConveyorCount = 0;
	for (const FIntPoint& Cell : Footprint)
	{
		for (const FIntPoint& Dir : Dirs)
		{
			const FIntPoint Neighbor = Cell + Dir;
			if (Footprint.Contains(Neighbor))
			{
				continue;
			}
			AConveyor* Conveyor = TargetGrid->OJJ_GetConveyorAtCell(Neighbor);
			if (!Conveyor || Seen.Contains(Conveyor))
			{
				continue;
			}
			Seen.Add(Conveyor);
			++AdjacentConveyorCount;

			// 검증: 실제로 이 머신을 끝점으로 갖는 라인만 삭제(나란히 붙은 다른 머신의 라인 오삭제 방지).
			if (Conveyor->GetSourceMachine() == Machine || Conveyor->GetTargetMachine() == Machine)
			{
				Result.Add(Conveyor);
			}
		}
	}

	// (보험) 인접에 컨베이어가 있었으나 이 머신과 연결된 것이 0개 → 나란한 라인이면 정상, 아니면 "연결 기록 vs 실제
	// 인접" 불일치(규칙 위반 데이터)의 조기 신호. 그리드만으로의 근사 검출(FactoryManager 그래프 무조회).
	if (Result.Num() == 0 && AdjacentConveyorCount > 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BuildController] 철거 머신 인접 컨베이어 %d개 — Source/Target 연결 일치 0. 나란한 라인이면 정상, 아니면 연결 데이터 불일치 의심. Machine=%s"),
			AdjacentConveyorCount, *Machine->GetName());
	}

	return Result;
}

void AOJJ_BuildController::OnLeftClickPressed()
{
	// SP-only contract 강제 (헤더의 MULTIPLAYER LIMITATION 명시와 일치).
	// 클라이언트에서 호출되면 TryPlaceMachine의 HasAuthority ensure가 트리거되고
	// spawn된 머신은 orphan으로 남음 → 진입부에서 차단.
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BuildController] OnLeftClickPressed called on non-authority — SP-only contract"));
		return;
	}

	if (!bIsBuildMode)
	{
		return;
	}

	// Conveyor 모드: 좌클릭 누름 = 드래그 시작. (커밋은 OnLeftClickReleased.)
	if (PlacementMode == EOJJ_BuildPlacementMode::Conveyor)
	{
		FIntPoint CursorCell;
		if (GetCursorCell(CursorCell))
		{
			BeginConveyorDrag(CursorCell);
		}
		return;
	}

	if (PlacementMode == EOJJ_BuildPlacementMode::PowerLine)
	{
		BeginPowerLineDrag(GetPowerLineEndpointUnderCursor());
		return;
	}

	// Demolish 모드: 좌클릭 = 커서 셀 대상 제거(머신/컨베이어/Foundation). 배치 경로(CanPlaceMachine 등)와 분리.
	if (PlacementMode == EOJJ_BuildPlacementMode::Demolish)
	{
		DemolishUnderCursor();
		return;
	}

	// Foundation 모드(F1-b): 클릭 즉시 배치(드래그 없음). 머신 spawn-validate-destroy 패턴 미러 —
	// 검증/등록은 F1-a TryPlaceFoundation(그리드는 데이터만), 액터 위치 세팅은 여기서.
	// 머신 경로 도달 전 return → #164 퀘스트 훅(NotifyMainQuestMachinePlaced) 비경유(비간섭).
	if (PlacementMode == EOJJ_BuildPlacementMode::Foundation)
	{
		PlaceFoundationAtCursor();
		return;
	}

	// === Machine 모드 (기존 동작 무변경) ===
	TSubclassOf<AMachineBase> ActiveMachineClass = GetActiveMachineClass();
	if (!TargetGrid || !ActiveMachineClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] TargetGrid 또는 MachineClass 미설정"));
		return;
	}

	// 마우스가 floor 밖이라 호버 갱신이 한 번도 안 됐으면 클릭 무시
	if (CurrentHoverCell.X == INT_MIN || CurrentHoverCell.Y == INT_MIN)
	{
		return;
	}

	AMachineBase* DefaultMachine = ActiveMachineClass.GetDefaultObject();
	if (!DefaultMachine)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] MachineClass CDO 없음"));
		return;
	}

	// cursor cell → lower-left origin — UpdateMouseHover와 같은 변환을 사용해야 호버
	// 미리보기와 실제 배치 위치가 어긋나지 않음. CanPlaceMachine이 IsValidGridCell +
	// OccupiedCells 통합 판정하므로 anchor 음수/초과도 자연 거부됨 → 사전 bounds
	// 차단(IsValidGridCell)은 더 이상 필요 없음.
	// 배치도 회전 반영(단계 4). origin/CanPlace/TryPlace + 메시 yaw 모두 같은 HoverRotationSteps를
	// 써야 점유·중심·메시가 일치(Codex 지적 핵심). 호버 미리보기(UpdateMouseHover)와도 동일 step이라
	// "미리보기 = 실제 배치" 정합.
	ApplyMachineDataToDefault(this, DefaultMachine);
	const FIntPoint Origin = ComputeOriginFromCursorCell(CurrentHoverCell, DefaultMachine, HoverRotationSteps);

	if (!TargetGrid->CanPlaceMachine(DefaultMachine, Origin, HoverRotationSteps))
	{
		UE_LOG(LogTemp, Log, TEXT("[BuildController] origin %s 배치 불가 (bounds/점유)"),
			*Origin.ToString());
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = this;

	AMachineBase* NewMachine = World->SpawnActor<AMachineBase>(
		ActiveMachineClass,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams);

	if (!NewMachine)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] SpawnActor 실패"));
		return;
	}

	FString OutReason;
	if (!TargetGrid->TryPlaceMachine(NewMachine, Origin, OutReason, HoverRotationSteps))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] TryPlaceMachine 실패: %s"), *OutReason);
		NewMachine->Destroy();
		return;
	}

	// 메시 yaw 회전 — TryPlaceMachine이 회전 footprint 중심(GetMachinePlacementLocation(.., step))에
	// 액터를 놓았으므로, 그 중심을 기준으로 yaw만 돌리면 center-anchor 메시가 회전 footprint와 정렬.
	// 시계방향 90°×step (R 방향). 부호가 R 의도와 반대면 -90.f로.
	NewMachine->SetActorRotation(FRotator(0.f, 90.f * HoverRotationSteps, 0.f));
	NotifyMainQuestMachinePlaced(this, GetQuestPlacementTargetId(PlacementMode));

	UE_LOG(LogTemp, Log, TEXT("[BuildController] origin %s 머신 배치 성공"),
		*Origin.ToString());

	// 직전 origin이 이제 점유됨 → 다음 UpdateMouseHover에서 빨강으로 강제 재표시
	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
}

// === Foundation 모드 (F1-b — 머신 경로와 독립, 커버리지 배치) ===

void AOJJ_BuildController::UpdateFoundationHover(FIntPoint CursorCell, const FHitResult& Hit)
{
	// 머신 호버와 동일한 표면 게이트: floor/머신 위에서만 유효(그 외 표면은 off-grid — 프리뷰 클리어).
	// 머신 위 호버는 점유 셀로 매핑돼 CanPlaceFoundation occupied 게이트가 빨강 표시 — 의도된 피드백.
	// (배치된 Foundation 슬래브는 NoCollision이라 트레이스가 통과해 floor에 닿음 → 겹침 빨강도 정상 동작.)
	UPrimitiveComponent* HitComp = Hit.GetComponent();
	AActor* HitActor = Hit.GetActor();
	const bool bHitFloor = (HitComp == TargetGrid->GetGridFloorMesh());
	const bool bHitMachine = HitActor && HitActor->IsA<AMachineBase>();
	// F1-c: 기존 슬래브 위 호버도 유효(겹침은 CanPlaceFoundation이 빨강으로 — 인접 확장 배치 UX).
	const bool bHitFoundation = HitActor && HitActor->IsA<AOJJ_Foundation>();
	// F2-1' 사각지대 해소: 평면 위(+델타) 지형의 Landscape 선히트 허용 — 머신 게이트와 동일 사유/처리.
	const bool bHitLandscape = HitActor && HitActor->IsA<ALandscapeProxy>();
	if (!bHitFloor && !bHitMachine && !bHitFoundation && !bHitLandscape)
	{
		TargetGrid->ClearHoverPreview();
		CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
		return;
	}

	// 머신 경로와 동일한 동일-셀 ISM 리빌드 스킵(Tick 경로 비용 절감).
	if (CursorCell == CurrentHoverCell)
	{
		return;
	}

	const AOJJ_Foundation* DefaultFoundation = FoundationClass ? FoundationClass.GetDefaultObject() : nullptr;
	if (!DefaultFoundation)
	{
		return;
	}

	// CDO에서 크기만 읽음(머신 CDO 풋프린트 조회와 동일 — spawn 부작용 없음). 회전은 F1 미지원(정사각 8×8).
	const FIntPoint Size = DefaultFoundation->GetFoundationSize();
	const FIntPoint Origin = ComputeOriginFromCursorCellForSize(CursorCell, Size);
	TargetGrid->OJJ_UpdateFoundationHoverPreview(Origin, Size);
	CurrentHoverCell = CursorCell;
}

void AOJJ_BuildController::PlaceFoundationAtCursor()
{
	if (!TargetGrid || !FoundationClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] TargetGrid 또는 FoundationClass 미설정"));
		return;
	}

	// 마우스가 floor 밖이라 호버 갱신이 한 번도 안 됐으면 클릭 무시(머신 경로와 동일).
	if (CurrentHoverCell.X == INT_MIN || CurrentHoverCell.Y == INT_MIN)
	{
		return;
	}

	const AOJJ_Foundation* DefaultFoundation = FoundationClass.GetDefaultObject();
	if (!DefaultFoundation)
	{
		return;
	}

	// 호버와 같은 origin 변환을 사용해야 "미리보기 = 실제 배치" 정합(머신 경로의 핵심 계약과 동일).
	const FIntPoint Size = DefaultFoundation->GetFoundationSize();
	const FIntPoint Origin = ComputeOriginFromCursorCellForSize(CurrentHoverCell, Size);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = this;

	AOJJ_Foundation* NewFoundation = World->SpawnActor<AOJJ_Foundation>(
		FoundationClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (!NewFoundation)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] Foundation SpawnActor 실패"));
		return;
	}

	// SurfaceZ = 평면 + Thickness + 스냅 리프트(F2-4 §5-4 — 풋프린트 GroundZ 최고점의 N×100단, 평탄 N=0 =
	// F1 동작). 좌표/리프트는 그리드 헬퍼(결정점 ② — 데이터/좌표는 그리드, 액터 이동은 컨트롤러).
	// 액터는 통째로 리프트만큼 위 — 슬래브 상면(액터Z+Thickness)이 SurfaceZ와 자동 일치. 실패 시 즉시 파기.
	const FVector PlaceLocation = TargetGrid->GetFoundationPlacementLocation(Origin, Size);
	const float SnapLift = TargetGrid->OJJ_ComputeFoundationSnapLift(Origin, Size, NewFoundation->GetThickness());
	const FVector SnappedLocation = PlaceLocation + FVector(0.0f, 0.0f, SnapLift);
	FString OutReason;
	if (!TargetGrid->TryPlaceFoundation(
		NewFoundation, Origin, Size, SnappedLocation.Z + NewFoundation->GetThickness(), OutReason))
	{
		// OutReason에 사유별 셀 수(water/occupied/overlap 등) — F1-b 디버깅·waterZ 재검토 실측 데이터.
		UE_LOG(LogTemp, Log, TEXT("[BuildController] Foundation 배치 불가: %s"), *OutReason);
		NewFoundation->Destroy();
		return;
	}

	NewFoundation->SetActorLocation(SnappedLocation);
	NewFoundation->OJJ_NotifyPlacedOnGrid(TargetGrid);

	// N 기록(결정 ⑤ 보강) — 단 분포 실측으로 상한 재검토 근거 축적.
	UE_LOG(LogTemp, Log, TEXT("[BuildController] origin %s Foundation 배치 성공 (%dx%d, N=%d단)"),
		*Origin.ToString(), Size.X, Size.Y,
		FMath::RoundToInt(SnapLift / AOJJ_Grid::OJJ_FoundationSnapStep));

	// 직전 영역이 이제 커버됨(겹침 금지) → 다음 호버에서 빨강 재표시 강제(머신 경로와 동일).
	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
}

// === Conveyor 입력 (Step 6 — Dummy 원본 이식(parity)) ===

void AOJJ_BuildController::OnLeftClickReleased()
{
	if (PlacementMode == EOJJ_BuildPlacementMode::Conveyor)
	{
		CommitConveyorDrag();
	}
	else if (PlacementMode == EOJJ_BuildPlacementMode::PowerLine)
	{
		CommitPowerLineDrag();
	}
}

void AOJJ_BuildController::SetPlacementMode(EOJJ_BuildPlacementMode NewMode)
{
	if (PlacementMode == NewMode)
	{
		return;
	}

	// 모드 전환 시 진행 중 드래그는 취소(잔여 상태 방지).
	CancelConveyorDrag();
	CancelPowerLineDrag();
	PlacementMode = NewMode;
	const TCHAR* ModeName = TEXT("Unknown");
	switch (PlacementMode)
	{
	case EOJJ_BuildPlacementMode::Machine:   ModeName = TEXT("Machine");   break;
	case EOJJ_BuildPlacementMode::Conveyor:  ModeName = TEXT("Conveyor");  break;
	case EOJJ_BuildPlacementMode::PowerNode: ModeName = TEXT("PowerNode"); break;
	case EOJJ_BuildPlacementMode::PowerLine: ModeName = TEXT("PowerLine"); break;
	case EOJJ_BuildPlacementMode::Shield:    ModeName = TEXT("Shield");    break;
	case EOJJ_BuildPlacementMode::PowerPlant: ModeName = TEXT("PowerPlant"); break;
	case EOJJ_BuildPlacementMode::Grinder:   ModeName = TEXT("Grinder");    break;
	case EOJJ_BuildPlacementMode::Miner:     ModeName = TEXT("Miner");      break;
	case EOJJ_BuildPlacementMode::Pump:      ModeName = TEXT("Pump");       break;
	case EOJJ_BuildPlacementMode::Smelter:   ModeName = TEXT("Smelter");    break;
	case EOJJ_BuildPlacementMode::Warehouse: ModeName = TEXT("Warehouse");  break;
	case EOJJ_BuildPlacementMode::Demolish:  ModeName = TEXT("Demolish");   break;
	case EOJJ_BuildPlacementMode::Foundation: ModeName = TEXT("Foundation"); break;
	}
	UE_LOG(LogTemp, Log, TEXT("[BuildController] Placement mode changed to %s"), ModeName);

	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
	UpdateMouseHover();
}

bool AOJJ_BuildController::GetCursorCell(FIntPoint& OutCell) const
{
	if (!TargetGrid)
	{
		return false;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return false;
	}

	FHitResult Hit;
	const bool bHit = PC->GetHitResultUnderCursorByChannel(
		UEngineTypes::ConvertToTraceType(ECC_Visibility),
		/*bTraceComplex=*/false,
		Hit);

	if (!bHit)
	{
		return false;
	}

	OutCell = TargetGrid->WorldToGrid(Hit.Location);
	return true;
}

void AOJJ_BuildController::BeginConveyorDrag(FIntPoint StartCell)
{
	bIsDraggingConveyor = true;
	ConveyorDragCells.Reset();
	ConveyorDragCells.Add(StartCell);
	CurrentHoverCell = StartCell;
	TargetGrid->OJJ_UpdateConveyorPathHoverPreview(ConveyorDragCells);
}

void AOJJ_BuildController::UpdateConveyorDrag(FIntPoint CursorCell)
{
	AppendConveyorPathTo(CursorCell);
	TargetGrid->OJJ_UpdateConveyorPathHoverPreview(ConveyorDragCells);
	CurrentHoverCell = CursorCell;
}

void AOJJ_BuildController::CancelConveyorDrag()
{
	if (!bIsDraggingConveyor)
	{
		return;
	}

	bIsDraggingConveyor = false;
	ConveyorDragCells.Reset();
	if (TargetGrid)
	{
		TargetGrid->ClearHoverPreview();
	}
	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
}

void AOJJ_BuildController::CommitConveyorDrag()
{
	if (!bIsDraggingConveyor)
	{
		return;
	}

	bIsDraggingConveyor = false;

	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] Conveyor placement called on non-authority"));
		ConveyorDragCells.Reset();
		return;
	}

	if (!TargetGrid || !ConveyorClass || ConveyorDragCells.Num() == 0)
	{
		ConveyorDragCells.Reset();
		return;
	}

	TArray<FIntPoint> PlacementCells;
	FString OutReason;
	if (!TargetGrid->OJJ_BuildConveyorPlacementPath(ConveyorDragCells, PlacementCells, OutReason))
	{
		UE_LOG(LogTemp, Log, TEXT("[BuildController] Conveyor path cannot be placed: %s"), *OutReason);
		ConveyorDragCells.Reset();
		TargetGrid->ClearHoverPreview();
		CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		ConveyorDragCells.Reset();
		TargetGrid->ClearHoverPreview();
		CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = this;

	AConveyor* Conveyor = World->SpawnActor<AConveyor>(
		ConveyorClass,
		TargetGrid->GetActorLocation(),
		FRotator::ZeroRotator,
		SpawnParams);

	if (!Conveyor)
	{
		ConveyorDragCells.Reset();
		TargetGrid->ClearHoverPreview();
		CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
		return;
	}

	if (!TargetGrid->OJJ_TryPlaceConveyor(Conveyor, PlacementCells, OutReason))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] OJJ_TryPlaceConveyor failed: %s"), *OutReason);
		Conveyor->Destroy();
		ConveyorDragCells.Reset();
		TargetGrid->ClearHoverPreview();
		CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
		return;
	}

	NotifyMainQuestMachinePlaced(this, GetQuestPlacementTargetId(EOJJ_BuildPlacementMode::Conveyor));
	ConveyorDragCells.Reset();
	TargetGrid->ClearHoverPreview();
	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
}

void AOJJ_BuildController::CancelPowerLineDrag()
{
	if (!bIsDraggingPowerLine)
	{
		return;
	}

	bIsDraggingPowerLine = false;
	PowerLineStartMachine.Reset();
}

AMachineBase* AOJJ_BuildController::GetPowerLineEndpointUnderCursor() const
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return nullptr;
	}

	FHitResult Hit;
	const bool bHit = PC->GetHitResultUnderCursorByChannel(
		UEngineTypes::ConvertToTraceType(ECC_Visibility),
		/*bTraceComplex=*/false,
		Hit);

	if (!bHit)
	{
		return nullptr;
	}

	AMachineBase* Machine = Cast<AMachineBase>(Hit.GetActor());
	if (IsPowerLineEndpoint(Machine))
	{
		return Machine;
	}

	return FindPowerLineEndpointNearLocation(Hit.Location);
}

AMachineBase* AOJJ_BuildController::FindPowerLineEndpointNearLocation(const FVector& Location) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	AMachineBase* BestMachine = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	for (TActorIterator<AMachineBase> It(World); It; ++It)
	{
		AMachineBase* Machine = *It;
		if (!IsPowerLineEndpoint(Machine))
		{
			continue;
		}

		const FVector2D MachineSize = Machine->GetMachineSize();
		const float PickRadius = FMath::Max(MachineSize.X, MachineSize.Y) * 60.0f;
		const FVector MachineLocation = Machine->GetActorLocation();
		const float DistanceSquared = FVector::DistSquared2D(Location, MachineLocation);
		if (DistanceSquared <= FMath::Square(PickRadius) && DistanceSquared < BestDistanceSquared)
		{
			BestMachine = Machine;
			BestDistanceSquared = DistanceSquared;
		}
	}

	return BestMachine;
}

bool AOJJ_BuildController::IsPowerLineEndpoint(const AMachineBase* Machine) const
{
	if (!Machine)
	{
		return false;
	}

	if (Machine->IsA<APowerGridNode>() || Machine->IsA<APowerPlant>())
	{
		return true;
	}

	if (PowerPlantClass && Machine->IsA(PowerPlantClass))
	{
		return true;
	}

	return Machine->GetMachineType() == FName(TEXT("BasicGenerator"));
}

void AOJJ_BuildController::BeginPowerLineDrag(AMachineBase* StartMachine)
{
	if (!StartMachine)
	{
		return;
	}

	bIsDraggingPowerLine = true;
	PowerLineStartMachine = StartMachine;
}

void AOJJ_BuildController::CommitPowerLineDrag()
{
	if (!bIsDraggingPowerLine)
	{
		return;
	}

	AMachineBase* SourceMachine = PowerLineStartMachine.Get();
	AMachineBase* TargetMachine = GetPowerLineEndpointUnderCursor();
	CancelPowerLineDrag();

	if (!SourceMachine || !TargetMachine || SourceMachine == TargetMachine || !PowerLineClass)
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UFactoryManagerSubsystem* FactoryManager = GameInstance
		? GameInstance->GetSubsystem<UFactoryManagerSubsystem>()
		: nullptr;
	if (!FactoryManager || !FactoryManager->CanConnectPowerLineEndpoints(SourceMachine, TargetMachine))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = this;

	APowerLine* PowerLine = World->SpawnActor<APowerLine>(
		PowerLineClass,
		SourceMachine->GetActorLocation(),
		FRotator::ZeroRotator,
		SpawnParams);

	if (!PowerLine)
	{
		return;
	}

	PowerLine->ConfigurePowerLine(SourceMachine, TargetMachine);
	if (!FactoryManager->AddPowerConnection(SourceMachine, TargetMachine, PowerLine))
	{
		PowerLine->Destroy();
	}
}

void AOJJ_BuildController::AppendConveyorPathTo(FIntPoint TargetCell)
{
	if (ConveyorDragCells.Num() == 0)
	{
		ConveyorDragCells.Add(TargetCell);
		return;
	}

	FIntPoint LastCell = ConveyorDragCells.Last();
	if (LastCell == TargetCell)
	{
		return;
	}

	const int32 StepX = TargetCell.X > LastCell.X ? 1 : -1;
	while (LastCell.X != TargetCell.X)
	{
		LastCell.X += StepX;
		AddConveyorPathCell(LastCell);
	}

	const int32 StepY = TargetCell.Y > LastCell.Y ? 1 : -1;
	while (LastCell.Y != TargetCell.Y)
	{
		LastCell.Y += StepY;
		AddConveyorPathCell(LastCell);
	}
}

void AOJJ_BuildController::AddConveyorPathCell(FIntPoint Cell)
{
	int32 ExistingIndex = INDEX_NONE;
	if (ConveyorDragCells.Find(Cell, ExistingIndex))
	{
		ConveyorDragCells.SetNum(ExistingIndex + 1);
		return;
	}

	ConveyorDragCells.Add(Cell);
}

void AOJJ_BuildController::UpdateConveyorHover(FIntPoint CursorCell)
{
	if (bIsDraggingConveyor)
	{
		UpdateConveyorDrag(CursorCell);
		return;
	}

	if (CursorCell == CurrentHoverCell)
	{
		return;
	}

	TArray<FIntPoint> PreviewCells;
	PreviewCells.Add(CursorCell);
	TargetGrid->OJJ_UpdateConveyorPathHoverPreview(PreviewCells);
	CurrentHoverCell = CursorCell;
}
