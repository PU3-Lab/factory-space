// Fill out your copyright notice in the Description page of Project Settings.


#include "OJJ_BuildController.h"

#include "DrawDebugHelpers.h"
#include "Engine/GameInstance.h"
#include "Engine/HitResult.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "FactoryManagerSubsystem.h"
#include "GameFramework/Pawn.h"
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
#include "Machines/LiquidTank.h"
#include "Machines/Smelter.h"
#include "Machines/WarehousePort.h"
#include "OJJ_Foundation.h"
#include "OJJ_ProtectionTower.h"
#include "Pipe.h"
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
		case EOJJ_BuildPlacementMode::LiquidTank:
			return TEXT("LiquidTank"); // F4-1' — ALiquidTank ctor의 MachineType과 동일 표기.
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
			QuestManager->NotifyTutorialEvent(TEXT("PlaceMachine"), MachineType);
		}
	}

	void NotifyTutorialQuestEvent(UObject* Context, FName EventId)
	{
		if (!Context || EventId.IsNone())
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
			QuestManager->NotifyTutorialEvent(EventId);
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
	PipeClass = APipe::StaticClass();
	LiquidTankClass = ALiquidTank::StaticClass();
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
		UpdateCharacterCellOverlay();
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
	if (PlacementMode == EOJJ_BuildPlacementMode::LiquidTank && !LiquidTankClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] LiquidTankClass missing. EnterBuildMode stopped."));
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

	// Foundation 종류도 평판으로 시작(F3-2.5) — 회전과 동일한 "예측 가능한 기본값" 정책.
	bRampFoundationSelected = false;

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

	// 캐릭터 셀 표시(F2-4 후속 ②) — 빈 캐시로 시작해 첫 Tick이 무조건 적재.
	CharacterOverlayCells.Reset();
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
		TargetGrid->OJJ_UpdateCharacterCellOverlay(TArray<FIntPoint>());  // 캐릭터 셀 표시 제거(F2-4 후속 ②)
	}
	CharacterOverlayCells.Reset();

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

	// Foundation 종류도 평판으로 리셋(F3-2.5 — EnterBuildMode 초기화와 대칭).
	bRampFoundationSelected = false;

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
	// 회전은 머신 + Foundation(F3-0 ㉱ — 램프 방향성 대비) 호버 전용 — 컨베이어 모드에서는 무시(Dummy parity).
	if (!bIsBuildMode
		|| (PlacementMode != EOJJ_BuildPlacementMode::Machine
			&& PlacementMode != EOJJ_BuildPlacementMode::PowerNode
			&& PlacementMode != EOJJ_BuildPlacementMode::Shield
			&& PlacementMode != EOJJ_BuildPlacementMode::PowerPlant
			&& PlacementMode != EOJJ_BuildPlacementMode::Grinder
			&& PlacementMode != EOJJ_BuildPlacementMode::Miner
			&& PlacementMode != EOJJ_BuildPlacementMode::Pump
			&& PlacementMode != EOJJ_BuildPlacementMode::Smelter
			&& PlacementMode != EOJJ_BuildPlacementMode::Warehouse
			&& PlacementMode != EOJJ_BuildPlacementMode::LiquidTank
			&& PlacementMode != EOJJ_BuildPlacementMode::Foundation))
	{
		return;
	}

	HoverRotationSteps = (HoverRotationSteps + 1) % 4;

	// 마우스가 같은 셀에 멈춰 있어도 회전이 즉시 미리보기에 반영되도록 sentinel 리셋 후 강제 갱신.
	// (UpdateMouseHover는 CursorCell==CurrentHoverCell이면 rebuild를 스킵하므로 sentinel이 필요.)
	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
	UpdateMouseHover();
}

void AOJJ_BuildController::OJJ_SelectFoundationKind(bool bSelectRamp)
{
	// F3.7' 키 개편(G=평판/H=램프 직행 — F3-2.5 T 토글 대체). 빌드모드 밖 호출은 기존 모드 키들과
	// 동일하게 무해(호버/클릭이 bIsBuildMode 게이트, EnterBuildMode가 종류를 평판으로 리셋 — 회전 정책).
	// 램프 미지정이면 선택 거부 — 사용처(호버/배치)의 silent 폴백보다 선택 시점 1회 경고가 명확.
	if (bSelectRamp && !RampFoundationClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BuildController] RampFoundationClass 미지정 — 램프 모드 선택 무시"));
		return;
	}

	const bool bKindChanged = (bRampFoundationSelected != bSelectRamp);
	bRampFoundationSelected = bSelectRamp;
	UE_LOG(LogTemp, Log, TEXT("[BuildController] Foundation: %s"),
		bRampFoundationSelected ? TEXT("Ramp") : TEXT("Flat"));

	// Foundation 모드 밖이면 진입까지(직행 키 의미) — SetPlacementMode가 sentinel 리셋 + 호버
	// 강제 갱신을 수행하므로 별도 처리 불필요.
	if (PlacementMode != EOJJ_BuildPlacementMode::Foundation)
	{
		SetPlacementMode(EOJJ_BuildPlacementMode::Foundation);
		return;
	}

	if (!bKindChanged)
	{
		return; // 같은 종류 재선택 — 호버 변화 없음.
	}

	// 종류가 바뀌면 CDO 풋프린트(평판 8×8 vs 램프 비정사각)가 달라짐 — 회전(RotateHoverClockwise)과
	// 동일하게 sentinel 리셋 후 강제 갱신(F3-2.5 T 로직 재사용). 단 커서가 유효 표면 밖이면
	// UpdateMouseHover의 Foundation 분기가 리빌드 없이 끝날 수 있으므로, 이전 종류 타일 잔존 금지를
	// 위해 먼저 명시적으로 클리어(유효 표면 위라면 프리뷰 함수가 어차피 클리어 후 재적재 — 이중 무해).
	if (TargetGrid)
	{
		TargetGrid->ClearHoverPreview();
	}
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
	// 머신/Foundation 공통 수식 — 두 경로의 "마우스 = 풋프린트 중심" 정책이 갈라지지 않게 단일원 유지.
	// F3.6-0: 본문을 그리드 정적으로 이관(Foundation 풋프린트 훅 베이스도 같은 수식을 쓰도록) — 위임만.
	return AOJJ_Grid::OJJ_OriginFromCursorCellForSize(CursorCell, EffSize);
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

	if (PlacementMode == EOJJ_BuildPlacementMode::LiquidTank)
	{
		return LiquidTankClass;
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

	const FIntPoint CursorCell = ResolveCursorCellOverWater(Hit.Location); // #182 물 위 패럴랙스 보정(호버=클릭 동일 셀)

	// Conveyor/Pipe 모드: 드래그/단일 셀 미리보기로 분기 (머신 경로와 독립 — 파이프는 프리뷰만 분기).
	if (PlacementMode == EOJJ_BuildPlacementMode::Conveyor
		|| PlacementMode == EOJJ_BuildPlacementMode::Pipe)
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

	// F4-1(Codex ③): 파이프 → Foundation 순서 — 클릭(DemolishUnderCursor)과 동일 우선순위(단일 진실원).
	// 위→아래(건물→파이프→기초): Foundation 위 파이프를 호버/클릭 모두 파이프로 잡는다.
	if (!Target)
	{
		Target = TargetGrid->OJJ_GetPipeAtCell(CursorCell);
	}
	// F1-b': 점유(머신/컨베이어)·파이프가 없는 셀은 Foundation 역조회.
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
		// F4-1: Foundation도 아니면 파이프 레이어 셀 — 라인 전체 빨강(클릭 철거 단위와 일치).
		if (!Cells)
		{
			Cells = TargetGrid->OJJ_GetPipeCells(Target);
		}
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
	// F4-1(Codex ③): 파이프가 Foundation보다 먼저 — 위→아래(건물→파이프→기초) 철거 순서. Foundation
	// 우선이면 그 위 파이프를 직접 철거할 수 없고(파이프 분기 도달 불가), Foundation 게이트는
	// OJJ_CountOccupiedFoundationCells의 파이프 합산이 막는다(거부 + 사유).
	if (!Target)
	{
		Target = TargetGrid->OJJ_GetPipeAtCell(CursorCell);
	}
	// F1-b': 점유·파이프가 없으면 Foundation 역조회 — 호버(UpdateDemolishHover)와 동일 우선순위.
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

		// F4-1: 파이프 캐스케이드 — 컨베이어와 동일 근거(끝점 머신 소실 = 라인 존재 조건 상실).
		// 수집은 레이어 역방향 맵 + 끝점 대조(그리드 헬퍼) — 컨베이어판(둘레 스캔)보다 직접적.
		for (APowerLine* PowerLine : CollectPowerLinesConnectedToMachine(Machine))
		{
			if (!PowerLine)
			{
				continue;
			}

			PowerLine->Destroy();
		}

		TArray<APipe*> ConnectedPipes;
		TargetGrid->OJJ_GetPipesConnectedToMachine(Machine, ConnectedPipes);
		for (APipe* Pipe : ConnectedPipes)
		{
			FString PipeReason;
			TargetGrid->OJJ_UnregisterPipeCells(Pipe, PipeReason);
			Pipe->Destroy();
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
	else if (APipe* Pipe = Cast<APipe>(Target))
	{
		// F4-1 파이프 직접 철거: 라인 단위(1액터=다중셀) 레이어 해제 + Destroy. 위 건물 게이트 없음
		// (파이프 레이어 위엔 아무것도 안 올라감 — 항상 성공).
		FString PipeReason;
		if (TargetGrid->OJJ_UnregisterPipeCells(Pipe, PipeReason))
		{
			Pipe->Destroy();
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
		NotifyTutorialQuestEvent(this, TEXT("DemolishRemoved"));
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

TArray<APowerLine*> AOJJ_BuildController::CollectPowerLinesConnectedToMachine(AMachineBase* Machine) const
{
	TArray<APowerLine*> Result;
	if (!Machine)
	{
		return Result;
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return Result;
	}

	UFactoryManagerSubsystem* FactoryManager = GameInstance->GetSubsystem<UFactoryManagerSubsystem>();
	if (!FactoryManager)
	{
		return Result;
	}

	TSet<APowerLine*> Seen;
	for (const FPowerConnectionEdge& Connection : FactoryManager->GetPowerConnectionEdges())
	{
		APowerLine* PowerLine = Connection.PowerLineActor.Get();
		if (!PowerLine || Seen.Contains(PowerLine))
		{
			continue;
		}

		if (PowerLine->GetSourceMachine() == Machine || PowerLine->GetTargetMachine() == Machine)
		{
			Seen.Add(PowerLine);
			Result.Add(PowerLine);
		}
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

	// Conveyor/Pipe 모드: 좌클릭 누름 = 드래그 시작. (커밋은 OnLeftClickReleased.)
	// 파이프(F4-1)는 드래그 상태머신 공용 — 프리뷰/커밋만 모드 분기.
	if (PlacementMode == EOJJ_BuildPlacementMode::Conveyor
		|| PlacementMode == EOJJ_BuildPlacementMode::Pipe)
	{
		FIntPoint CursorCell;
		if (GetCursorCell(CursorCell))
		{
			// #182 파이프 시작 스냅(파이프 전용) — 펌프 본체/출력 포트 근방을 클릭하면 등록된 출력 포트 셀로
			// 보정. 3×3 펌프 바깥 한 칸을 픽셀단위로 집는 비현실적 조준 제거. 컨베이어는 미적용(시작 판정 무변경).
			if (PlacementMode == EOJJ_BuildPlacementMode::Pipe)
			{
				// ⭐ 1순위: 스크린 공간 출력 포트 스냅 — 화면에서 커서 근처 포트 박스로 스냅(월드 Z 패럴랙스 무관).
				// 2순위(폴백): 그리드 셀 근방 2칸 스냅. 둘 다 실패면 원래 셀 유지.
				APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
				FIntPoint SnapCell;
				if (PC && TargetGrid->OJJ_FindLiquidOutputPortUnderCursorScreen(PC, /*MaxScreenDist=*/64.0f, SnapCell))
				{
					CursorCell = SnapCell;
				}
				else if (TargetGrid->OJJ_GetPipeOutputStartCell(CursorCell, /*MaxSnap=*/2, SnapCell))
				{
					CursorCell = SnapCell;
				}
			}
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

TSubclassOf<AOJJ_Foundation> AOJJ_BuildController::GetActiveFoundationClass() const
{
	// F3-2.5: 종류 상태에 따라 평판/램프 선택. 램프 선택 상태인데 클래스가 비어 있는 경우는
	// OJJ_SelectFoundationKind 게이트가 차단하므로 정상 흐름에선 도달 불가 — 그래도 null이면
	// 사용처(호버/배치)의 기존 null 가드가 동작한다.
	return bRampFoundationSelected ? RampFoundationClass : FlatFoundationClass;
}

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

	const TSubclassOf<AOJJ_Foundation> ActiveClass = GetActiveFoundationClass();
	const AOJJ_Foundation* DefaultFoundation = ActiveClass ? ActiveClass.GetDefaultObject() : nullptr;
	if (!DefaultFoundation)
	{
		// 활성 클래스가 비어 있으면(예: 램프 선택 상태에서 PIE 중 에디터로 클래스 비움) 이전 종류
		// 프리뷰가 stale로 남지 않게 정리 후 반환(Codex F3-2.5 ④ RISK 방어).
		TargetGrid->ClearHoverPreview();
		CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
		return;
	}

	// F3.6-0(㉽): 풋프린트는 CDO 훅이 산출 — 베이스는 기존 정적 산출(홀수 step 스왑 + origin 공통
	// 수식)과 동작 동일(회귀 0). 자동 맞춤 램프(F3.6-1)부터 커서+그리드 상태 기반 동적 풋프린트가
	// override로 들어온다(CDO 호출 — spawn 부작용 없음은 종전과 동일).
	const FOJJFoundationFitResult Fit = DefaultFoundation->OJJ_ComputeHoverFootprint(
		*TargetGrid, CursorCell, HoverRotationSteps);
	// F3.6-1(㊂): 풋프린트 구성 불가(자동 맞춤 경사 한계)는 클릭도 같은 훅 bValid로 거부 — 빨강 강제로
	// 색 단일 진실원 유지. 사유 텍스트는 클릭 시 로그(아래 호버 로그에도 동반 — 셀 변경 시만이라 저빈도).
	TargetGrid->OJJ_UpdateFoundationHoverPreview(Fit.Origin, Fit.EffSize, !Fit.bValid);

	// 고스트 프리뷰(#187): 평판 전용. 색 판정원은 호버 타일과 동일(Fit.bValid AND CanPlaceFoundation)로 일치.
	// 램프는 후속 범위라 고스트 미표시 — OJJ_HideGhost로 전환 잔존 방지(ClearHoverPreview도 숨기지만 명시적).
	if (!bRampFoundationSelected)
	{
		FString GhostReason;
		const bool bGhostValid = Fit.bValid && TargetGrid->CanPlaceFoundation(Fit.Origin, Fit.EffSize, GhostReason);
		TargetGrid->OJJ_ShowGhostForFoundation(
			ActiveClass.GetDefaultObject(), Fit.Origin, Fit.EffSize, bGhostValid);
	}
	else
	{
		TargetGrid->OJJ_HideGhost();
	}
	// ㊁ 보강: 방향 출처 표시 — 자동(이웃 낮→높) vs 수동(R) 이원화의 UX 방어. 평판은 출처가 비어
	// 있어 무로그(스팸 0), 램프만 셀 변경 시 1줄.
	if (!Fit.DirectionSource.IsEmpty())
	{
		const FString InvalidSuffix = Fit.bValid
			? FString()
			: FString::Printf(TEXT(" — 구성 불가: %s"), *Fit.FailReason);
		UE_LOG(LogTemp, Log, TEXT("[BuildController] 램프 풋프린트 %s%s"), *Fit.DirectionSource, *InvalidSuffix);
	}
	CurrentHoverCell = CursorCell;
}

void AOJJ_BuildController::PlaceFoundationAtCursor()
{
	const TSubclassOf<AOJJ_Foundation> ActiveClass = GetActiveFoundationClass();
	if (!TargetGrid || !ActiveClass)
	{
		// F3-2.5 마이그레이션 안내: 구 FoundationClass 지정은 CoreRedirects가 FlatFoundationClass로
		// 이관 — 그래도 비어 있으면 BP/레벨 인스턴스에서 재지정 필요.
		UE_LOG(LogTemp, Warning,
			TEXT("[BuildController] TargetGrid 또는 %s 미설정 — BP/레벨 인스턴스에서 지정 확인(구 FoundationClass는 FlatFoundationClass로 개명됨)"),
			bRampFoundationSelected ? TEXT("RampFoundationClass") : TEXT("FlatFoundationClass"));
		return;
	}

	// 마우스가 floor 밖이라 호버 갱신이 한 번도 안 됐으면 클릭 무시(머신 경로와 동일).
	if (CurrentHoverCell.X == INT_MIN || CurrentHoverCell.Y == INT_MIN)
	{
		return;
	}

	const AOJJ_Foundation* DefaultFoundation = ActiveClass.GetDefaultObject();
	if (!DefaultFoundation)
	{
		return;
	}

	// 호버와 같은 풋프린트 훅을 사용해야 "미리보기 = 실제 배치" 정합(머신 경로의 핵심 계약과 동일 —
	// F3.6-0 ㉽: CDO 정적 산출 → 훅. 같은 입력(셀·회전)이면 같은 결과 — 클릭 시 재산출이 진실원).
	const FOJJFoundationFitResult Fit = DefaultFoundation->OJJ_ComputeHoverFootprint(
		*TargetGrid, CurrentHoverCell, HoverRotationSteps);
	if (!Fit.bValid)
	{
		// 풋프린트 구성 불가(F3.6-1 자동 맞춤 경사 한계 미달 등 — 베이스/고정 램프는 항상 valid).
		UE_LOG(LogTemp, Log, TEXT("[BuildController] Foundation 배치 거부(풋프린트): %s"), *Fit.FailReason);
		return;
	}
	const FIntPoint EffSize = Fit.EffSize;
	const FIntPoint Origin = Fit.Origin;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = this;

	AOJJ_Foundation* NewFoundation = World->SpawnActor<AOJJ_Foundation>(
		ActiveClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (!NewFoundation)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] Foundation SpawnActor 실패"));
		return;
	}

	// F3.6-1: 동적 풋프린트 확정값(자동 맞춤 길이/단수)을 액터에 통지 — 등록 전에 저장해
	// 비주얼(OJJ_NotifyPlacedOnGrid → UpdateSlabVisual)이 등록 데이터와 같은 규격으로 그린다.
	NewFoundation->OJJ_NotifyFitResult(Fit);

	// SurfaceZ = 평면 + Thickness + 스냅 리프트(F2-4 §5-4 — 풋프린트 GroundZ 최고점의 N×100단, 평탄 N=0 =
	// F1 동작). 좌표/리프트는 그리드 헬퍼(결정점 ② — 데이터/좌표는 그리드, 액터 이동은 컨트롤러).
	// 액터는 통째로 리프트만큼 위 — 슬래브 상면(액터Z+Thickness)이 SurfaceZ와 자동 일치. 실패 시 즉시 파기.
	const FVector PlaceLocation = TargetGrid->GetFoundationPlacementLocation(Origin, EffSize);
	// 높이 결정은 클래스 훅(F3.5 우선순위: ① 이웃 상속 → ② 지형 씨앗 / 램프 ③ 엣지 스냅 → 폴백).
	// HeightSource는 배치 로그용 출처(결정 ㉷ 보강 — 정책 동작 실측).
	// F3.6-1(㊁): 회전은 훅이 확정한 유효 step — 자동 맞춤은 부호(낮→높)가 이웃에서 자동, 그 외는
	// 입력 step 그대로. 스냅/퍼셀 산식/액터 yaw가 전부 같은 값을 써야 낮은 끝 판정이 안 어긋난다.
	FString HeightSource;
	const float SnapLift = NewFoundation->OJJ_ComputeSnapLift(
		*TargetGrid, Origin, EffSize, Fit.EffectiveRotationSteps, &HeightSource);
	const FVector SnappedLocation = PlaceLocation + FVector(0.0f, 0.0f, SnapLift);
	const float BaseSurfaceZ = SnappedLocation.Z + NewFoundation->GetThickness();

	// F3-2: 비평탄(램프) Foundation은 셀별 SurfaceZ — 산식은 클래스 책임(결정 ㉲), 등록은 PerCell 경유
	// (그리드가 불변식 검증). 평탄은 기존 단일값 경로 그대로(배열 미생성).
	FString OutReason;
	TArray<float> CellZs;
	const bool bPlaced = NewFoundation->OJJ_BuildPerCellSurfaceZ(EffSize, Fit.EffectiveRotationSteps, BaseSurfaceZ, Fit.RiseSteps, CellZs)
		? TargetGrid->OJJ_TryPlaceFoundationPerCell(NewFoundation, Origin, EffSize, CellZs, OutReason)
		: TargetGrid->TryPlaceFoundation(NewFoundation, Origin, EffSize, BaseSurfaceZ, OutReason);
	if (!bPlaced)
	{
		// OutReason에 사유별 셀 수(water/occupied/overlap 등) — F1-b 디버깅·waterZ 재검토 실측 데이터.
		UE_LOG(LogTemp, Log, TEXT("[BuildController] Foundation 배치 불가: %s"), *OutReason);
		NewFoundation->Destroy();
		return;
	}

	// F3-0(㉱): 액터 yaw = 90°×step — 로컬 Size 메시가 월드에서 EffSize 풋프린트와 정렬(머신 :873 패턴).
	// 정사각 평판 큐브는 시각 동일(회귀 0). step은 훅 확정값(F3.6-1 ㊁ — 산식과 동일 회전 규약).
	NewFoundation->SetActorLocationAndRotation(
		SnappedLocation, FRotator(0.0f, 90.0f * Fit.EffectiveRotationSteps, 0.0f));
	NewFoundation->OJJ_NotifyPlacedOnGrid(TargetGrid);

	// N + 높이 출처(결정 ⑤·㉷ 보강) + 방향 출처(㊁ 보강 — 자동/수동) 기록 — 정책 동작 실측.
	UE_LOG(LogTemp, Log, TEXT("[BuildController] origin %s Foundation 배치 성공 (%dx%d, R=%d, N=%d단, %s%s%s)"),
		*Origin.ToString(), EffSize.X, EffSize.Y, Fit.EffectiveRotationSteps,
		FMath::RoundToInt(SnapLift / AOJJ_Grid::OJJ_FoundationSnapStep), *HeightSource,
		Fit.DirectionSource.IsEmpty() ? TEXT("") : TEXT(", "), *Fit.DirectionSource);

	// F2-4 후속 ①: 풋프린트에 깔린 Pawn을 상면으로 올려태움(F3-2부터 셀별 SurfaceZ — 등록 데이터를
	// 그리드에서 읽음). 후속 ② 캐시도 리셋 — 셀은 그대로여도 비주얼 Z가 상면으로 바뀌므로 강제 재적재.
	OJJ_LiftPawnsOntoFoundation(Origin, EffSize, NewFoundation->GetThickness());
	CharacterOverlayCells.Reset();

	// 직전 영역이 이제 커버됨(겹침 금지) → 다음 호버에서 빨강 재표시 강제(머신 경로와 동일).
	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
}

void AOJJ_BuildController::OJJ_LiftPawnsOntoFoundation(FIntPoint Origin, FIntPoint Size, float SlabThickness)
{
	// 서버 권위 — 배치(TryPlaceFoundation의 HasAuthority)와 같은 흐름. 모든 Pawn 대상(멀티 대비).
	// 배치 성공 직후 호출되므로 셀별 SurfaceZ는 그리드 등록 데이터(GetFoundationSurfaceZ)가 진실원 —
	// 평판(전 셀 동일)과 램프(F3-2 계단)를 같은 코드로 처리(㉳).
	if (!HasAuthority() || !TargetGrid)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Pawn = *It;
		if (!IsValid(Pawn))
		{
			continue;
		}
		float Radius = 0.0f;
		float HalfHeight = 0.0f;
		Pawn->GetSimpleCollisionCylinder(Radius, HalfHeight);
		const FVector Loc = Pawn->GetActorLocation();
		const float Feet = Loc.Z - HalfHeight;
		const float Head = Loc.Z + HalfHeight;

		// 캡슐이 걸친 셀 범위(WorldToGrid — XY 전용, 셀 반올림 규칙 공유) ∩ 풋프린트.
		const FIntPoint MinCell = TargetGrid->WorldToGrid(Loc - FVector(Radius, Radius, 0.0f));
		const FIntPoint MaxCell = TargetGrid->WorldToGrid(Loc + FVector(Radius, Radius, 0.0f));
		const int32 IterMinX = FMath::Max(MinCell.X, Origin.X);
		const int32 IterMaxX = FMath::Min(MaxCell.X, Origin.X + Size.X - 1);
		const int32 IterMinY = FMath::Max(MinCell.Y, Origin.Y);
		const int32 IterMaxY = FMath::Min(MaxCell.Y, Origin.Y + Size.Y - 1);

		// 셀별 판정: 캡슐이 그 셀 슬래브 구간 [상면−두께, 상면]과 겹칠 때만 — 발이 이미 상면 이상이면
		// no-op, 머리가 슬래브 바닥 아래면(높은 단 밑 갭 보행 — 결정 ⑥) 간섭 없음. 올림 목표는
		// 걸린 셀 상면의 max(램프 위면 더 높은 행 기준 — 재끼임 방지).
		float LiftToZ = 0.0f;
		bool bLift = false;
		for (int32 X = IterMinX; X <= IterMaxX; ++X)
		{
			for (int32 Y = IterMinY; Y <= IterMaxY; ++Y)
			{
				float CellSurfaceZ = 0.0f;
				if (!TargetGrid->GetFoundationSurfaceZ(FIntPoint(X, Y), CellSurfaceZ))
				{
					continue;
				}
				if (Feet < CellSurfaceZ && Head > CellSurfaceZ - SlabThickness)
				{
					LiftToZ = bLift ? FMath::Max(LiftToZ, CellSurfaceZ) : CellSurfaceZ;
					bLift = true;
				}
			}
		}
		if (!bLift)
		{
			continue;
		}

		// 상면 + 캡슐 반높이(+2 초기 침투 방지 — 착지는 중력이 정리). XY 유지("깔면 올라탐").
		// 위 공간이 다른 액터로 막힌 경우의 정교한 처리는 백로그 — 일단 올리고 로그로 추적.
		const FVector NewLoc(Loc.X, Loc.Y, LiftToZ + HalfHeight + 2.0f);
		Pawn->SetActorLocation(NewLoc, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
		UE_LOG(LogTemp, Log, TEXT("[BuildController] Foundation 배치 — Pawn 올려태움: %s Z %.1f→%.1f (상면 %.1f)"),
			*Pawn->GetName(), Loc.Z, NewLoc.Z, LiftToZ);
	}
}

void AOJJ_BuildController::UpdateCharacterCellOverlay()
{
	if (!TargetGrid)
	{
		return;
	}

	// 로컬 플레이어만(F2-4 후속 ② — 타 플레이어 표시는 백로그). Pawn 없음(관전 등)이면 표시 제거 경로.
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;

	TArray<FIntPoint> Cells;
	if (Pawn)
	{
		float Radius = 0.0f;
		float HalfHeight = 0.0f;
		Pawn->GetSimpleCollisionCylinder(Radius, HalfHeight);
		const FVector Loc = Pawn->GetActorLocation();
		// 캡슐 풋프린트가 걸친 셀(보통 1~2, 최대 4) — WorldToGrid가 XY만 쓰므로 Z 무관. off-grid는 제외.
		const FIntPoint MinCell = TargetGrid->WorldToGrid(Loc - FVector(Radius, Radius, 0.0f));
		const FIntPoint MaxCell = TargetGrid->WorldToGrid(Loc + FVector(Radius, Radius, 0.0f));
		for (int32 X = MinCell.X; X <= MaxCell.X; ++X)
		{
			for (int32 Y = MinCell.Y; Y <= MaxCell.Y; ++Y)
			{
				const FIntPoint Cell(X, Y);
				if (TargetGrid->IsValidGridCell(Cell))
				{
					Cells.Add(Cell);
				}
			}
		}
	}

	// 셀 좌표 변경 시에만 ISM 재빌드(계약 — 비교는 ≤4원소라 틱 비용 무시 가능).
	if (Cells != CharacterOverlayCells)
	{
		CharacterOverlayCells = Cells;
		TargetGrid->OJJ_UpdateCharacterCellOverlay(Cells);
	}
}

// === Conveyor 입력 (Step 6 — Dummy 원본 이식(parity)) ===

void AOJJ_BuildController::OnLeftClickReleased()
{
	if (PlacementMode == EOJJ_BuildPlacementMode::Conveyor
		|| PlacementMode == EOJJ_BuildPlacementMode::Pipe)
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
	case EOJJ_BuildPlacementMode::Pipe:      ModeName = TEXT("Pipe");       break;
	case EOJJ_BuildPlacementMode::LiquidTank: ModeName = TEXT("LiquidTank"); break;
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

	OutCell = ResolveCursorCellOverWater(Hit.Location); // #182 물 위 패럴랙스 보정(호버=클릭 동일 셀)
	return true;
}

FIntPoint AOJJ_BuildController::ResolveCursorCellOverWater(const FVector& TerrainHitLocation) const
{
	const FIntPoint TerrainCell = TargetGrid->WorldToGrid(TerrainHitLocation);

	// #182 패럴랙스: WaterArea가 Visibility Ignore라 커서 레이가 물을 통과해 물 밑 지형을 맞는다. 깊은 물에선
	// 그 지형 히트가 보이는 수면 셀에서 십수 칸까지 빗나가(물 밖 육지로) 호버/클릭 셀이 어긋난다. 마우스 레이를
	// 직접 각 WaterArea 수면 평면과 교차시켜, 지형 히트보다 가까운(=보이는) 수면 셀이 있으면 그 셀을 쓴다.
	// 물 위가 아니면(육지/머신) 수면 교차가 없어 기존 지형 히트 XY 그대로 — 회귀 0.
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	FVector RayOrigin = FVector::ZeroVector;
	FVector RayDir = FVector::ZeroVector;
	if (PC && PC->DeprojectMousePositionToWorld(RayOrigin, RayDir))
	{
		const float TerrainDist = FVector::Dist(RayOrigin, TerrainHitLocation);
		FIntPoint WaterCell;
		if (TargetGrid->OJJ_TraceCursorToWaterSurface(RayOrigin, RayDir, TerrainDist, WaterCell))
		{
			return WaterCell;
		}
	}

	return TerrainCell;
}

void AOJJ_BuildController::UpdatePathDragHoverPreview(const TArray<FIntPoint>& Cells)
{
	// F4-1: 컨베이어/파이프가 드래그 상태머신(bIsDraggingConveyor/ConveyorDragCells)을 공용 —
	// 모드는 동시에 하나라 상태 충돌 없음(SetPlacementMode가 전환 시 드래그 취소). 프리뷰만 분기.
	if (PlacementMode == EOJJ_BuildPlacementMode::Pipe)
	{
		TargetGrid->OJJ_UpdatePipePathHoverPreview(Cells);
	}
	else
	{
		TargetGrid->OJJ_UpdateConveyorPathHoverPreview(Cells);
	}
}

void AOJJ_BuildController::BeginConveyorDrag(FIntPoint StartCell)
{
	bIsDraggingConveyor = true;
	ConveyorDragCells.Reset();
	ConveyorDragCells.Add(StartCell);
	CurrentHoverCell = StartCell;
	UpdatePathDragHoverPreview(ConveyorDragCells);
}

void AOJJ_BuildController::UpdateConveyorDrag(FIntPoint CursorCell)
{
	AppendConveyorPathTo(CursorCell);
	UpdatePathDragHoverPreview(ConveyorDragCells);
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

	// F4-1: 파이프 모드는 클래스/정규화/배치만 분기 — 드래그 수집·정리 흐름은 공용.
	const bool bPipeMode = PlacementMode == EOJJ_BuildPlacementMode::Pipe;
	if (!TargetGrid || (bPipeMode ? !PipeClass : !ConveyorClass) || ConveyorDragCells.Num() == 0)
	{
		ConveyorDragCells.Reset();
		return;
	}

	TArray<FIntPoint> PlacementCells;
	FString OutReason;
	const bool bPathBuilt = bPipeMode
		? TargetGrid->OJJ_BuildPipePlacementPath(ConveyorDragCells, PlacementCells, OutReason)
		: TargetGrid->OJJ_BuildConveyorPlacementPath(ConveyorDragCells, PlacementCells, OutReason);
	if (!bPathBuilt)
	{
		UE_LOG(LogTemp, Log, TEXT("[BuildController] %s path cannot be placed: %s"),
			bPipeMode ? TEXT("Pipe") : TEXT("Conveyor"), *OutReason);
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

	// F4-1: spawn-validate-destroy 패턴 공용 — 파이프는 클래스/TryPlace만 다름.
	if (bPipeMode)
	{
		APipe* Pipe = World->SpawnActor<APipe>(
			PipeClass, TargetGrid->GetActorLocation(), FRotator::ZeroRotator, SpawnParams);
		if (!Pipe)
		{
			ConveyorDragCells.Reset();
			TargetGrid->ClearHoverPreview();
			CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
			return;
		}
		if (!TargetGrid->OJJ_TryPlacePipe(Pipe, PlacementCells, OutReason))
		{
			UE_LOG(LogTemp, Warning, TEXT("[BuildController] OJJ_TryPlacePipe failed: %s"), *OutReason);
			Pipe->Destroy();
		}
		// 파이프는 퀘스트 배치 타깃 미등록(NotifyMainQuestMachinePlaced 비호출 — 컨베이어 전용 훅).
		ConveyorDragCells.Reset();
		TargetGrid->ClearHoverPreview();
		CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
		return;
	}

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
	FactoryManager->UpdatePowerGrid();
	PowerLine->UpdateLineVisual();
	NotifyTutorialQuestEvent(this, TEXT("PowerLineConnected"));
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
	UpdatePathDragHoverPreview(PreviewCells);
	CurrentHoverCell = CursorCell;
}
