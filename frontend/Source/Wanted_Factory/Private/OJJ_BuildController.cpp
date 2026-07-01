// Fill out your copyright notice in the Description page of Project Settings.


#include "OJJ_BuildController.h"

#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
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
#include "UObject/ConstructorHelpers.h"
#include "Machines/PowerGridNode.h"
#include "Machines/PowerLine.h"
#include "Machines/PowerPlant.h"
#include "Machines/Grinder.h"
#include "Machines/MachineSubsystem.h"
#include "Machines/MinerMachine.h"
#include "Machines/MoldingMachine.h"
#include "Machines/Pump.h"
#include "Machines/LiquidTank.h"
#include "Machines/Smelter.h"
#include "Machines/Synthesizer.h"
#include "Machines/TeleCommunicationTower.h"
#include "Machines/WarehousePort.h"
#include "Machines/EscapePod.h"
#include "Machines/BaseCamp.h"
#include "Machines/SignalAmplifier.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "OJJ_DemolishEffectActor.h"
#include "OJJ_Foundation.h"
#include "OJJ_HologramBuildUpComponent.h"
#include "OJJ_HologramPathBuildUpComponent.h"
#include "OJJ_Ladder.h"
#include "OJJ_ProtectionTower.h"
#include "Pipe.h"
#include "PlayerWarehouseSubsystem.h"
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
			return TEXT("LiquidTank"); // F4-1' ??ALiquidTank ctor??MachineType怨??숈씪 ?쒓린.
		case EOJJ_BuildPlacementMode::MoldingMachine:
			return TEXT("MoldingMachine");
		case EOJJ_BuildPlacementMode::Synthesizer:
			return TEXT("Synthesizer");
		case EOJJ_BuildPlacementMode::TeleCommunicationTower:
			return TEXT("TeleCommunicationTower");
		case EOJJ_BuildPlacementMode::BaseCamp:
			return TEXT("BaseCamp");
		case EOJJ_BuildPlacementMode::SignalAmplifier:
			return TEXT("Signal_Amplifier");
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

	FName GetRequiredInventoryItemForPlacement(const AMachineBase* Machine)
	{
		if (Machine && Machine->GetMachineType() == TEXT("TeleCommunicationTower"))
		{
			return TEXT("TeleCommunicationTower");
		}

		return NAME_None;
	}

	UPlayerWarehouseSubsystem* GetWarehouseSubsystem(UObject* Context)
	{
		UGameInstance* GameInstance = Context && Context->GetWorld()
			? Context->GetWorld()->GetGameInstance()
			: nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UPlayerWarehouseSubsystem>() : nullptr;
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
	// 鍮뚮뱶紐⑤뱶 ?숈븞留??몃쾭瑜?媛깆떊?섎㈃ ?섎?濡?Tick? 耳쒕몢??湲곕낯 鍮꾪솢??
	// Enter/ExitBuildMode?먯꽌 SetActorTickEnabled濡?on/off ??鍮뚮뱶紐⑤뱶 諛?0鍮꾩슜.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 而⑤쿋?댁뼱 紐⑤뱶 湲곕낯 ?대옒??BP 誘몄?????. Dummy? ?숈씪 ?⑦꽩.
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
	MoldingMachineClass = AMoldingMachine::StaticClass();
	SynthesizerClass = ASynthesizer::StaticClass();
	TeleCommunicationTowerClass = ATeleCommunicationTower::StaticClass();
	BaseCampClass = ABaseCamp::StaticClass();
	SignalAmplifierClass = ASignalAmplifier::StaticClass();
	// [#184] ?щ떎由?湲곕낯 ?대옒??BP 誘몄????? ??C++ AOJJ_Ladder. BP ?놁씠??C??諛곗튂 ?숈옉.
	LadderClass = AOJJ_Ladder::StaticClass();

	// [#5 전선 미리보기] 메시/머티리얼 기본값 로드(에디터 재지정 가능). BP 와이어링 없이도 동작.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PreviewCylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (PreviewCylinder.Succeeded())
	{
		PowerLinePreviewMesh = PreviewCylinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PreviewGhostMat(TEXT("/Game/OJJ/Materials/M_Ghost_Preview.M_Ghost_Preview"));
	if (PreviewGhostMat.Succeeded())
	{
		PowerLinePreviewMaterial = PreviewGhostMat.Object;
	}

	// [송전탑 범위] Sphere 메시 + 전기 플라즈마 머티리얼 기본 로드(에디터 재지정 가능).
	// M_PowerRange_Plasma는 별도 스크립트로 생성 — 미존재 시 머티리얼 null(메시 기본 머티리얼로 동작).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> RangeSphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (RangeSphere.Succeeded())
	{
		PowerRangeSphereMesh = RangeSphere.Object;
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RangePlasmaMat(TEXT("/Game/OJJ/Materials/M_PowerRange_Plasma.M_PowerRange_Plasma"));
	if (RangePlasmaMat.Succeeded())
	{
		PowerRangeMaterial = RangePlasmaMat.Object;
	}
}

void AOJJ_BuildController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Enter/Exit?먯꽌 Tick??on/off?섏?留? 諛⑹뼱?곸쑝濡?紐⑤뱶 媛?쒕룄 ?좎?(UpdateMouseHover ?대??먮룄 媛???덉쓬).
	if (IsInBuildMode())
	{
		UpdateMouseHover();
		UpdateCharacterCellOverlay();

		// [진입 hitch] 시각화 윈도우가 카메라(뷰타겟) 중심을 따라가도록 갱신. 중심 셀이 충분히 이동했을 때만
		// 재페인트(UpdateVisualWindow 내부 임계). 탑다운=빌드캠, TPS=플레이어가 뷰타겟.
		if (TargetGrid)
		{
			if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
			{
				if (AActor* ViewTarget = PC->GetViewTarget())
				{
					TargetGrid->UpdateVisualWindow(ViewTarget->GetActorLocation());
				}
			}
		}
	}
}

void AOJJ_BuildController::EnterBuildMode()
{
	if (IsInBuildMode())
	{
		return;
	}

	if (!TargetGrid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] TargetGrid 誘몄꽕????EnterBuildMode 以묐떒"));
		return;
	}

	// 紐⑤뱶蹂??대옒??誘몄꽕??媛????癒몄떊 紐⑤뱶??MachineClass, 而⑤쿋?댁뼱 紐⑤뱶??ConveyorClass ?꾩슂.
	// [진입 선택없음] 빌드모드는 항상 선택없음(None)으로 시작 — 직전 선택/기본값(Machine) 잔류로 고스트가 바로
	// 들리는 것 방지. 카테고리/숫자키로 골라야 고스트가 뜬다(UpdateMouseHover의 None 분기가 호버 클리어).
	// ⚠️ 아래 클래스 검증 가드 '이전'에 둠 — None이면 가드 전부 통과 → 직전 선택의 클래스 누락으로 진입이
	// 막히던 엣지도 해소. 실제 클래스 검증은 선택 시점으로 이동(UpdateMouseHover가 !ActiveMachineClass면 no-op).
	PlacementMode = EOJJ_BuildPlacementMode::None;

	if (PlacementMode == EOJJ_BuildPlacementMode::Machine && !MachineClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] MachineClass 誘몄꽕????EnterBuildMode 以묐떒"));
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
	if (PlacementMode == EOJJ_BuildPlacementMode::MoldingMachine && !MoldingMachineClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] MoldingMachineClass missing. EnterBuildMode stopped."));
		return;
	}
	if (PlacementMode == EOJJ_BuildPlacementMode::Synthesizer && !SynthesizerClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] SynthesizerClass missing. EnterBuildMode stopped."));
		return;
	}
	if (PlacementMode == EOJJ_BuildPlacementMode::TeleCommunicationTower && !TeleCommunicationTowerClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] TeleCommunicationTowerClass missing. EnterBuildMode stopped."));
		return;
	}
	if (PlacementMode == EOJJ_BuildPlacementMode::BaseCamp && !BaseCampClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] BaseCampClass missing. EnterBuildMode stopped."));
		return;
	}
	if (PlacementMode == EOJJ_BuildPlacementMode::SignalAmplifier && !SignalAmplifierClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] SignalAmplifierClass missing. EnterBuildMode stopped."));
		return;
	}
	if (PlacementMode == EOJJ_BuildPlacementMode::Conveyor && !ConveyorClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] ConveyorClass 誘몄꽕????EnterBuildMode 以묐떒"));
		return;
	}
	if (PlacementMode == EOJJ_BuildPlacementMode::PowerLine && !PowerLineClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] PowerLineClass missing. EnterBuildMode stopped."));
		return;
	}

	// [그리드 색상 2단계] 현재 모드 색상 규칙을 먼저 주입(bVisualizationActive 아직 false → paint 없이 멤버만 저장),
	// 직후 SetVisualizationVisible(true)가 그 규칙으로 1회 paint(중복 repaint 회피).
	// [진입 hitch] 시각화 윈도우 중심을 플레이어 위치로 시드 — SetVisualizationVisible의 첫 paint가
	// 전체(756²) 대신 윈도우만 그리도록(빌드캠 진입 XY = 플레이어 XY와 일치). Tick이 이후 카메라를 추종.
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		TargetGrid->SetVisualWindowCenterCell(TargetGrid->WorldToGrid(PlayerPawn->GetActorLocation()));
	}

	UpdateGridColorForCurrentMode();

	TargetGrid->SetVisualizationVisible(true);

	// 吏꾩엯 利됱떆 諛곗튂 癒몄떊 ?ы듃 ?붿궡???쒖떆(泥??몃쾭 ?꾩씠?쇰룄 蹂댁씠?꾨줉). ?몃쾭 ?붿궡?쒕뒗 泥?UpdateMouseHover?먯꽌.
	TargetGrid->RefreshPlacedMachineArrows();

	// 진입은 항상 TopDown으로 확정(EnterBuildMode = 기존 B 경로). TPS 요청 시 SetBuildViewMode가 직후 갱신.
	BuildViewMode = EBuildViewMode::TopDown;

	// 鍮뚮뱶 ?몄뀡? ??긽 ?뚯쟾 0(誘명쉶???쇰줈 ?쒖옉 ???덉륫 媛?ν븳 湲곕낯 諛⑺뼢.
	HoverRotationSteps = 0;

	// Foundation 醫낅쪟???됲뙋?쇰줈 ?쒖옉(F3-2.5) ???뚯쟾怨??숈씪??"?덉륫 媛?ν븳 湲곕낯媛? ?뺤콉.
	bRampFoundationSelected = false;

	// 而⑤쿋?댁뼱 ?쒕옒洹??곹깭 珥덇린???댁쟾 ?몄뀡 ?붿뿬 諛⑹?).
	bIsDraggingConveyor = false;
	ConveyorDragCells.Reset();
	bIsDraggingPowerLine = false;
	PowerLineStartMachine.Reset();

	// 鍮뚮뱶紐⑤뱶 ?숈븞?먮쭔 ?몃쾭 Tick 媛??
	SetActorTickEnabled(true);

	// [공중 Foundation] 진입 시 높이 오프셋 0으로 리셋(회전/Foundation종류와 동일 — 이전 세션 잔류 방지).
	BuildHeightOffset = 0;

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->bShowMouseCursor = true;
		PC->bEnableClickEvents = true;
		PC->bEnableMouseOverEvents = true;
	}

	// 泥?UpdateMouseHover ?몄텧??臾댁“嫄?媛깆떊???몃━嫄고븯?꾨줉 sentinel濡?珥덇린??
	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);

	// 罹먮┃??? ?쒖떆(F2-4 ?꾩냽 ?? ??鍮?罹먯떆濡??쒖옉??泥?Tick??臾댁“嫄??곸옱.
	CharacterOverlayCells.Reset();
}

void AOJJ_BuildController::ExitBuildMode()
{
	if (!IsInBuildMode())
	{
		return;
	}

	if (TargetGrid)
	{
		TargetGrid->SetVisualizationVisible(false);
		TargetGrid->ClearHoverPreview();         // ?몃쾭 ? + ?몃쾭 ?붿궡???쒓굅
		TargetGrid->ClearPlacedMachineArrows();
		TargetGrid->OJJ_UpdateCharacterCellOverlay(TArray<FIntPoint>());  // 罹먮┃??? ?쒖떆 ?쒓굅(F2-4 ?꾩냽 ??
	}
	CharacterOverlayCells.Reset();

	BuildViewMode = EBuildViewMode::None;

	// 而⑤쿋?댁뼱 ?쒕옒洹??곹깭 ?뺣━.
	bIsDraggingConveyor = false;
	ConveyorDragCells.Reset();
	bIsDraggingPowerLine = false;
	PowerLineStartMachine.Reset();

	// ?몃쾭 Tick ?뺤? (鍮뚮뱶紐⑤뱶 諛?0鍮꾩슜)
	// [#5] 빌드 해제 시 전선 미리보기 소등(Tick 중단 후엔 갱신이 안 돌아 잔상 방지 — 명시적 숨김 필요).
	HidePowerLinePreview();
	// [송전탑 범위] 빌드 해제 시 범위 구도 소등(동일 이유 — Tick 중단 후 잔상 방지).
	HidePowerRangePreview();

	SetActorTickEnabled(false);

	// [공중 Foundation] 종료 시 높이 오프셋 0으로 리셋(다음 진입이 어차피 0으로 덮지만 명시적 정리).
	BuildHeightOffset = 0;

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->bShowMouseCursor = false;
	}

	// ?ъ쭊????媛숈? ????뺤????덉뼱??泥?媛깆떊???숈옉?섎룄濡?sentinel濡?由ъ뀑
	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);

	// ?뚯쟾 ?곹깭??由ъ뀑 ???ㅼ쓬 吏꾩엯? 誘명쉶??0)?쇰줈 ?쒖옉(EnterBuildMode 珥덇린?붿? ?쇨?).
	HoverRotationSteps = 0;

	// Foundation 醫낅쪟???됲뙋?쇰줈 由ъ뀑(F3-2.5 ??EnterBuildMode 珥덇린?붿? ?移?.
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
	// 레거시 토글(B 단독 시절). 이제 SetBuildViewMode로 위임 — 빌드 중이면 해제, 아니면 TopDown 진입.
	// 신규 B/V 라우팅(모드 전환 포함)은 플레이어가 SetBuildViewMode를 직접 호출한다.
	SetBuildViewMode(IsInBuildMode() ? EBuildViewMode::None : EBuildViewMode::TopDown);
}

void AOJJ_BuildController::SetBuildViewMode(EBuildViewMode NewMode)
{
	if (NewMode == BuildViewMode)
	{
		return; // 동일 모드 — no-op
	}

	if (NewMode == EBuildViewMode::None)
	{
		ExitBuildMode(); // BuildViewMode → None
		UE_LOG(LogTemp, Log, TEXT("[BuildController] BuildViewMode -> None (해제)"));
		return;
	}

	if (!IsInBuildMode())
	{
		// 신규 진입(현재 None). EnterBuildMode는 검증 실패 시 early-return(BuildViewMode None 유지).
		EnterBuildMode(); // 성공 시 BuildViewMode = TopDown
		if (IsInBuildMode())
		{
			BuildViewMode = NewMode; // 요청이 TPS면 TopDown→TPS로 갱신
		}
	}
	else
	{
		// 이미 빌드모드 — TopDown↔TPS 전환(1단계 골격: 그리드 상태 유지, 모드 플래그만 교체).
		BuildViewMode = NewMode;
	}

	UE_LOG(LogTemp, Log, TEXT("[BuildController] BuildViewMode -> %s"),
		BuildViewMode == EBuildViewMode::TopDown ? TEXT("TopDown")
		: BuildViewMode == EBuildViewMode::TPS ? TEXT("TPS")
		: TEXT("None"));
}

void AOJJ_BuildController::RotateHoverClockwise()
{
	// R? IMC_Build ?꾩슜?대씪 鍮뚮뱶紐⑤뱶?먯꽌留?諛쒕룞?섏?留? 諛⑹뼱?곸쑝濡?媛??
	// ?뚯쟾? 癒몄떊 + Foundation(F3-0 ?????⑦봽 諛⑺뼢???鍮? ?몃쾭 ?꾩슜 ??而⑤쿋?댁뼱 紐⑤뱶?먯꽌??臾댁떆(Dummy parity).
	if (!IsInBuildMode()
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
			&& PlacementMode != EOJJ_BuildPlacementMode::MoldingMachine
			&& PlacementMode != EOJJ_BuildPlacementMode::Synthesizer
			&& PlacementMode != EOJJ_BuildPlacementMode::BaseCamp
			&& PlacementMode != EOJJ_BuildPlacementMode::SignalAmplifier
			&& PlacementMode != EOJJ_BuildPlacementMode::Foundation))
	{
		return;
	}

	HoverRotationSteps = (HoverRotationSteps + 1) % 4;

	// 留덉슦?ㅺ? 媛숈? ???硫덉떠 ?덉뼱???뚯쟾??利됱떆 誘몃━蹂닿린??諛섏쁺?섎룄濡?sentinel 由ъ뀑 ??媛뺤젣 媛깆떊.
	// (UpdateMouseHover??CursorCell==CurrentHoverCell?대㈃ rebuild瑜??ㅽ궢?섎?濡?sentinel???꾩슂.)
	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
	UpdateMouseHover();
}

void AOJJ_BuildController::OJJ_SelectFoundationKind(bool bSelectRamp)
{
	// 怨듭슜??吏곹뻾 ??F=?됲뙋/G=?⑦봽(怨듭슜 BindKey, ??G=?됲뙋/H=?⑦봽 IA 寃쎈줈 ?먭린). 鍮뚮뱶紐⑤뱶 諛??몄텧? 湲곗〈 紐⑤뱶 ?ㅻ뱾怨?
	// ?숈씪?섍쾶 臾댄빐(?몃쾭/?대┃??bIsBuildMode 寃뚯씠?? EnterBuildMode媛 醫낅쪟瑜??됲뙋?쇰줈 由ъ뀑 ???뚯쟾 ?뺤콉).
	// ?⑦봽 誘몄??뺤씠硫??좏깮 嫄곕? ???ъ슜泥??몃쾭/諛곗튂)??silent ?대갚蹂대떎 ?좏깮 ?쒖젏 1??寃쎄퀬媛 紐낇솗.
	if (bSelectRamp && !RampFoundationClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BuildController] RampFoundationClass 誘몄??????⑦봽 紐⑤뱶 ?좏깮 臾댁떆"));
		return;
	}

	const bool bKindChanged = (bRampFoundationSelected != bSelectRamp);
	bRampFoundationSelected = bSelectRamp;
	UE_LOG(LogTemp, Log, TEXT("[BuildController] Foundation: %s"),
		bRampFoundationSelected ? TEXT("Ramp") : TEXT("Flat"));

	// Foundation 紐⑤뱶 諛뽰씠硫?吏꾩엯源뚯?(吏곹뻾 ???섎?) ??SetPlacementMode媛 sentinel 由ъ뀑 + ?몃쾭
	// 媛뺤젣 媛깆떊???섑뻾?섎?濡?蹂꾨룄 泥섎━ 遺덊븘??
	if (PlacementMode != EOJJ_BuildPlacementMode::Foundation)
	{
		SetPlacementMode(EOJJ_BuildPlacementMode::Foundation);
		return;
	}

	if (!bKindChanged)
	{
		return; // 媛숈? 醫낅쪟 ?ъ꽑?????몃쾭 蹂???놁쓬.
	}

	// 醫낅쪟媛 諛붾뚮㈃ CDO ?뗮봽由고듃(?됲뙋 8횞8 vs ?⑦봽 鍮꾩젙?ш컖)媛 ?щ씪吏????뚯쟾(RotateHoverClockwise)怨?
	// ?숈씪?섍쾶 sentinel 由ъ뀑 ??媛뺤젣 媛깆떊(F3-2.5 T 濡쒖쭅 ?ъ궗??. ??而ㅼ꽌媛 ?좏슚 ?쒕㈃ 諛뽰씠硫?
	// UpdateMouseHover??Foundation 遺꾧린媛 由щ퉴???놁씠 ?앸궇 ???덉쑝誘濡? ?댁쟾 醫낅쪟 ????붿〈 湲덉?瑜?
	// ?꾪빐 癒쇱? 紐낆떆?곸쑝濡??대━???좏슚 ?쒕㈃ ?꾨씪硫??꾨━酉??⑥닔媛 ?댁감???대━?????ъ쟻?????댁쨷 臾댄빐).
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

	// AOJJ_Grid::CalculateFootprint / GetMachinePlacementLocation怨??숈씪???뺤닔?붋룻쉶??洹쒖튃(EffectiveSize).
	// ?낅젰(cursor ??origin)怨??쒓컖 蹂댁젙(origin ??footprint center)??諛섎? 諛⑺뼢?댁?留?
	// 媛숈? size 媛?뺤뿉???숈옉?댁빞 ?몃쾭/諛곗튂? occupancy/硫붿떆 ?꾩튂媛 ?닿툔?섏? ?딆쓬. step 0?대㈃ 湲곗〈怨??숈씪.
	const FIntPoint Size = AOJJ_Grid::EffectiveSize(Machine->GetMachineSize(), RotationSteps);

	return ComputeOriginFromCursorCellForSize(CursorCell, Size);
}

FIntPoint AOJJ_BuildController::ComputeOriginFromCursorCellForSize(FIntPoint CursorCell, FIntPoint EffSize)
{
	// 癒몄떊/Foundation 怨듯넻 ?섏떇 ????寃쎈줈??"留덉슦??= ?뗮봽由고듃 以묒떖" ?뺤콉??媛덈씪吏吏 ?딄쾶 ?⑥씪???좎?.
	// F3.6-0: 蹂몃Ц??洹몃━???뺤쟻?쇰줈 ?닿?(Foundation ?뗮봽由고듃 ??踰좎씠?ㅻ룄 媛숈? ?섏떇???곕룄濡? ???꾩엫留?
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

	if (PlacementMode == EOJJ_BuildPlacementMode::MoldingMachine)
	{
		return MoldingMachineClass;
	}

	if (PlacementMode == EOJJ_BuildPlacementMode::Synthesizer)
	{
		return SynthesizerClass;
	}

	if (PlacementMode == EOJJ_BuildPlacementMode::TeleCommunicationTower)
	{
		return TeleCommunicationTowerClass;
	}

	if (PlacementMode == EOJJ_BuildPlacementMode::BaseCamp)
	{
		return BaseCampClass;
	}

	if (PlacementMode == EOJJ_BuildPlacementMode::SignalAmplifier)
	{
		return SignalAmplifierClass;
	}

	return MachineClass;
}

void AOJJ_BuildController::UpdateGridColorForCurrentMode()
{
	if (!TargetGrid)
	{
		return;
	}

	EOJJGridColorMode ColorMode = EOJJGridColorMode::Machine;
	bool bRaw = false;
	bool bWater = false;

	switch (PlacementMode)
	{
	case EOJJ_BuildPlacementMode::Foundation:
		// Foundation/Ramp(bRampFoundationSelected는 종류만 다름) — CanPlaceFoundation 기준(경사 포함, 물·이미foundation 제외).
		ColorMode = EOJJGridColorMode::Foundation;
		break;
	case EOJJ_BuildPlacementMode::Miner:
		// 채굴기 — 광맥 4방향 인접 셀만 placeable(평지·경사 무관).
		ColorMode = EOJJGridColorMode::Miner;
		break;
	case EOJJ_BuildPlacementMode::Conveyor:
	case EOJJ_BuildPlacementMode::Pipe:
	case EOJJ_BuildPlacementMode::PowerLine:
	case EOJJ_BuildPlacementMode::Demolish:
		// 별도 액터 — constructible(buildable OR Foundation) 위에 동작 → raw 평지·Foundation 초록.
		bRaw = true;
		break;
	case EOJJ_BuildPlacementMode::None:
		// ⚠️ None = 빌드모드 아님/모드 전환 과도기 — raw 허용 금지(맨땅 파랑 오표시 방지).
		// 빌드모드 진입 경로가 PlacementMode=None인 채 UpdateGridColorForCurrentMode→SetVisualizationVisible로 1회
		// paint하면, raw=true 시 맨땅이 파랑으로 칠해져 일반 머신 들었을 때까지 잔존(버그). 일반 머신 기본과 동일 raw=false.
		bRaw = false;
		break;
	default:
		// 일반 머신 모드 — CDO 지형규칙(CanPlaceOnRawGround/CanStandOnWater). 일반 머신은 둘 다 false = Foundation 위만.
		if (TSubclassOf<AMachineBase> ActiveClass = GetActiveMachineClass())
		{
			if (const AMachineBase* CDO = ActiveClass.GetDefaultObject())
			{
				bRaw = CDO->CanPlaceOnRawGround();
				bWater = CDO->CanStandOnWater();
			}
		}
		break;
	}

	TargetGrid->OJJ_UpdateGridColorRule(ColorMode, bRaw, bWater);
}

void AOJJ_BuildController::UpdateMouseHover()
{
	if (!IsInBuildMode())
	{
		return;
	}

	if (!TargetGrid)
	{
		return;
	}

	// [송전탑 범위] PowerNode 모드가 아니면 범위 구를 항상 숨김(아래 모든 early-return 경로를 한 번에 커버).
	if (PlacementMode != EOJJ_BuildPlacementMode::PowerNode)
	{
		HidePowerRangePreview();
	}

	// [怨듭슜??Z] None = ?ㅺ퀬 ?덈뒗 placement ?놁쓬. 怨좎뒪??ISM/?붿궡?쒕쭔 ?대━?댄븯怨?臾대룞??鍮뚮뱶紐⑤뱶 ?좎?).
	if (PlacementMode == EOJJ_BuildPlacementMode::None)
	{
		TargetGrid->ClearHoverPreview();
		CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return;
	}

	// === Demolish 紐⑤뱶 ??而ㅼ꽌 ???鍮④컯 ?섏씠?쇱씠??諛곗튂 ?몃젅?댁뒪/?뗮봽由고듃 寃쎈줈? 遺꾨━) ===
	if (PlacementMode == EOJJ_BuildPlacementMode::Demolish)
	{
		UpdateDemolishHover();
		return;
	}

	FHitResult Hit;
	const bool bHit = ResolveBuildTraceHit(PC, Hit); // 모드별 소스(탑다운=커서 / TPS=화면중앙 전방레이+클램프)

	if (!bHit)
	{
		// ?몃젅?댁뒪 ?ㅽ뙣 ??stale 誘몃━蹂닿린/罹먯떆媛 ?ㅼ쓬 ?대┃???섎せ ?곸슜?섏? ?딅룄濡?紐낆떆??由ъ뀑
		TargetGrid->ClearHoverPreview();
		CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
		// [#5] 트레이스 미스(커서가 허공) 시에도 전선 미리보기 잔상 제거.
		HidePowerLinePreview();
		// [송전탑 범위] PowerNode 모드 트레이스 실패 시 범위 구도 숨김(멱등 — 타 모드는 위에서 이미 숨김).
		HidePowerRangePreview();
		return;
	}

	const FIntPoint CursorCell = ResolveCursorCellOverWater(Hit.Location); // #182 臾????⑤윺?숈뒪 蹂댁젙(?몃쾭=?대┃ ?숈씪 ?)

	// Conveyor/Pipe 紐⑤뱶: ?쒕옒洹??⑥씪 ? 誘몃━蹂닿린濡?遺꾧린 (癒몄떊 寃쎈줈? ?낅┰ ???뚯씠?꾨뒗 ?꾨━酉곕쭔 遺꾧린).
	// [송전탑 범위] PowerNode 모드: 커서 위치(송전탑 놓일 자리)에 SupplyRadius 전기 플라즈마 구 표시.
	if (PlacementMode == EOJJ_BuildPlacementMode::PowerNode)
	{
		UpdatePowerRangePreview(Hit.Location);
	}

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

		UpdatePowerLinePreview(Hit);
		return;
	}

	// Foundation 紐⑤뱶(F1-b): 癒몄떊 寃쎈줈? ?낅┰ 遺꾧린(Conveyor/Demolish ?⑦꽩) ??CDO FoundationSize ?뗮봽由고듃 ?몃쾭.
	if (PlacementMode == EOJJ_BuildPlacementMode::Foundation)
	{
		UpdateFoundationHover(CursorCell, Hit);
		return;
	}

	// [#184] Ladder 紐⑤뱶: Foundation 蹂 議곗? ??蹂 諛붽묑 吏硫댁뿉 ?몃줈 ?щ떎由?怨좎뒪???낅┰ 遺꾧린, ?먯쑀 諛곗튂).
	if (PlacementMode == EOJJ_BuildPlacementMode::Ladder)
	{
		UpdateLadderHover(CursorCell, Hit);
		return;
	}

	// === Machine 紐⑤뱶 (湲곗〈 ?숈옉 臾대?寃? ===
	TSubclassOf<AMachineBase> ActiveMachineClass = GetActiveMachineClass();
	if (!ActiveMachineClass)
	{
		return;
	}

	// floor ?먮뒗 ?대? 諛곗튂??癒몄떊 ?꾩뿉??hover瑜??좎?.
	// 癒몄떊 Cube mesh媛 Visibility 梨꾨꼸??Block?댁꽌 trace瑜?媛濡쒖콈?? 癒몄떊 ??XY??
	// ?먯쑀??????뺥솗??留ㅽ븨?섎?濡?CanPlaceMachine 寃利앹쓣 嫄곗튂寃?洹몃?濡??듦낵?쒗궓??
	// ???먯쑀 ?怨?寃뱀튇 ?뗮봽由고듃媛 鍮④컯?쇰줈 ?쒖떆?? 洹????쒕㈃(罹먮┃??踰????
	// off-grid?대?濡?湲곗〈泥섎읆 ClearHoverPreview濡?李⑤떒.
	UPrimitiveComponent* HitComp = Hit.GetComponent();
	AActor* HitActor = Hit.GetActor();
	const bool bHitFloor = (HitComp == TargetGrid->GetGridFloorMesh());
	const bool bHitMachine = HitActor && HitActor->IsA<AMachineBase>();
	// F1-c: Foundation ?щ옒釉??곷㈃(Visibility Block)???좏슚 ?몃쾭 ?쒕㈃ ???놁쑝硫?Foundation ??癒몄떊 諛곗튂 遺덇?.
	const bool bHitFoundation = HitActor && HitActor->IsA<AOJJ_Foundation>();
	// F2-1' ?ш컖吏? ?댁냼: ?됰㈃ ??+?명?) 吏?뺤? Landscape媛 而ㅼ꽌 ?뚮젅?몃낫??癒쇱? ?덊듃 ??buildable ??몃뜲
	// ?몃쾭 ?щ쭩. ? 留ㅽ븨? WorldToGrid媛 XY留??곕?濡?Z 臾닿?) ?뚮줈???덊듃? ?숈씪?섍쾶 ?듦낵?쒗궓??
	// 諛곗튂 媛???щ???湲곗〈泥섎읆 CanPlace 寃쎈줈媛 ?먯젙(寃뚯씠?몃뒗 ?쒕㈃ ?앸퀎留?.
	const bool bHitLandscape = HitActor && HitActor->IsA<ALandscapeProxy>();
	if (!bHitFloor && !bHitMachine && !bHitFoundation && !bHitLandscape)
	{
		TargetGrid->ClearHoverPreview();
		CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
		return;
	}

	// Tick留덈떎 ?몄텧?섎뒗 寃쎈줈???숈씪 ??대㈃ ISM 由щ퉴???ㅽ궢 (CursorCell? ?꾩뿉??怨꾩궛??
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

	// cursor cell ??lower-left origin (留덉슦??= ?뗮봽由고듃 以묒떖 ?뺤콉).
	// ?덉쟾??IsValidGridCell(cursor)濡?anchor ?뚯닔/珥덇낵瑜??ъ쟾 李⑤떒?덉쑝?? ??李⑤떒??
	// ?쇱そ/??寃쎄퀎 鍮꾨?移?쓣 留뚮뱾?덉쓬 (?ㅻⅨ履??꾨옒??anchor媛 valid???곹깭?먯꽌 ?뗮봽由고듃媛
	// +X,+Y濡??덉꽌 鍮④컯 ?쒖떆?섎뒗?? ?쇱そ/?꾨뒗 anchor ?먯껜媛 ?뚯닔媛 ?섏뼱 hover ?щ씪吏?.
	// ?댁젣 origin??洹몃━???뚯닔/珥덇낵?щ룄 洹몃?濡??섍? ??CanPlaceMachine???뗮봽由고듃 ?蹂?
	// IsValidGridCell 寃?щ줈 false ??UpdateHoverPreview媛 ?뗮봽由고듃 ?꾩껜 鍮④컯 (?移?.
	const FIntPoint Origin = ComputeOriginFromCursorCell(CursorCell, DefaultMachine, HoverRotationSteps);

	TargetGrid->UpdateHoverPreview(DefaultMachine, Origin, HoverRotationSteps);
	if (PlacementMode == EOJJ_BuildPlacementMode::Miner
		&& TargetGrid->CanPlaceMachine(DefaultMachine, Origin, HoverRotationSteps))
	{
		NotifyTutorialQuestEvent(this, TEXT("ValidMinerPlacement"));
	}
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
		// 而ㅼ꽌媛 洹몃━??諛????섏씠?쇱씠???쒓굅.
		TargetGrid->ClearHoverPreview();
		CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
		return;
	}

	// ?숈씪 ??대㈃ 由щ퉴???ㅽ궢(Tick 寃쎈줈). 泥좉굅 吏곹썑??DemolishUnderCursor媛 sentinel??由ъ뀑??媛뺤젣 媛깆떊.
	if (CursorCell == CurrentHoverCell)
	{
		return;
	}
	CurrentHoverCell = CursorCell;

	AActor* Target = TargetGrid->GetActorAtCell(CursorCell);
	// [WaterArea 철거 통과] GetActorAtCell이 묻힌 Foundation 위 WaterArea(액체 자원, OccupiedCells 점유)를 먼저 잡으면
	// 그 아래 Foundation 철거 타깃 조회가 가려진다. WaterArea는 철거 대상이 아니므로 무시 → 아래 파이프/Foundation 폴백 진행.
	if (Target && Target == TargetGrid->GetLiquidResourceAtCell(CursorCell))
	{
		Target = nullptr;
	}

	// F4-1(Codex ??: ?뚯씠????Foundation ?쒖꽌 ???대┃(DemolishUnderCursor)怨??숈씪 ?곗꽑?쒖쐞(?⑥씪 吏꾩떎??.
	// ?꾟넂?꾨옒(嫄대Ъ?믫뙆?댄봽?믨린珥?: Foundation ???뚯씠?꾨? ?몃쾭/?대┃ 紐⑤몢 ?뚯씠?꾨줈 ?〓뒗??
	if (!Target)
	{
		Target = TargetGrid->OJJ_GetPipeAtCell(CursorCell);
	}
	// F1-b': ?먯쑀(癒몄떊/而⑤쿋?댁뼱)쨌?뚯씠?꾧? ?녿뒗 ?? Foundation ??“??
	if (!Target)
	{
		Target = TargetGrid->GetFoundationAtCell(CursorCell);
	}

	// 鍮?? ?먮뒗 留?怨좎젙臾?愿묐㎘/WaterArea = AResourceBase)? 泥좉굅 ????꾨떂 ???섏씠?쇱씠???놁쓬.
	// [#184 철거] 사다리 폴백 — Foundation 없어도 지면 셀 키로 잡아 호버 하이라이트 가능(클릭 철거와 동일 우선순위).
	if (!Target)
	{
		Target = TargetGrid->OJJ_GetLadderAtCell(CursorCell);
	}

	if (!Target || Target->IsA<AResourceBase>())
	{
		TargetGrid->ClearHoverPreview();
		return;
	}

	// [#184 철거] 사다리는 그리드 미등록이라 GetActorCells/FoundationCells에 없다 → 단일 지면 셀(CursorCell) 강조.
	if (Cast<AOJJ_Ladder>(Target))
	{
		TargetGrid->OJJ_HighlightCellsInvalid({ CursorCell });
		return;
	}

	// 癒몄떊/而⑤쿋?댁뼱???먯쑀 留? Foundation? 而ㅻ쾭由ъ? 留????대뒓 履쎌씠?????? ?꾩껜 鍮④컯.
	const TArray<FIntPoint>* Cells = TargetGrid->GetActorCells(Target);
	if (!Cells)
	{
		// F2-0(Codex F1-b' #4): ??嫄대Ъ???덈뒗 Foundation? ?대┃(RemoveFoundation)??嫄곕??섎?濡??몃쾭??
		// ?쒖떆 ?앸왂 ???⑥씪 吏꾩떎???몃쾭 = ?대┃ ?먯젙)??泥좉굅 紐⑤뱶?먮룄 ?곸슜. 嫄곕? ?ъ쑀 ?붾㈃ ?쒖떆??UI 諛깅줈洹?
		if (TargetGrid->OJJ_CountOccupiedFoundationCells(Target) > 0)
		{
			TargetGrid->ClearHoverPreview();
			return;
		}
		Cells = TargetGrid->GetFoundationCells(Target);
		// F4-1: Foundation???꾨땲硫??뚯씠???덉씠??? ???쇱씤 ?꾩껜 鍮④컯(?대┃ 泥좉굅 ?⑥쐞? ?쇱튂).
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
		return; // 洹몃━??諛??대┃ 臾댁떆.
	}

	AActor* Target = TargetGrid->GetActorAtCell(CursorCell);
	// [WaterArea 철거 통과] GetActorAtCell이 묻힌 Foundation 위 WaterArea(액체 자원, OccupiedCells 점유)를 먼저 잡으면
	// 그 아래 Foundation 철거 타깃 조회가 가려진다. WaterArea는 철거 대상이 아니므로 무시 → 아래 파이프/Foundation 폴백 진행.
	if (Target && Target == TargetGrid->GetLiquidResourceAtCell(CursorCell))
	{
		Target = nullptr;
	}
	// F4-1(Codex ??: ?뚯씠?꾧? Foundation蹂대떎 癒쇱? ???꾟넂?꾨옒(嫄대Ъ?믫뙆?댄봽?믨린珥? 泥좉굅 ?쒖꽌. Foundation
	// ?곗꽑?대㈃ 洹????뚯씠?꾨? 吏곸젒 泥좉굅?????녾퀬(?뚯씠??遺꾧린 ?꾨떖 遺덇?), Foundation 寃뚯씠?몃뒗
	// OJJ_CountOccupiedFoundationCells???뚯씠???⑹궛??留됰뒗??嫄곕? + ?ъ쑀).
	if (!Target)
	{
		Target = TargetGrid->OJJ_GetPipeAtCell(CursorCell);
	}
	// F1-b': ?먯쑀쨌?뚯씠?꾧? ?놁쑝硫?Foundation ??“?????몃쾭(UpdateDemolishHover)? ?숈씪 ?곗꽑?쒖쐞.
	if (!Target)
	{
		Target = TargetGrid->GetFoundationAtCell(CursorCell);
	}
	// [#184 철거] 머신/파이프/Foundation 다음 우선순위로 사다리(그리드 미등록 자유액터) 폴백. 별도 사다리
	// 레이어(지면 셀 키)라 Foundation이 사라진 떠 있는 사다리도 잡힌다.
	if (!Target)
	{
		Target = TargetGrid->OJJ_GetLadderAtCell(CursorCell);
	}
	if (!Target)
	{
		return; //鍮?? 臾댁떆.
	}

	// 愿묐㎘/WaterArea(AResourceBase)??留?怨좎젙臾???泥좉굅 湲덉?.
	if (Target->IsA<AResourceBase>())
	{
		UE_LOG(LogTemp, Log, TEXT("[BuildController] 愿묐㎘/Water(AResourceBase)??泥좉굅 ??곸씠 ?꾨떂 ??臾댁떆. Cell=%s"),
			*CursorCell.ToString());
		return;
	}

	bool bRemoved = false;
	bool bRemovedEscapePod = false;

	if (AMachineBase* Machine = Cast<AMachineBase>(Target))
	{
		bRemovedEscapePod = Machine->IsA<AEscapePod>();

		// 1) ??癒몄떊???앹젏(Source/Target)?쇰줈 媛뽯뒗 而⑤쿋?댁뼱 ?쇱씤??癒쇱? ??젣(怨좎븘 諛⑹?). ??癒몄떊 ?꾩젣???쒖そ??
		//    ?щ씪吏硫??쇱씤? 議댁옱 議곌굔???껊뒗?? 而⑤쿋?댁뼱???쇱씤 ?⑥쐞(1?≫꽣=?ㅼ쨷?)???먯쑀 ? ?섎굹濡?
		//    OJJ_RemoveActorAt ?몄텧 ???쇱씤 ?꾩껜媛 ?뺣━?섍퀬, ?대? UnregisterConveyor(洹몃옒???ｌ? ?쒓굅) +
		//    RefreshPlacedMachineArrows濡?諛섎????댁븘?덈뒗) 癒몄떊??鍮??ы듃 ?붿궡?쒓? 蹂듦??쒕떎.
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
					TargetGrid->OJJ_RemoveActorAt((*ConvCells)[0]); // ?쇱씤 ?꾩껜 洹몃━???먯쑀 ?댁젣.
				}
			}
			Conveyor->RefundItemsToWarehouse();
			Conveyor->Destroy(); // ?≫꽣/鍮꾩＜???ㅼ젣 ?쒓굅 ??洹몃━???⑥닔???먯쑀 ?댁젣留? Destroy???몄텧??梨낆엫(湲곗〈 854 ?⑦꽩).
		}

		// F4-1: ?뚯씠??罹먯뒪耳?대뱶 ??而⑤쿋?댁뼱? ?숈씪 洹쇨굅(?앹젏 癒몄떊 ?뚯떎 = ?쇱씤 議댁옱 議곌굔 ?곸떎).
		// ?섏쭛? ?덉씠????갑??留?+ ?앹젏 ?議?洹몃━???ы띁) ??而⑤쿋?댁뼱???섎젅 ?ㅼ틪)蹂대떎 吏곸젒??
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
			Pipe->RefundLiquidsToWarehouse();
			Pipe->Destroy();
		}

		// 2) 癒몄떊 蹂몄껜: RemoveMachineAt ??RemoveMachine ??OnRemovedFromGrid ???먯썝 Release/Claim ?뺣━) +
		//    FactoryManager Unregister + ?붿궡???ъ쟻?? 洹????≫꽣 Destroy.
		if (TargetGrid->RemoveMachineAt(CursorCell))
		{
			// [철거 빌드다운] Destroy '직전'에 메시(단일 SMC)를 독립 프록시로 복제해 역방향 디졸브(부기/Destroy 무수정).
			StartBuildDownEffect(Machine->GetMeshComponent());
			Machine->Destroy();
			bRemoved = true;
		}
	}
	else if (AConveyor* Conveyor = Cast<AConveyor>(Target))
	{
		// 而⑤쿋?댁뼱 吏곸젒 泥좉굅: ?쇱씤 ?⑥쐞(?≫꽣 ?ㅼ쨷?) ?꾩껜 洹몃━???댁젣 + Destroy. 諛섎???癒몄떊 ?붿궡?쒕뒗 ?대? RefreshArrows濡?蹂듦?.
		if (TargetGrid->OJJ_RemoveActorAt(CursorCell))
		{
			Conveyor->RefundItemsToWarehouse();
			Conveyor->Destroy();
			bRemoved = true;
		}
	}
	else if (APipe* Pipe = Cast<APipe>(Target))
	{
		// F4-1 ?뚯씠??吏곸젒 泥좉굅: ?쇱씤 ?⑥쐞(1?≫꽣=?ㅼ쨷?) ?덉씠???댁젣 + Destroy. ??嫄대Ъ 寃뚯씠???놁쓬
		// (?뚯씠???덉씠???꾩뿏 ?꾨Т寃껊룄 ???щ씪媛?????긽 ?깃났).
		FString PipeReason;
		if (TargetGrid->OJJ_UnregisterPipeCells(Pipe, PipeReason))
		{
			Pipe->RefundLiquidsToWarehouse();
			Pipe->Destroy();
			bRemoved = true;
		}
	}
	else if (AOJJ_Foundation* Foundation = Cast<AOJJ_Foundation>(Target))
	{
		// Foundation 泥좉굅(F1-b'): RemoveFoundation??而ㅻ쾭 ? ??嫄대Ъ(?먯쑀)??寃?ы빐 嫄곕? + ?ъ쑀 諛섑솚 ??
		// ?깃났 ?쒖뿉留?Destroy(癒몄떊??RemoveMachineAt?묭estroy ?쒖꽌? ?숈씪). Destroy ??EndPlay??
		// RemoveFoundation ?ы샇異쒖? "not registered"濡??앸굹 ?댁쨷 ?댁젣 ?덉쟾(EndPlay 二쇱꽍???移?怨꾩빟).
		FString OutReason;
		if (TargetGrid->RemoveFoundation(Foundation, OutReason))
		{
			// [#184 철거 cascade] Foundation 위/인접 사다리 종속 삭제 — Foundation->Destroy() '이전'에 호출해야
			// OwningFoundation 링크가 아직 유효(머신 cascade 패턴 미러). 안 하면 사다리가 공중에 고아로 남음.
			TargetGrid->OJJ_DestroyLaddersOnFoundation(Foundation);
			// [철거 빌드다운] Destroy '직전'에 슬래브 메시(단일 SMC)를 독립 프록시로 복제해 역방향 디졸브(부기/Destroy 무수정).
			StartBuildDownEffect(Foundation->GetSlabMesh());
			Foundation->Destroy();
			bRemoved = true;
		}
		else
		{
			// 嫄곕? ?ъ쑀 ?쒖떆 ??諛곗튂 嫄곕?(TryPlaceFoundation ?ㅽ뙣)? ?숈씪 梨꾨꼸(濡쒓렇). ?? ??嫄대Ъ N?.
			UE_LOG(LogTemp, Warning, TEXT("[BuildController] Foundation 泥좉굅 嫄곕?: %s"), *OutReason);
		}
	}
	else if (AOJJ_Ladder* Ladder = Cast<AOJJ_Ladder>(Target))
	{
		// [#184 철거] 떠 있는/지상 사다리 개별 철거 — 그리드 미등록이라 OJJ_GetLadderAtCell로 잡았다.
		// 환불 없음(MVP 무료). Destroy → EndPlay가 OJJ_UnregisterLadder로 사다리 레이어 청소.
		Ladder->Destroy();
		bRemoved = true;
	}

	if (bRemoved)
	{
		NotifyTutorialQuestEvent(this, TEXT("DemolishRemoved"));
		if (bRemovedEscapePod)
		{
			NotifyTutorialQuestEvent(this, TEXT("EscapePodDemolished"));
		}
		// ?곗냽 泥좉굅: ???鍮꾩뿀?쇰땲 ?몃쾭 利됱떆 媛깆떊(sentinel 由ъ뀑 ???ㅼ쓬 UpdateMouseHover?먯꽌 鍮??濡?由щ퉴??.
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

	// footprint ???섎젅??4諛⑺뼢 ?몄젒 ????ㅼ틪(2x2/3x3??硫댁씠 ?щ윭 ? ???ы듃 ?留뚯씠 ?꾨땲???섎젅 ?꾩껜).
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

			// 寃利? ?ㅼ젣濡???癒몄떊???앹젏?쇰줈 媛뽯뒗 ?쇱씤留???젣(?섎???遺숈? ?ㅻⅨ 癒몄떊???쇱씤 ?ㅼ궘??諛⑹?).
			if (Conveyor->GetSourceMachine() == Machine || Conveyor->GetTargetMachine() == Machine)
			{
				Result.Add(Conveyor);
			}
		}
	}

	// (蹂댄뿕) ?몄젒??而⑤쿋?댁뼱媛 ?덉뿀?쇰굹 ??癒몄떊怨??곌껐??寃껋씠 0媛????섎????쇱씤?대㈃ ?뺤긽, ?꾨땲硫?"?곌껐 湲곕줉 vs ?ㅼ젣
	// ?몄젒" 遺덉씪移?洹쒖튃 ?꾨컲 ?곗씠????議곌린 ?좏샇. 洹몃━?쒕쭔?쇰줈??洹쇱궗 寃異?FactoryManager 洹몃옒??臾댁“??.
	if (Result.Num() == 0 && AdjacentConveyorCount > 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BuildController] 泥좉굅 癒몄떊 ?몄젒 而⑤쿋?댁뼱 %d媛???Source/Target ?곌껐 ?쇱튂 0. ?섎????쇱씤?대㈃ ?뺤긽, ?꾨땲硫??곌껐 ?곗씠??遺덉씪移??섏떖. Machine=%s"),
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
	// SP-only contract 媛뺤젣 (?ㅻ뜑??MULTIPLAYER LIMITATION 紐낆떆? ?쇱튂).
	// ?대씪?댁뼵?몄뿉???몄텧?섎㈃ TryPlaceMachine??HasAuthority ensure媛 ?몃━嫄곕릺怨?
	// spawn??癒몄떊? orphan?쇰줈 ?⑥쓬 ??吏꾩엯遺?먯꽌 李⑤떒.
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BuildController] OnLeftClickPressed called on non-authority ??SP-only contract"));
		return;
	}

	if (!IsInBuildMode())
	{
		return;
	}

	// [怨듭슜??Z] None = ?꾨Т寃껊룄 ?ㅺ퀬 ?덉? ?딆쓬 ???대┃ 臾대룞??諛곗튂 寃쎈줈 吏꾩엯 諛⑹?).
	if (PlacementMode == EOJJ_BuildPlacementMode::None)
	{
		return;
	}

	// Conveyor/Pipe 紐⑤뱶: 醫뚰겢由??꾨쫫 = ?쒕옒洹??쒖옉. (而ㅻ컠? OnLeftClickReleased.)
	// ?뚯씠??F4-1)???쒕옒洹??곹깭癒몄떊 怨듭슜 ???꾨━酉?而ㅻ컠留?紐⑤뱶 遺꾧린.
	if (PlacementMode == EOJJ_BuildPlacementMode::Conveyor
		|| PlacementMode == EOJJ_BuildPlacementMode::Pipe)
	{
		// [TPS 2클릭] 컨베이어·파이프 둘 다 TPS서 2클릭. 오토라우터(L자)·드래그 상태머신 공유,
		// 커밋만 bPipeMode 분기(OJJ_BuildPipePlacementPath). 탑다운은 드래그 유지.
		const bool bPathTwoClick = IsTwoClickConnectMode()
			&& (PlacementMode == EOJJ_BuildPlacementMode::Conveyor
				|| PlacementMode == EOJJ_BuildPlacementMode::Pipe);
		if (bPathTwoClick && bIsDraggingConveyor)
		{
			// 2번째 클릭 = 커밋(앵커셀↔조준셀 자동 L경로). CommitConveyorDrag가 현재 ConveyorDragCells 사용.
			CommitConveyorDrag();
			return;
		}

		FIntPoint CursorCell;
		if (GetCursorCell(CursorCell))
		{
			// #182 ?뚯씠???쒖옉 ?ㅻ깄(?뚯씠???꾩슜) ???뚰봽 蹂몄껜/異쒕젰 ?ы듃 洹쇰갑???대┃?섎㈃ ?깅줉??異쒕젰 ?ы듃 ?濡?
			// 蹂댁젙. 3횞3 ?뚰봽 諛붽묑 ??移몄쓣 ?쎌??⑥쐞濡?吏묐뒗 鍮꾪쁽?ㅼ쟻 議곗? ?쒓굅. 而⑤쿋?댁뼱??誘몄쟻???쒖옉 ?먯젙 臾대?寃?.
			if (PlacementMode == EOJJ_BuildPlacementMode::Pipe)
			{
				// 狩?1?쒖쐞: ?ㅽ겕由?怨듦컙 異쒕젰 ?ы듃 ?ㅻ깄 ???붾㈃?먯꽌 而ㅼ꽌 洹쇱쿂 ?ы듃 諛뺤뒪濡??ㅻ깄(?붾뱶 Z ?⑤윺?숈뒪 臾닿?).
				// 2?쒖쐞(?대갚): 洹몃━??? 洹쇰갑 2移??ㅻ깄. ?????ㅽ뙣硫??먮옒 ? ?좎?.
				APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
				FIntPoint SnapCell;
				// [M3] 포트스냅 기준점: TopDown=마우스 커서, TPS=화면중앙(크로스헤어). TPS는 마우스가 중앙고정/숨김이라
				// 마우스 좌표가 무의미 → 화면중앙 기준으로 출력포트 탐색해야 조준점과 일치.
				const bool bUseScreenCenter = IsTwoClickConnectMode();
				if (PC && TargetGrid->OJJ_FindLiquidOutputPortUnderCursorScreen(PC, /*MaxScreenDist=*/64.0f, SnapCell, bUseScreenCenter))
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
		// [TPS 2클릭] 탑다운=드래그(누름=앵커, 릴리스=커밋). TPS=2클릭(누름1=앵커, 누름2=커밋, 릴리스 무시).
		if (IsTwoClickConnectMode() && bIsDraggingPowerLine)
		{
			// 2번째 클릭 = 커밋(앵커 머신 ↔ 현재 조준 머신). 내부에서 CancelPowerLineDrag로 앵커 정리.
			CommitPowerLineDrag();
		}
		else
		{
			// 1번째 클릭(TPS) 또는 드래그 시작(탑다운) = 앵커 머신 픽(미스 시 BeginPowerLineDrag가 no-op).
			BeginPowerLineDrag(GetPowerLineEndpointUnderCursor());
		}
		return;
	}

	// Demolish 紐⑤뱶: 醫뚰겢由?= 而ㅼ꽌 ? ????쒓굅(癒몄떊/而⑤쿋?댁뼱/Foundation). 諛곗튂 寃쎈줈(CanPlaceMachine ??? 遺꾨━.
	if (PlacementMode == EOJJ_BuildPlacementMode::Demolish)
	{
		DemolishUnderCursor();
		return;
	}

	// Foundation 紐⑤뱶(F1-b): ?대┃ 利됱떆 諛곗튂(?쒕옒洹??놁쓬). 癒몄떊 spawn-validate-destroy ?⑦꽩 誘몃윭 ??
	// 寃利??깅줉? F1-a TryPlaceFoundation(洹몃━?쒕뒗 ?곗씠?곕쭔), ?≫꽣 ?꾩튂 ?명똿? ?ш린??
	// 癒몄떊 寃쎈줈 ?꾨떖 ??return ??#164 ?섏뒪????NotifyMainQuestMachinePlaced) 鍮꾧꼍??鍮꾧컙??.
	if (PlacementMode == EOJJ_BuildPlacementMode::Foundation)
	{
		PlaceFoundationAtCursor();
		return;
	}

	// [#184] Ladder 紐⑤뱶: ?대┃ 利됱떆 ?먯쑀 諛곗튂(?쒕옒洹??놁쓬, 洹몃━???λ? 誘몃벑濡?. Foundation 遺꾧린 誘몃윭.
	if (PlacementMode == EOJJ_BuildPlacementMode::Ladder)
	{
		PlaceLadderAtCursor();
		return;
	}

	// === Machine 紐⑤뱶 (湲곗〈 ?숈옉 臾대?寃? ===
	TSubclassOf<AMachineBase> ActiveMachineClass = GetActiveMachineClass();
	if (!TargetGrid || !ActiveMachineClass)
	{
        UE_LOG(LogTemp, Warning, TEXT("[BuildController] TargetGrid or MachineClass is missing."));
		return;
	}

	// 留덉슦?ㅺ? floor 諛뽰씠???몃쾭 媛깆떊????踰덈룄 ???먯쑝硫??대┃ 臾댁떆
	if (CurrentHoverCell.X == INT_MIN || CurrentHoverCell.Y == INT_MIN)
	{
		return;
	}

	AMachineBase* DefaultMachine = ActiveMachineClass.GetDefaultObject();
	if (!DefaultMachine)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] MachineClass CDO ?놁쓬"));
		return;
	}

	// cursor cell ??lower-left origin ??UpdateMouseHover? 媛숈? 蹂?섏쓣 ?ъ슜?댁빞 ?몃쾭
	// 誘몃━蹂닿린? ?ㅼ젣 諛곗튂 ?꾩튂媛 ?닿툔?섏? ?딆쓬. CanPlaceMachine??IsValidGridCell +
	// OccupiedCells ?듯빀 ?먯젙?섎?濡?anchor ?뚯닔/珥덇낵???먯뿰 嫄곕??????ъ쟾 bounds
	// 李⑤떒(IsValidGridCell)? ???댁긽 ?꾩슂 ?놁쓬.
	// 諛곗튂???뚯쟾 諛섏쁺(?④퀎 4). origin/CanPlace/TryPlace + 硫붿떆 yaw 紐⑤몢 媛숈? HoverRotationSteps瑜?
	// ?⑥빞 ?먯쑀쨌以묒떖쨌硫붿떆媛 ?쇱튂(Codex 吏???듭떖). ?몃쾭 誘몃━蹂닿린(UpdateMouseHover)????숈씪 step?대씪
	// "誘몃━蹂닿린 = ?ㅼ젣 諛곗튂" ?뺥빀.
	ApplyMachineDataToDefault(this, DefaultMachine);
	const FName RequiredPlacementItem = GetRequiredInventoryItemForPlacement(DefaultMachine);
	UPlayerWarehouseSubsystem* WarehouseSubsystem = nullptr;
	if (!RequiredPlacementItem.IsNone())
	{
		WarehouseSubsystem = GetWarehouseSubsystem(this);
		if (!WarehouseSubsystem || !WarehouseSubsystem->CanTakeItem(RequiredPlacementItem, 1))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[BuildController] %s item is required before placing this machine."),
				*RequiredPlacementItem.ToString());
			return;
		}
	}

	const FIntPoint Origin = ComputeOriginFromCursorCell(CurrentHoverCell, DefaultMachine, HoverRotationSteps);

	if (!TargetGrid->CanPlaceMachine(DefaultMachine, Origin, HoverRotationSteps))
	{
		UE_LOG(LogTemp, Log, TEXT("[BuildController] origin %s 諛곗튂 遺덇? (bounds/?먯쑀)"),
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
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] SpawnActor ?ㅽ뙣"));
		return;
	}

	FString OutReason;
	if (!TargetGrid->TryPlaceMachine(NewMachine, Origin, OutReason, HoverRotationSteps))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] TryPlaceMachine ?ㅽ뙣: %s"), *OutReason);
		NewMachine->Destroy();
		return;
	}

	// 硫붿떆 yaw ?뚯쟾 ??TryPlaceMachine???뚯쟾 footprint 以묒떖(GetMachinePlacementLocation(.., step))??
	// ?≫꽣瑜??볦븯?쇰?濡? 洹?以묒떖??湲곗??쇰줈 yaw留??뚮━硫?center-anchor 硫붿떆媛 ?뚯쟾 footprint? ?뺣젹.
	// ?쒓퀎諛⑺뼢 90째횞step (R 諛⑺뼢). 遺?멸? R ?섎룄? 諛섎?硫?-90.f濡?
	if (!RequiredPlacementItem.IsNone() && (!WarehouseSubsystem || !WarehouseSubsystem->TakeItem(RequiredPlacementItem, 1)))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BuildController] Failed to consume %s item after placement. Reverting placement."),
			*RequiredPlacementItem.ToString());
		TargetGrid->RemoveMachine(NewMachine);
		NewMachine->Destroy();
		return;
	}

	// [2단계] 위치+회전 결합 적용 — 평지/비채굴기는 기존(위치=Flat, yaw만)과 동일, 채굴기-경사면 지형 틸트 합성.
	// TryPlaceMachine이 이미 Flat 위치를 set했으나, 채굴기-경사는 여기서 틸트 안착 위치+회전으로 덮어쓴다(scale 불변).
	const FTransform PlaceXform =
		TargetGrid->OJJ_GetMachinePlacementTransform(NewMachine, Origin, HoverRotationSteps);
	NewMachine->SetActorLocationAndRotation(PlaceXform.GetLocation(), PlaceXform.GetRotation());
	if (IsInBuildMode())
	{
		TargetGrid->RefreshPlacedMachineArrows();
	}
	// [#3 점진 건설] 신규 배치 머신에 홀로그램 빌드업(신규 전용 — 로드는 FactorySave 경로라 안 거침).
	StartBuildUpEffect(NewMachine, NewMachine->GetMeshComponent());
	NotifyMainQuestMachinePlaced(this, GetQuestPlacementTargetId(PlacementMode));

	UE_LOG(LogTemp, Log, TEXT("[BuildController] origin %s 癒몄떊 諛곗튂 ?깃났"),
		*Origin.ToString());

	// 吏곸쟾 origin???댁젣 ?먯쑀?????ㅼ쓬 UpdateMouseHover?먯꽌 鍮④컯?쇰줈 媛뺤젣 ?ы몴??
	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
	// [손 비우기] 단일 배치(머신) 성공 후 선택 해제 — 빌드모드 유지(PlacementMode만 None). Z키와 동일 경로(고스트 숨김/호버 리셋). 컨베이어/파이프 드래그·철거는 무관.
	SetPlacementMode(EOJJ_BuildPlacementMode::None);
}

// === Foundation 紐⑤뱶 (F1-b ??癒몄떊 寃쎈줈? ?낅┰, 而ㅻ쾭由ъ? 諛곗튂) ===

TSubclassOf<AOJJ_Foundation> AOJJ_BuildController::GetActiveFoundationClass() const
{
	// F3-2.5: 醫낅쪟 ?곹깭???곕씪 ?됲뙋/?⑦봽 ?좏깮. ?⑦봽 ?좏깮 ?곹깭?몃뜲 ?대옒?ㅺ? 鍮꾩뼱 ?덈뒗 寃쎌슦??
	// OJJ_SelectFoundationKind 寃뚯씠?멸? 李⑤떒?섎?濡??뺤긽 ?먮쫫?먯꽑 ?꾨떖 遺덇? ??洹몃옒??null?대㈃
	// ?ъ슜泥??몃쾭/諛곗튂)??湲곗〈 null 媛?쒓? ?숈옉?쒕떎.
	return bRampFoundationSelected ? RampFoundationClass : FlatFoundationClass;
}

float AOJJ_BuildController::OJJ_ComputeFoundationTopZ(AOJJ_Foundation* FoundationCDO, FIntPoint Origin, FIntPoint EffSize, int32 RotationSteps) const
{
	// [#B 묻힘 금지] 파운데이션 상면 Z 단일원 — 배치 PlaceFoundationAtCursor의 BaseSurfaceZ 산식과 동일.
	// 정보 부족 시 큰 값 반환 = 묻힘 게이트 사실상 통과(거부 안 함, 안전측).
	if (!TargetGrid || !FoundationCDO)
	{
		return TNumericLimits<float>::Max();
	}
	const float PlaceZ = TargetGrid->GetFoundationPlacementLocation(Origin, EffSize).Z;
	const float SnapLift = FoundationCDO->OJJ_ComputeSnapLift(*TargetGrid, Origin, EffSize, RotationSteps, nullptr);
	const float HeightLift = GetFoundationHeightLiftZ(FoundationCDO, Origin, EffSize);
	return PlaceZ + SnapLift + HeightLift + FoundationCDO->GetThickness();
}

void AOJJ_BuildController::StartBuildUpEffect(AActor* Building, UStaticMeshComponent* Mesh)
{
	// [#3 점진 건설] 신규 배치 전용 — 동적 컴포넌트 부여 후 빌드업 시작. 머티리얼 미지정이면 컴포넌트가 무동작.
	if (!Building || !Mesh)
	{
		return;
	}
	UOJJ_HologramBuildUpComponent* Effect = NewObject<UOJJ_HologramBuildUpComponent>(Building);
	if (!Effect)
	{
		return;
	}
	Effect->HologramMaterial = HologramBuildUpMaterial; // null이면 StartBuildUp이 안전하게 skip(배치 정상).
	Effect->Duration = HologramBuildUpDuration;
	Effect->RegisterComponent();
	Effect->StartBuildUp(Mesh);
}

void AOJJ_BuildController::StartBuildDownEffect(UStaticMeshComponent* Mesh)
{
	// [철거 빌드다운] 철거 대상 메시를 역방향(위→아래) 빨강 홀로그램으로 디졸브 — 대상 액터는 즉시 Destroy되므로
	// 연출은 독립 프록시 액터(AOJJ_DemolishEffectActor)로 분리해 자체 소멸시킨다. 머티리얼 미지정/메시 null이면 무동작(철거 정상).
	if (!Mesh || !HologramBuildUpMaterial)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// 대상 트랜스폼 자리에 스폰(프록시 액터의 위치 — 실제 디졸브 프록시는 컴포넌트가 Source 트랜스폼으로 자체 정렬).
	const FTransform SpawnXf = Mesh->GetComponentTransform();
	AOJJ_DemolishEffectActor* Fx = World->SpawnActor<AOJJ_DemolishEffectActor>(
		AOJJ_DemolishEffectActor::StaticClass(), SpawnXf);
	if (Fx)
	{
		// Mesh를 동기 복제(대상 Destroy 직전 호출이라 메시/인스턴스 유효). 시작 못 하면 프록시가 자체 소멸.
		Fx->StartBuildDown(Mesh, HologramBuildUpMaterial, BuildDownColor, BuildDownDuration);
	}
}

void AOJJ_BuildController::StartPathBuildUpEffect(AActor* Building, const TArray<FIntPoint>& Cells)
{
	// [#3 확장] 컨베이어/파이프 — 신규 배치 전용. 경로 시작/끝 월드좌표 산출 + ISM 머티 스왑 빌드업.
	if (!Building || !TargetGrid || Cells.Num() == 0)
	{
		return;
	}
	const FVector PathStart = TargetGrid->GridToWorld(Cells[0]);
	const FVector PathEnd = TargetGrid->GridToWorld(Cells.Last());

	TArray<UInstancedStaticMeshComponent*> ISMs;
	Building->GetComponents<UInstancedStaticMeshComponent>(ISMs);
	if (ISMs.Num() == 0)
	{
		return;
	}

	UOJJ_HologramPathBuildUpComponent* Effect = NewObject<UOJJ_HologramPathBuildUpComponent>(Building);
	if (!Effect)
	{
		return;
	}
	Effect->PathHologramMaterial = HologramPathMaterial; // null이면 StartBuildUp이 안전하게 skip(배치 정상).
	Effect->Duration = HologramBuildUpDuration;
	Effect->RegisterComponent();
	Effect->StartBuildUp(ISMs, PathStart, PathEnd);
}

void AOJJ_BuildController::UpdateFoundationHover(FIntPoint CursorCell, const FHitResult& Hit)
{
	// 癒몄떊 ?몃쾭? ?숈씪???쒕㈃ 寃뚯씠?? floor/癒몄떊 ?꾩뿉?쒕쭔 ?좏슚(洹????쒕㈃? off-grid ???꾨━酉??대━??.
	// 癒몄떊 ???몃쾭???먯쑀 ?濡?留ㅽ븨??CanPlaceFoundation occupied 寃뚯씠?멸? 鍮④컯 ?쒖떆 ???섎룄???쇰뱶諛?
	// (諛곗튂??Foundation ?щ옒釉뚮뒗 NoCollision?대씪 ?몃젅?댁뒪媛 ?듦낵??floor???우쓬 ??寃뱀묠 鍮④컯???뺤긽 ?숈옉.)
	UPrimitiveComponent* HitComp = Hit.GetComponent();
	AActor* HitActor = Hit.GetActor();
	const bool bHitFloor = (HitComp == TargetGrid->GetGridFloorMesh());
	const bool bHitMachine = HitActor && HitActor->IsA<AMachineBase>();
	// F1-c: 湲곗〈 ?щ옒釉????몃쾭???좏슚(寃뱀묠? CanPlaceFoundation??鍮④컯?쇰줈 ???몄젒 ?뺤옣 諛곗튂 UX).
	const bool bHitFoundation = HitActor && HitActor->IsA<AOJJ_Foundation>();
	// F2-1' ?ш컖吏? ?댁냼: ?됰㈃ ??+?명?) 吏?뺤쓽 Landscape ?좏엳???덉슜 ??癒몄떊 寃뚯씠?몄? ?숈씪 ?ъ쑀/泥섎━.
	const bool bHitLandscape = HitActor && HitActor->IsA<ALandscapeProxy>();
	if (!bHitFloor && !bHitMachine && !bHitFoundation && !bHitLandscape)
	{
		TargetGrid->ClearHoverPreview();
		CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
		return;
	}

	// 癒몄떊 寃쎈줈? ?숈씪???숈씪-? ISM 由щ퉴???ㅽ궢(Tick 寃쎈줈 鍮꾩슜 ?덇컧).
	if (CursorCell == CurrentHoverCell)
	{
		return;
	}

	const TSubclassOf<AOJJ_Foundation> ActiveClass = GetActiveFoundationClass();
	const AOJJ_Foundation* DefaultFoundation = ActiveClass ? ActiveClass.GetDefaultObject() : nullptr;
	if (!DefaultFoundation)
	{
		// ?쒖꽦 ?대옒?ㅺ? 鍮꾩뼱 ?덉쑝硫??? ?⑦봽 ?좏깮 ?곹깭?먯꽌 PIE 以??먮뵒?곕줈 ?대옒??鍮꾩?) ?댁쟾 醫낅쪟
		// ?꾨━酉곌? stale濡??⑥? ?딄쾶 ?뺣━ ??諛섑솚(Codex F3-2.5 ??RISK 諛⑹뼱).
		TargetGrid->ClearHoverPreview();
		CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
		return;
	}

	// F3.6-0(??: ?뗮봽由고듃??CDO ?낆씠 ?곗텧 ??踰좎씠?ㅻ뒗 湲곗〈 ?뺤쟻 ?곗텧(???step ?ㅼ솑 + origin 怨듯넻
	// ?섏떇)怨??숈옉 ?숈씪(?뚭? 0). ?먮룞 留욎땄 ?⑦봽(F3.6-1)遺??而ㅼ꽌+洹몃━???곹깭 湲곕컲 ?숈쟻 ?뗮봽由고듃媛
	// override濡??ㅼ뼱?⑤떎(CDO ?몄텧 ??spawn 遺?묒슜 ?놁쓬? 醫낆쟾怨??숈씪).
	const FOJJFoundationFitResult Fit = DefaultFoundation->OJJ_ComputeHoverFootprint(
		*TargetGrid, CursorCell, HoverRotationSteps);
	// F3.6-1(??: ?뗮봽由고듃 援ъ꽦 遺덇?(?먮룞 留욎땄 寃쎌궗 ?쒓퀎)???대┃??媛숈? ??bValid濡?嫄곕? ??鍮④컯 媛뺤젣濡?
	// ???⑥씪 吏꾩떎???좎?. ?ъ쑀 ?띿뒪?몃뒗 ?대┃ ??濡쒓렇(?꾨옒 ?몃쾭 濡쒓렇?먮룄 ?숇컲 ??? 蹂寃??쒕쭔?대씪 ?鍮덈룄).
	// [공중 Foundation] 높이 오프셋 Z 1회 계산 — 타일 프리뷰·슬래브 고스트·스폰이 동일 값을 쓰도록(이웃 조회 중복 방지).
	const float FoundationHeightLiftZ = GetFoundationHeightLiftZ(DefaultFoundation, Fit.Origin, Fit.EffSize);
	// [#B 묻힘 금지] 상면 Z 단일원 + 램프 예외(램프는 묻힘 허용). 호버 타일색/고스트/배치가 같은 TopZ로 정합.
	const float FoundationTopZ = OJJ_ComputeFoundationTopZ(ActiveClass.GetDefaultObject(), Fit.Origin, Fit.EffSize, Fit.EffectiveRotationSteps);
	const bool bRejectBuried = !bRampFoundationSelected;
	TargetGrid->OJJ_UpdateFoundationHoverPreview(Fit.Origin, Fit.EffSize, !Fit.bValid, FoundationHeightLiftZ, FoundationTopZ, bRejectBuried);

	// 怨좎뒪???꾨━酉?#187): ?됲뙋 ?꾩슜. ???먯젙?먯? ?몃쾭 ??쇨낵 ?숈씪(Fit.bValid AND CanPlaceFoundation)濡??쇱튂.
	// ?⑦봽???꾩냽 踰붿쐞??怨좎뒪??誘명몴????OJJ_HideGhost濡??꾪솚 ?붿〈 諛⑹?(ClearHoverPreview???④린吏留?紐낆떆??.
	if (!bRampFoundationSelected)
	{
		FString GhostReason;
		// 평탄 고스트 — 묻힘 거부 적용(bRejectBuried=true). 램프 분기(else)는 면제.
		const bool bGhostValid = Fit.bValid && TargetGrid->CanPlaceFoundation(Fit.Origin, Fit.EffSize, GhostReason, FoundationTopZ, /*bRejectBuried=*/true);
		TargetGrid->OJJ_ShowGhostForFoundation(
			ActiveClass.GetDefaultObject(), Fit.Origin, Fit.EffSize, bGhostValid, FoundationHeightLiftZ);
	}
	else
	{
		// 램프 고스트: 평판 박스가 아니라 경사 Deck 메시로(OJJ_ShowGhostForRamp). 색 판정은 평판과 동일 단일원.
		FString RampGhostReason;
		const bool bRampGhostValid = Fit.bValid && TargetGrid->CanPlaceFoundation(Fit.Origin, Fit.EffSize, RampGhostReason);
		TargetGrid->OJJ_ShowGhostForRamp(
			ActiveClass.GetDefaultObject(), Fit.Origin, Fit.EffSize,
			Fit.EffectiveRotationSteps, Fit.RiseSteps, bRampGhostValid);
	}
	// ??蹂닿컯: 諛⑺뼢 異쒖쿂 ?쒖떆 ???먮룞(?댁썐 ??넂?? vs ?섎룞(R) ?댁썝?붿쓽 UX 諛⑹뼱. ?됲뙋? 異쒖쿂媛 鍮꾩뼱
	// ?덉뼱 臾대줈洹??ㅽ뙵 0), ?⑦봽留?? 蹂寃???1以?
	if (!Fit.DirectionSource.IsEmpty())
	{
		const FString InvalidSuffix = Fit.bValid
			? FString()
			: FString::Printf(TEXT(" ??援ъ꽦 遺덇?: %s"), *Fit.FailReason);
		UE_LOG(LogTemp, Log, TEXT("[BuildController] ?⑦봽 ?뗮봽由고듃 %s%s"), *Fit.DirectionSource, *InvalidSuffix);
	}
	CurrentHoverCell = CursorCell;
}

void AOJJ_BuildController::PlaceFoundationAtCursor()
{
	const TSubclassOf<AOJJ_Foundation> ActiveClass = GetActiveFoundationClass();
	if (!TargetGrid || !ActiveClass)
	{
		// F3-2.5 留덉씠洹몃젅?댁뀡 ?덈궡: 援?FoundationClass 吏?뺤? CoreRedirects媛 FlatFoundationClass濡?
		// ?닿? ??洹몃옒??鍮꾩뼱 ?덉쑝硫?BP/?덈꺼 ?몄뒪?댁뒪?먯꽌 ?ъ????꾩슂.
		UE_LOG(LogTemp, Warning,
			TEXT("[BuildController] TargetGrid ?먮뒗 %s 誘몄꽕????BP/?덈꺼 ?몄뒪?댁뒪?먯꽌 吏???뺤씤(援?FoundationClass??FlatFoundationClass濡?媛쒕챸??"),
			bRampFoundationSelected ? TEXT("RampFoundationClass") : TEXT("FlatFoundationClass"));
		return;
	}

	// 留덉슦?ㅺ? floor 諛뽰씠???몃쾭 媛깆떊????踰덈룄 ???먯쑝硫??대┃ 臾댁떆(癒몄떊 寃쎈줈? ?숈씪).
	if (CurrentHoverCell.X == INT_MIN || CurrentHoverCell.Y == INT_MIN)
	{
		return;
	}

	const AOJJ_Foundation* DefaultFoundation = ActiveClass.GetDefaultObject();
	if (!DefaultFoundation)
	{
		return;
	}

	// ?몃쾭? 媛숈? ?뗮봽由고듃 ?낆쓣 ?ъ슜?댁빞 "誘몃━蹂닿린 = ?ㅼ젣 諛곗튂" ?뺥빀(癒몄떊 寃쎈줈???듭떖 怨꾩빟怨??숈씪 ??
	// F3.6-0 ?? CDO ?뺤쟻 ?곗텧 ???? 媛숈? ?낅젰(?쨌?뚯쟾)?대㈃ 媛숈? 寃곌낵 ???대┃ ???ъ궛異쒖씠 吏꾩떎??.
	const FOJJFoundationFitResult Fit = DefaultFoundation->OJJ_ComputeHoverFootprint(
		*TargetGrid, CurrentHoverCell, HoverRotationSteps);
	if (!Fit.bValid)
	{
		// ?뗮봽由고듃 援ъ꽦 遺덇?(F3.6-1 ?먮룞 留욎땄 寃쎌궗 ?쒓퀎 誘몃떖 ????踰좎씠??怨좎젙 ?⑦봽????긽 valid).
		UE_LOG(LogTemp, Log, TEXT("[BuildController] Foundation 諛곗튂 嫄곕?(?뗮봽由고듃): %s"), *Fit.FailReason);
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
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] Foundation SpawnActor ?ㅽ뙣"));
		return;
	}

	// F3.6-1: ?숈쟻 ?뗮봽由고듃 ?뺤젙媛??먮룞 留욎땄 湲몄씠/?⑥닔)???≫꽣???듭? ???깅줉 ?꾩뿉 ??ν빐
	// 鍮꾩＜??OJJ_NotifyPlacedOnGrid ??UpdateSlabVisual)???깅줉 ?곗씠?곗? 媛숈? 洹쒓꺽?쇰줈 洹몃┛??
	NewFoundation->OJJ_NotifyFitResult(Fit);

	// SurfaceZ = ?됰㈃ + Thickness + ?ㅻ깄 由ы봽??F2-4 짠5-4 ???뗮봽由고듃 GroundZ 理쒓퀬?먯쓽 N횞100?? ?됲깂 N=0 =
	// F1 ?숈옉). 醫뚰몴/由ы봽?몃뒗 洹몃━???ы띁(寃곗젙???????곗씠??醫뚰몴??洹몃━?? ?≫꽣 ?대룞? 而⑦듃濡ㅻ윭).
	// ?≫꽣???듭㎏濡?由ы봽?몃쭔???????щ옒釉??곷㈃(?≫꽣Z+Thickness)??SurfaceZ? ?먮룞 ?쇱튂. ?ㅽ뙣 ??利됱떆 ?뚭린.
	const FVector PlaceLocation = TargetGrid->GetFoundationPlacementLocation(Origin, EffSize);
	// ?믪씠 寃곗젙? ?대옒????F3.5 ?곗꽑?쒖쐞: ???댁썐 ?곸냽 ????吏???⑥븮 / ?⑦봽 ???ｌ? ?ㅻ깄 ???대갚).
	// HeightSource??諛곗튂 濡쒓렇??異쒖쿂(寃곗젙 ??蹂닿컯 ???뺤콉 ?숈옉 ?ㅼ륫).
	// F3.6-1(??: ?뚯쟾? ?낆씠 ?뺤젙???좏슚 step ???먮룞 留욎땄? 遺????넂??媛 ?댁썐?먯꽌 ?먮룞, 洹??몃뒗
	// ?낅젰 step 洹몃?濡? ?ㅻ깄/?쇱? ?곗떇/?≫꽣 yaw媛 ?꾨? 媛숈? 媛믪쓣 ?⑥빞 ??? ???먯젙?????닿툔?쒕떎.
	FString HeightSource;
	const float SnapLift = NewFoundation->OJJ_ComputeSnapLift(
		*TargetGrid, Origin, EffSize, Fit.EffectiveRotationSteps, &HeightSource);
	// [공중 Foundation] 지형 스냅 위에 빌드 높이 오프셋(층×100, TPS Flat 전용) 추가 적층 → 허공 배치.
	// SnappedLocation이 액터 위치 + 등록 SurfaceZ(BaseSurfaceZ) 둘 다의 기준이라 한 곳만 더하면 둘 다 반영.
	// 다리(LegISM)는 OJJ_NotifyPlacedOnGrid→UpdateLegVisual이 ActorZ↔지형 차이로 자동 연장(무작업).
	const FVector SnappedLocation = PlaceLocation + FVector(0.0f, 0.0f, SnapLift + GetFoundationHeightLiftZ(NewFoundation, Origin, EffSize));
	const float BaseSurfaceZ = SnappedLocation.Z + NewFoundation->GetThickness();

	// F3-2: 鍮꾪룊???⑦봽) Foundation? ?蹂?SurfaceZ ???곗떇? ?대옒??梨낆엫(寃곗젙 ??, ?깅줉? PerCell 寃쎌쑀
	// (洹몃━?쒓? 遺덈???寃利?. ?됲깂? 湲곗〈 ?⑥씪媛?寃쎈줈 洹몃?濡?諛곗뿴 誘몄깮??.
	FString OutReason;
	TArray<float> CellZs;
	// #261: ?쒖そ 吏硫??⑦봽硫??깅줉 ?λ???吏硫??꾨옒 ???吏硫댁쑝濡??대옩???ㅻ쾭?덉씠/?대┃/而⑤쿋?댁뼱媛 吏硫댁뿉??留뚮궓).
	// span 寃利앹? ?먮낯 CellZs濡?洹몃?濡? ?먭린 硫붿떆??蹂꾧컻 ?곗텧?대씪 臾대?寃????쇰컲/?묒そ ?⑦봽??false???곹뼢 ?놁쓬.
	const bool bPlaced = NewFoundation->OJJ_BuildPerCellSurfaceZ(EffSize, Fit.EffectiveRotationSteps, BaseSurfaceZ, Fit.RiseSteps, CellZs)
		? TargetGrid->OJJ_TryPlaceFoundationPerCell(NewFoundation, Origin, EffSize, CellZs, OutReason, Fit.bOneSideGroundRamp)
		: TargetGrid->TryPlaceFoundation(NewFoundation, Origin, EffSize, BaseSurfaceZ, OutReason);
	if (!bPlaced)
	{
		// OutReason???ъ쑀蹂?? ??water/occupied/overlap ?? ??F1-b ?붾쾭源끒톣aterZ ?ш????ㅼ륫 ?곗씠??
		UE_LOG(LogTemp, Log, TEXT("[BuildController] Foundation 諛곗튂 遺덇?: %s"), *OutReason);
		NewFoundation->Destroy();
		return;
	}

	// F3-0(??: ?≫꽣 yaw = 90째횞step ??濡쒖뺄 Size 硫붿떆媛 ?붾뱶?먯꽌 EffSize ?뗮봽由고듃? ?뺣젹(癒몄떊 :873 ?⑦꽩).
	// ?뺤궗媛??됲뙋 ?먮툕???쒓컖 ?숈씪(?뚭? 0). step? ???뺤젙媛?F3.6-1 ?????곗떇怨??숈씪 ?뚯쟾 洹쒖빟).
	NewFoundation->SetActorLocationAndRotation(
		SnappedLocation, FRotator(0.0f, 90.0f * Fit.EffectiveRotationSteps, 0.0f));
	NewFoundation->OJJ_NotifyPlacedOnGrid(TargetGrid);
	// [#3 점진 건설] 신규 배치 파운데이션에 홀로그램 빌드업(v1 슬래브만 — LegISM 다리는 후속). 신규 전용.
	StartBuildUpEffect(NewFoundation, NewFoundation->GetSlabMesh());

	// N + ?믪씠 異쒖쿂(寃곗젙 ?ㅒ룔돴 蹂닿컯) + 諛⑺뼢 異쒖쿂(??蹂닿컯 ???먮룞/?섎룞) 湲곕줉 ???뺤콉 ?숈옉 ?ㅼ륫.
	UE_LOG(LogTemp, Log, TEXT("[BuildController] origin %s Foundation 諛곗튂 ?깃났 (%dx%d, R=%d, N=%d?? %s%s%s)"),
		*Origin.ToString(), EffSize.X, EffSize.Y, Fit.EffectiveRotationSteps,
		FMath::RoundToInt(SnapLift / AOJJ_Grid::OJJ_FoundationSnapStep), *HeightSource,
		Fit.DirectionSource.IsEmpty() ? TEXT("") : TEXT(", "), *Fit.DirectionSource);
	NotifyTutorialQuestEvent(this, bRampFoundationSelected ? TEXT("PlaceRampFoundation") : TEXT("PlaceFlatFoundation"));

	// F2-4 ?꾩냽 ?? ?뗮봽由고듃??源붾┛ Pawn???곷㈃?쇰줈 ?щ젮?쒖?(F3-2遺???蹂?SurfaceZ ???깅줉 ?곗씠?곕?
	// 洹몃━?쒖뿉???쎌쓬). ?꾩냽 ??罹먯떆??由ъ뀑 ???? 洹몃?濡쒖뿬??鍮꾩＜??Z媛 ?곷㈃?쇰줈 諛붾뚮?濡?媛뺤젣 ?ъ쟻??
	OJJ_LiftPawnsOntoFoundation(Origin, EffSize, NewFoundation->GetThickness());
	CharacterOverlayCells.Reset();

	// 吏곸쟾 ?곸뿭???댁젣 而ㅻ쾭??寃뱀묠 湲덉?) ???ㅼ쓬 ?몃쾭?먯꽌 鍮④컯 ?ы몴??媛뺤젣(癒몄떊 寃쎈줈? ?숈씪).
	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
	// [손 비우기] 단일 배치(파운데이션) 성공 후 선택 해제 — 빌드모드 유지. 머신과 동일.
	SetPlacementMode(EOJJ_BuildPlacementMode::None);
}

void AOJJ_BuildController::OJJ_LiftPawnsOntoFoundation(FIntPoint Origin, FIntPoint Size, float SlabThickness)
{
	// ?쒕쾭 沅뚯쐞 ??諛곗튂(TryPlaceFoundation??HasAuthority)? 媛숈? ?먮쫫. 紐⑤뱺 Pawn ???硫???鍮?.
	// 諛곗튂 ?깃났 吏곹썑 ?몄텧?섎?濡??蹂?SurfaceZ??洹몃━???깅줉 ?곗씠??GetFoundationSurfaceZ)媛 吏꾩떎????
	// ?됲뙋(??? ?숈씪)怨??⑦봽(F3-2 怨꾨떒)瑜?媛숈? 肄붾뱶濡?泥섎━(??.
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

		// 罹≪뒓??嫄몄튇 ? 踰붿쐞(WorldToGrid ??XY ?꾩슜, ? 諛섏삱由?洹쒖튃 怨듭쑀) ???뗮봽由고듃.
		const FIntPoint MinCell = TargetGrid->WorldToGrid(Loc - FVector(Radius, Radius, 0.0f));
		const FIntPoint MaxCell = TargetGrid->WorldToGrid(Loc + FVector(Radius, Radius, 0.0f));
		const int32 IterMinX = FMath::Max(MinCell.X, Origin.X);
		const int32 IterMaxX = FMath::Min(MaxCell.X, Origin.X + Size.X - 1);
		const int32 IterMinY = FMath::Max(MinCell.Y, Origin.Y);
		const int32 IterMaxY = FMath::Min(MaxCell.Y, Origin.Y + Size.Y - 1);

		// ?蹂??먯젙: 罹≪뒓??洹?? ?щ옒釉?援ш컙 [?곷㈃?믩몢猿? ?곷㈃]怨?寃뱀튌 ?뚮쭔 ??諛쒖씠 ?대? ?곷㈃ ?댁긽?대㈃
		// no-op, 癒몃━媛 ?щ옒釉?諛붾떏 ?꾨옒硫??믪? ??諛?媛?蹂댄뻾 ??寃곗젙 ?? 媛꾩꽠 ?놁쓬. ?щ┝ 紐⑺몴??
		// 嫄몃┛ ? ?곷㈃??max(?⑦봽 ?꾨㈃ ???믪? ??湲곗? ???щ겮??諛⑹?).
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

		// ?곷㈃ + 罹≪뒓 諛섎넂??+2 珥덇린 移⑦닾 諛⑹? ??李⑹???以묐젰???뺣━). XY ?좎?("源붾㈃ ?щ씪??).
		// ??怨듦컙???ㅻⅨ ?≫꽣濡?留됲엺 寃쎌슦???뺢탳??泥섎━??諛깅줈洹????쇰떒 ?щ━怨?濡쒓렇濡?異붿쟻.
		const FVector NewLoc(Loc.X, Loc.Y, LiftToZ + HalfHeight + 2.0f);
		Pawn->SetActorLocation(NewLoc, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
		UE_LOG(LogTemp, Log, TEXT("[BuildController] Foundation 諛곗튂 ??Pawn ?щ젮?쒖?: %s Z %.1f??.1f (?곷㈃ %.1f)"),
			*Pawn->GetName(), Loc.Z, NewLoc.Z, LiftToZ);
	}
}

void AOJJ_BuildController::UpdateCharacterCellOverlay()
{
	if (!TargetGrid)
	{
		return;
	}

	// 濡쒖뺄 ?뚮젅?댁뼱留?F2-4 ?꾩냽 ????? ?뚮젅?댁뼱 ?쒖떆??諛깅줈洹?. Pawn ?놁쓬(愿?????대㈃ ?쒖떆 ?쒓굅 寃쎈줈.
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;

	TArray<FIntPoint> Cells;
	if (Pawn)
	{
		float Radius = 0.0f;
		float HalfHeight = 0.0f;
		Pawn->GetSimpleCollisionCylinder(Radius, HalfHeight);
		const FVector Loc = Pawn->GetActorLocation();
		// 罹≪뒓 ?뗮봽由고듃媛 嫄몄튇 ?(蹂댄넻 1~2, 理쒕? 4) ??WorldToGrid媛 XY留??곕?濡?Z 臾닿?. off-grid???쒖쇅.
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

	// ? 醫뚰몴 蹂寃??쒖뿉留?ISM ?щ퉴??怨꾩빟 ??鍮꾧탳?????먯냼????鍮꾩슜 臾댁떆 媛??.
	if (Cells != CharacterOverlayCells)
	{
		CharacterOverlayCells = Cells;
		TargetGrid->OJJ_UpdateCharacterCellOverlay(Cells);
	}
}

// === Conveyor ?낅젰 (Step 6 ??Dummy ?먮낯 ?댁떇(parity)) ===

void AOJJ_BuildController::OnLeftClickReleased()
{
	// [TPS 2클릭] TPS 컨베이어·파이프는 누름2에서 커밋 → 릴리스 커밋 억제(억제 안 하면 앵커 클릭 직후 릴리스에 즉시 커밋돼 깨짐).
	// 탑다운은 둘 다 릴리스=커밋(드래그).
	if (PlacementMode == EOJJ_BuildPlacementMode::Conveyor
		|| PlacementMode == EOJJ_BuildPlacementMode::Pipe)
	{
		const bool bPathTwoClick = IsTwoClickConnectMode()
			&& (PlacementMode == EOJJ_BuildPlacementMode::Conveyor
				|| PlacementMode == EOJJ_BuildPlacementMode::Pipe);
		if (!bPathTwoClick)
		{
			CommitConveyorDrag();
		}
	}
	else if (PlacementMode == EOJJ_BuildPlacementMode::PowerLine)
	{
		// [TPS 2클릭] 전선만 TPS에서 누름2(OnLeftClickPressed)에서 커밋 → 릴리스 커밋 억제.
		// (릴리스 커밋이 살아 있으면 앵커 클릭 직후 릴리스에 즉시 커밋돼 2클릭이 깨짐.) 탑다운은 릴리스=커밋.
		if (!IsTwoClickConnectMode())
		{
			CommitPowerLineDrag();
		}
	}
}

bool AOJJ_BuildController::ComputeLadderPlacement(
	FIntPoint CursorCell,
	FVector& OutBottomLocation,
	float& OutClimbHeight,
	FRotator& OutRotation) const
{
	if (!TargetGrid)
	{
		return false;
	}

	// 而ㅼ꽌 ?????뒗 Foundation??李얜뒗???щ옒釉뚮뒗 NoCollision?대씪 ?덉씠??floor瑜?留욎?留?XY ?? Foundation ??.
	AActor* Foundation = TargetGrid->GetFoundationAtCell(CursorCell);
	if (!Foundation)
	{
		return false; // Foundation ?꾧? ?꾨떂 ??臾댄슚(?몄텧?먭? 鍮④컯/?④? 泥섎━).
	}
	const TArray<FIntPoint>* FootprintCells = TargetGrid->GetFoundationCells(Foundation);
	if (!FootprintCells || FootprintCells->Num() == 0)
	{
		return false;
	}

	// 湲곗???= 而ㅼ꽌 ? 以묒떖. ?몃쾭/?대┃ 紐⑤몢 ? ?⑥쐞??媛숈? ?낅젰 ??媛숈? 蹂(誘몃━蹂닿린 = 諛곗튂 寃곗젙???뺥빀).
	const FVector CursorRef = TargetGrid->GridToWorld(CursorCell);

	// 4諛⑺뼢 ??OJJ_Grid??OJJ_NeighborSteps? ?숈씪(洹?諛곗뿴? ?뚯씪 濡쒖뺄?대씪 ?ш린 ?숈씪 媛??ъ꽑??.
	static const FIntPoint Cardinals[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };

	// 寃쎄퀎 蹂 ?꾨낫: ?뗮봽由고듃 ? 以??댁썐??鍮?Foundation(吏硫??닿퀬 洹몃━???덉씤 (蹂?, 諛붽묑諛⑺뼢, 諛붽묑 吏硫댁?).
	// 而ㅼ꽌 湲곗??먯뿉 XY 理쒓렐?묒씤 蹂? ?좏깮 ??"而ㅼ꽌濡?議곗???蹂".
	bool bFound = false;
	FIntPoint BestEdgeCell(0, 0);     // Foundation ?곷㈃ Z(蹂 ?)
	FIntPoint BestGroundCell(0, 0);   // ?щ떎由?諛붾떏/吏硫?Z(諛붽묑 ?몄젒 ?)
	FIntPoint BestOutwardStep(0, 0);  // 諛붽묑(吏硫? 諛⑺뼢
	float BestDistSq = TNumericLimits<float>::Max();
	for (const FIntPoint& Cell : *FootprintCells)
	{
		for (const FIntPoint& Step : Cardinals)
		{
			const FIntPoint Neighbor = Cell + Step;
			if (TargetGrid->IsCellOnFoundation(Neighbor))
			{
				continue; // ?댁썐??Foundation ???대?(蹂 ?꾨떂).
			}
			if (!TargetGrid->IsValidGridCell(Neighbor))
			{
				continue; // 洹몃━??諛???諛붽묑 吏硫?? ?곗텧 遺덇?.
			}
			const FVector EdgeCenter = TargetGrid->GridToWorld(Cell);
			const float dx = EdgeCenter.X - CursorRef.X;
			const float dy = EdgeCenter.Y - CursorRef.Y;
			const float DistSq = dx * dx + dy * dy;
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				BestEdgeCell = Cell;
				BestGroundCell = Neighbor;
				BestOutwardStep = Step;
				bFound = true;
			}
		}
	}
	if (!bFound)
	{
		return false;
	}

	// ?믪씠 = Foundation ?곷㈃ Z(蹂 ?) ??吏硫?Z(諛붽묑 ?몄젒 ?).
	float TopZ = 0.0f;
	if (!TargetGrid->GetFoundationSurfaceZ(BestEdgeCell, TopZ))
	{
		return false;
	}
	float GroundZ = 0.0f;
	if (!TargetGrid->OJJ_GetRawTerrainSurfaceZ(BestGroundCell, GroundZ))
	{
		// 踰좎씠??吏??Z ?놁쑝硫?誘몃쿋?댄겕) 洹몃━???됰㈃ Z ?대갚 ???됰㈃ ?덈꺼?먯꽌???숈옉.
		GroundZ = TargetGrid->GridToWorld(BestGroundCell).Z;
	}
	const float Height = TopZ - GroundZ;
	if (Height <= 1.0f)
	{
		return false; // 吏硫닿낵 ?숈씪/??쟾 ???щ떎由?遺덊븘??臾댄슚).
	}

	// 諛붾떏 XY = Foundation 踰쎈㈃(edge ? ??諛붽묑 吏硫?? 寃쎄퀎 ?쇱씤)???щ떎由?以묒떖??遺숈씤??#184 踰쎈㈃ ?뺣젹).
	// 諛붽묑 吏硫?? "以묒븰"???먮㈃ 踰쎌뿉??half-cell ?⑥뼱??蹂댁씠誘濡? ??? 以묒떖??以묒젏(=?뺥솗??踰쎈㈃ ?쇱씤)???대떎.
	// 蹂留덈떎 踰뺤꽑 諛⑺뼢???щ씪??以묒젏???먮룞?쇰줈 洹?蹂??踰쎈㈃??留욎쓬(遺???????怨듯넻, CellSize 臾댁쓽議?. Z??吏硫??좎?.
	// ?좑툘 硫붿떆 ?먭퍡(placeholder Cube ~20uu) 蹂댁젙? PIE ??誘몄꽭議곗젙 ???곗꽑 踰쎈㈃ ?쇱씤??以묒떖 ?뺣젹.
	const FVector EdgeCenter = TargetGrid->GridToWorld(BestEdgeCell);
	const FVector GroundCenter = TargetGrid->GridToWorld(BestGroundCell);
	const FVector WallMid = (EdgeCenter + GroundCenter) * 0.5f;
	OutBottomLocation = FVector(WallMid.X, WallMid.Y, GroundZ);
	const FVector InwardDir(-(float)BestOutwardStep.X, -(float)BestOutwardStep.Y, 0.0f);
	OutRotation = InwardDir.Rotation(); // cardinal ??yaw 0/90/180/270 (?꾨갑 +X = Foundation ?ν븿)
	OutClimbHeight = Height;
	return true;
}

void AOJJ_BuildController::UpdateLadderHover(FIntPoint CursorCell, const FHitResult& Hit)
{
	// ?쒕㈃ 寃뚯씠????Foundation ?몃쾭? ?숈씪(floor/癒몄떊/Foundation/Landscape ?꾩뿉?쒕쭔 ?좏슚, 洹???off-grid).
	UPrimitiveComponent* HitComp = Hit.GetComponent();
	AActor* HitActor = Hit.GetActor();
	const bool bHitFloor = (HitComp == TargetGrid->GetGridFloorMesh());
	const bool bHitMachine = HitActor && HitActor->IsA<AMachineBase>();
	const bool bHitFoundation = HitActor && HitActor->IsA<AOJJ_Foundation>();
	const bool bHitLandscape = HitActor && HitActor->IsA<ALandscapeProxy>();
	if (!bHitFloor && !bHitMachine && !bHitFoundation && !bHitLandscape)
	{
		// ClearHoverPreview = ISM ????붿궡???대━??+ 怨좎뒪???④?(OJJ_HideGhost留뚯쑝濡?ISM ?붿〈). off-grid ?댁옣 ?뺣━.
		TargetGrid->ClearHoverPreview();
		CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
		return;
	}

	// ?숈씪-? ISM/怨좎뒪??由щ퉴???ㅽ궢(Tick 鍮꾩슜 ?덇컧) ??癒몄떊/Foundation 寃쎈줈? ?숈씪.
	if (CursorCell == CurrentHoverCell)
	{
		return;
	}
	CurrentHoverCell = CursorCell;

	// [#184] ?댁쟾 紐⑤뱶(癒몄떊/Foundation)??ISM ?몃쾭 ????붿궡???붿〈 ?쒓굅 ???щ떎由щ뒗 ISM 誘몄궗??怨좎뒪?몃쭔)?대씪
	// 吏곸젒 ??吏?곕㈃ 紐⑤뱶 ?꾪솚 ???ㅽ뀒????쇱씠 ?щ떎由?怨좎뒪??諛묒뿉 ?⑤뒗?? ClearHoverPreview媛 ISM+?붿궡???대━??
	// ??怨좎뒪?몃룄 ?④린誘濡? ?꾨옒 OJJ_ShowGhostForLadder媛 ?ㅼ떆 ?쒖떆(Foundation ?몃쾭??Clear?뭆how ?⑦꽩怨??숈씪).
	TargetGrid->ClearHoverPreview();

	// 怨좎뒪?몃뒗 ?붿쭊 Cube 諛뺤뒪(洹몃━???대??먯꽌 濡쒕뱶) ???ㅼ젣 ?щ떎由?ISM 硫붿떆? ?낅┰. ?꾩튂/?믪씠留??꾨떖.
	// 고스트는 실제 사다리(LadderClass CDO)의 타일 적층을 그대로 그린다(막대 Cube 대체). CDO는 베이스 메시·기본
	// 회전/스케일을 보유 — 스폰될 인스턴스와 동일 외형. LadderClass 미지정이면 null → 그리드가 안전 숨김.
	AOJJ_Ladder* LadderCDO = LadderClass ? LadderClass.GetDefaultObject() : nullptr;

	FVector BottomLoc;
	float ClimbHeight = 0.0f;
	FRotator Rot = FRotator::ZeroRotator;
	if (ComputeLadderPlacement(CursorCell, BottomLoc, ClimbHeight, Rot))
	{
		TargetGrid->OJJ_ShowGhostForLadder(LadderCDO, BottomLoc, ClimbHeight, Rot, /*bValid=*/true);
	}
	else
	{
		// ?좏슚 蹂 ?놁쓬(Foundation ???꾨떂/??? ?? ??鍮④컯 怨좎뒪??而ㅼ꽌 ? 吏硫댁뿉 湲곕낯 ?믪씠)濡?"?ш린 遺덇?" ?좏샇.
		const FVector CursorWorld = TargetGrid->GridToWorld(CursorCell);
		float GroundZ = 0.0f;
		if (!TargetGrid->OJJ_GetRawTerrainSurfaceZ(CursorCell, GroundZ))
		{
			GroundZ = CursorWorld.Z;
		}
		TargetGrid->OJJ_ShowGhostForLadder(
			LadderCDO, FVector(CursorWorld.X, CursorWorld.Y, GroundZ), /*ClimbHeight=*/100.0f, FRotator::ZeroRotator, /*bValid=*/false);
	}
}

void AOJJ_BuildController::PlaceLadderAtCursor()
{
	if (!TargetGrid || !LadderClass)
	{
        UE_LOG(LogTemp, Warning, TEXT("[BuildController] TargetGrid or LadderClass is missing."));
		return;
	}

	// 留덉슦?ㅺ? ?좏슚 ?쒕㈃ 諛뽰씠???몃쾭 媛깆떊????踰덈룄 ???먯쑝硫??대┃ 臾댁떆(癒몄떊/Foundation 寃쎈줈? ?숈씪).
	if (CurrentHoverCell.X == INT_MIN || CurrentHoverCell.Y == INT_MIN)
	{
		return;
	}

	// ?몃쾭? 媛숈? ?낅젰(CurrentHoverCell)?쇰줈 ?ъ궛異???"誘몃━蹂닿린 = ?ㅼ젣 諛곗튂" ?뺥빀(Foundation 寃쎈줈 怨꾩빟怨??숈씪).
	FVector BottomLoc;
	float ClimbHeight = 0.0f;
	FRotator Rot = FRotator::ZeroRotator;
	if (!ComputeLadderPlacement(CurrentHoverCell, BottomLoc, ClimbHeight, Rot))
	{
		UE_LOG(LogTemp, Log, TEXT("[BuildController] ?щ떎由?諛곗튂 嫄곕? ??而ㅼ꽌媛 Foundation 蹂???꾨떂"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// ?먯쑀 諛곗튂(洹몃━???λ? 誘몃벑濡? ??TryPlaceMachine 誘멸꼍?? SpawnActorDeferred濡?FinishSpawning ?꾩뿉
	// ClimbHeight瑜?二쇱엯??OnConstruction(ApplyDimensions)???щ컮瑜??믪씠濡?硫붿떆/?몃━嫄곕? ?ъ씠吏뺥븯寃??쒕떎.
	// ?좑툘 ?≫꽣 ?ㅼ??쇱? 1 ?좎?(?앹젏 ?곗떇???ㅼ???誘몃컲?? ???믪씠??ClimbHeight濡쒕쭔.
	const FTransform SpawnXf(Rot, BottomLoc);
	AOJJ_Ladder* NewLadder = World->SpawnActorDeferred<AOJJ_Ladder>(
		LadderClass, SpawnXf, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!NewLadder)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] ?щ떎由?SpawnActorDeferred ?ㅽ뙣"));
		return;
	}
	NewLadder->OJJ_SetClimbHeight(ClimbHeight);
	NewLadder->FinishSpawning(SpawnXf);
	// [#184 철거] 사다리 레이어 등록 — 지면 셀 → 사다리 맵 + 기댄 Foundation 링크 주입(transform에서 산출).
	// 개별 철거(GetLadderAtCell) + Foundation 철거 cascade(OwningFoundation 매칭)의 단일 등록점.
	TargetGrid->OJJ_RegisterLadder(NewLadder);

	// [#3 점진 건설] 사다리도 머신/Foundation처럼 설치 빌드업(아래→위 시안 홀로그램). 이 시점 ApplyDimensions 완료라
	// LadderMesh ISM에 타일이 채워진 상태 → 빌드업 컴포넌트가 ISM 분기로 적층을 프록시에 복제. 동일 머티리얼/Duration
	// 슬롯 재사용. HologramBuildUpMaterial 미지정이면 컴포넌트가 안전하게 무동작(배치 정상).
	StartBuildUpEffect(NewLadder, NewLadder->GetLadderMeshComponent());

	NotifyTutorialQuestEvent(this, TEXT("PlaceLadder"));
	UE_LOG(LogTemp, Log, TEXT("[BuildController] ?щ떎由?諛곗튂 ??height=%.1f loc=%s"), ClimbHeight, *BottomLoc.ToString());
}

void AOJJ_BuildController::SetPlacementMode(EOJJ_BuildPlacementMode NewMode)
{
	if (PlacementMode == NewMode)
	{
		return;
	}

	// 紐⑤뱶 ?꾪솚 ??吏꾪뻾 以??쒕옒洹몃뒗 痍⑥냼(?붿뿬 ?곹깭 諛⑹?).
	CancelConveyorDrag();
	CancelPowerLineDrag();
	PlacementMode = NewMode;
	const TCHAR* ModeName = TEXT("Unknown");
	switch (PlacementMode)
	{
	case EOJJ_BuildPlacementMode::Machine:   ModeName = TEXT("Machine");   break;
	case EOJJ_BuildPlacementMode::Conveyor:  ModeName = TEXT("Conveyor");  break;
	case EOJJ_BuildPlacementMode::PowerNode: ModeName = TEXT("PowerNode"); break;
	case EOJJ_BuildPlacementMode::PowerLine: ModeName = TEXT("Cable"); break;
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
	case EOJJ_BuildPlacementMode::MoldingMachine: ModeName = TEXT("MoldingMachine"); break;
	case EOJJ_BuildPlacementMode::Synthesizer: ModeName = TEXT("Synthesizer"); break;
	case EOJJ_BuildPlacementMode::TeleCommunicationTower: ModeName = TEXT("TeleCommunicationTower"); break;
	case EOJJ_BuildPlacementMode::Ladder:    ModeName = TEXT("Ladder");     break;
	case EOJJ_BuildPlacementMode::BaseCamp:  ModeName = TEXT("BaseCamp");   break;
	case EOJJ_BuildPlacementMode::SignalAmplifier: ModeName = TEXT("Signal_Amplifier"); break;
	case EOJJ_BuildPlacementMode::None:      ModeName = TEXT("None");       break;
	}
	UE_LOG(LogTemp, Log, TEXT("[BuildController] Placement mode changed to %s"), ModeName);

	// [그리드 색상 2단계] 든 머신/모드 바뀜 → 필드 색상 규칙 갱신(시그니처 동일하면 그리드가 스킵).
	UpdateGridColorForCurrentMode();

	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
	UpdateMouseHover();
}

bool AOJJ_BuildController::ResolveBuildTraceHit(APlayerController* PC, FHitResult& OutHit) const
{
	if (!PC)
	{
		return false;
	}

	// 탑다운/None: 기존 — 마우스 커서 아래 트레이스. (탑다운은 커서가 곧 조준점)
	if (GetBuildViewMode() != EBuildViewMode::TPS)
	{
		return PC->GetHitResultUnderCursorByChannel(
			UEngineTypes::ConvertToTraceType(ECC_Visibility),
			/*bTraceComplex=*/false,
			OutHit);
	}

	// === TPS: 화면 중앙(크로스헤어) deproject → 전방 LineTrace ===
	// ⭐ deproject(전방벡터 아님): 카메라 투영/FOV를 그대로 반영해 화면중앙 픽셀이 실제로 가리키는 월드 레이를
	//    얻는다. 카메라 위치+전방벡터 방식은 SpringArm 소켓 오프셋만큼 조준점이 어긋날 수 있어 deproject가 정확.
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	int32 ViewX = 0, ViewY = 0;
	PC->GetViewportSize(ViewX, ViewY);
	if (ViewX <= 0 || ViewY <= 0)
	{
		return false;
	}

	FVector RayOrigin = FVector::ZeroVector;
	FVector RayDir = FVector::ZeroVector;
	if (!PC->DeprojectScreenPositionToWorld(ViewX * 0.5f, ViewY * 0.5f, RayOrigin, RayDir))
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(OJJ_BuildTPSTrace), /*bTraceComplex=*/false);
	APawn* OwnerPawn = PC->GetPawn();
	if (OwnerPawn)
	{
		Params.AddIgnoredActor(OwnerPawn); // 전방 근거리에서 자기 캡슐을 맞혀 조준이 막히는 것 방지
	}

	FHitResult Hit;
	const FVector TraceEnd = RayOrigin + RayDir * BuildTPSTraceLength;
	if (!World->LineTraceSingleByChannel(Hit, RayOrigin, TraceEnd, ECC_Visibility, Params))
	{
		// 전방에 표면 없음 → 고스트 없음(탑다운 !bHit과 동일 처리)
		return false;
	}

	// === 10m 도달거리 클램프 (TPS 전용) ===
	const FVector PlayerLoc = OwnerPawn ? OwnerPawn->GetActorLocation() : RayOrigin;
	const FVector ToHit = Hit.Location - PlayerLoc;
	if (ToHit.SizeSquared() > FMath::Square(BuildTPSMaxReach))
	{
		// 너무 멀다 → 도달거리 링 방향(같은 조준 방향)의 지면으로 클램프: 클램프된 XY에서 수직 하향 트레이스.
		// ⚠️ 클램프 후에도 그리드 스냅은 호출부의 WorldToGrid가 동일하게 처리(좌표만 바뀜).
		constexpr float ProbeUp = 500.f;
		constexpr float ProbeDown = 5000.f;
		const FVector Clamped = PlayerLoc + ToHit.GetClampedToMaxSize(BuildTPSMaxReach);
		const FVector DownStart(Clamped.X, Clamped.Y, Clamped.Z + ProbeUp);
		const FVector DownEnd(Clamped.X, Clamped.Y, Clamped.Z - ProbeDown);
		FHitResult GroundHit;
		if (World->LineTraceSingleByChannel(GroundHit, DownStart, DownEnd, ECC_Visibility, Params))
		{
			OutHit = GroundHit;
			return true;
		}
		// 클램프 지점 지면도 못 찾으면 설치 불가(고스트 없음).
		return false;
	}

	OutHit = Hit;
	return true;
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
	if (!ResolveBuildTraceHit(PC, Hit)) // 모드별 소스(탑다운=커서 / TPS=화면중앙 전방레이+클램프)
	{
		return false;
	}

	OutCell = ResolveCursorCellOverWater(Hit.Location); // #182 臾????⑤윺?숈뒪 蹂댁젙(?몃쾭=?대┃ ?숈씪 ?)
	return true;
}

FIntPoint AOJJ_BuildController::ResolveCursorCellOverWater(const FVector& TerrainHitLocation) const
{
	const FIntPoint TerrainCell = TargetGrid->WorldToGrid(TerrainHitLocation);

	// #182 ?⑤윺?숈뒪: WaterArea媛 Visibility Ignore??而ㅼ꽌 ?덉씠媛 臾쇱쓣 ?듦낵??臾?諛?吏?뺤쓣 留욌뒗?? 源딆? 臾쇱뿉??
	// 洹?吏???덊듃媛 蹂댁씠???섎㈃ ??먯꽌 ??닔 移멸퉴吏 鍮쀫굹媛(臾?諛??≪?濡? ?몃쾭/?대┃ ????닿툔?쒕떎. 留덉슦???덉씠瑜?
	// 吏곸젒 媛?WaterArea ?섎㈃ ?됰㈃怨?援먯감?쒖폒, 吏???덊듃蹂대떎 媛源뚯슫(=蹂댁씠?? ?섎㈃ ????덉쑝硫?洹?????대떎.
	// 臾??꾧? ?꾨땲硫??≪?/癒몄떊) ?섎㈃ 援먯감媛 ?놁뼱 湲곗〈 吏???덊듃 XY 洹몃?濡????뚭? 0.
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
	// F4-1: 而⑤쿋?댁뼱/?뚯씠?꾧? ?쒕옒洹??곹깭癒몄떊(bIsDraggingConveyor/ConveyorDragCells)??怨듭슜 ??
	// 紐⑤뱶???숈떆???섎굹???곹깭 異⑸룎 ?놁쓬(SetPlacementMode媛 ?꾪솚 ???쒕옒洹?痍⑥냼). ?꾨━酉곕쭔 遺꾧린.
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
	// [성능] 조준 셀이 직전과 같으면 재계산 스킵 — A*가 매 틱 도는 것 방지(셀 바뀔 때만 라우팅).
	// 탑다운 드래그(AppendConveyorPathTo)도 같은 셀이면 어차피 no-op이라 동치.
	if (CursorCell == CurrentHoverCell)
	{
		return;
	}

	// [TPS 2클릭] 컨베이어·파이프는 앵커↔조준 자동 L경로로 매 호버 재구성(드래그 스윕 아님 — 마우스=카메라).
	// 탑다운은 기존 증분 누적(드래그 스윕) 유지.
	if (IsTwoClickConnectMode()
		&& (PlacementMode == EOJJ_BuildPlacementMode::Conveyor
			|| PlacementMode == EOJJ_BuildPlacementMode::Pipe))
	{
		OJJ_BuildConveyorAutoRoute(CursorCell);
	}
	else
	{
		AppendConveyorPathTo(CursorCell);
	}
	UpdatePathDragHoverPreview(ConveyorDragCells);
	CurrentHoverCell = CursorCell;
}

void AOJJ_BuildController::OJJ_BuildConveyorAutoRoute(FIntPoint TargetCell)
{
	// 앵커(드래그 시작셀) 보존 + 그 뒤를 L경로로 재생성. 앵커 없으면 no-op(방어).
	if (ConveyorDragCells.Num() == 0)
	{
		return;
	}
	const FIntPoint Anchor = ConveyorDragCells[0];

	// A* 우회 경로 우선(장애물=머신 점유·비건설 셀 돌아가기). 파이프는 물 위 허용(bAllowWater).
	// 점유맵·분류는 OJJ_Grid 소유라 그리드 라우터에 위임. 시그니처/호출부는 그대로 — 내부만 A*.
	const bool bAllowWater = (PlacementMode == EOJJ_BuildPlacementMode::Pipe);
	TArray<FIntPoint> Route;
	if (TargetGrid && TargetGrid->OJJ_FindConveyorRoute(Anchor, TargetCell, bAllowWater, Route) && Route.Num() > 0)
	{
		ConveyorDragCells = MoveTemp(Route);
		return;
	}

	// 우회로 없음(완전 포위/탐색범위 밖) → L자 폴백(직선 시도). 최종 검증/프리뷰가 빨강 처리.
	// L자 = 가로 먼저(x1→x2) then 세로(y1→y2). 4방향 인접 연속 셀.
	ConveyorDragCells.Reset();
	ConveyorDragCells.Add(Anchor);
	FIntPoint Cur = Anchor;
	const int32 StepX = (TargetCell.X >= Cur.X) ? 1 : -1;
	while (Cur.X != TargetCell.X)
	{
		Cur.X += StepX;
		ConveyorDragCells.Add(Cur);
	}
	const int32 StepY = (TargetCell.Y >= Cur.Y) ? 1 : -1;
	while (Cur.Y != TargetCell.Y)
	{
		Cur.Y += StepY;
		ConveyorDragCells.Add(Cur);
	}
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

	// F4-1: ?뚯씠??紐⑤뱶???대옒???뺢퇋??諛곗튂留?遺꾧린 ???쒕옒洹??섏쭛쨌?뺣━ ?먮쫫? 怨듭슜.
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

	// F4-1: spawn-validate-destroy ?⑦꽩 怨듭슜 ???뚯씠?꾨뒗 ?대옒??TryPlace留??ㅻ쫫.
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
		else
		{
			// [#3 확장] 신규 배치 파이프 — 시작→끝 길이 방향 홀로그램 빌드업.
			StartPathBuildUpEffect(Pipe, PlacementCells);
		}
		// ?뚯씠?꾨뒗 ?섏뒪??諛곗튂 ?源?誘몃벑濡?NotifyMainQuestMachinePlaced 鍮꾪샇異???而⑤쿋?댁뼱 ?꾩슜 ??.
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

	// [#3 확장] 신규 배치 컨베이어 — 시작→끝 길이 방향 홀로그램 빌드업.
	StartPathBuildUpEffect(Conveyor, PlacementCells);

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
	// [#5] 미리보기 소등 — 취소/커밋(Commit이 본 함수 호출)/모드전환(SetPlacementMode가 본 함수 호출) 공통 경로.
	HidePowerLinePreview();
}

void AOJJ_BuildController::EnsurePowerLinePreviewMID()
{
	if (!PowerLinePreviewMaterial)
	{
		// 머티리얼이 (에디터에서) 해제된 경우: 낡은 MID와 세그먼트 오버라이드를 명시적으로 정리한다.
		// (SetMaterial 오버라이드는 해제 전까지 유지되므로 stale MID 색이 남는 것 방지)
		if (PowerLinePreviewMID)
		{
			PowerLinePreviewMID = nullptr;
			for (UStaticMeshComponent* Segment : PowerLinePreviewSegments)
			{
				if (Segment)
				{
					Segment->SetMaterial(0, nullptr);
				}
			}
		}
		return;
	}
	// 베이스 머티리얼이 에디터에서 교체됐으면 재생성(옛 parent MID 잔류 방지).
	if (PowerLinePreviewMID && PowerLinePreviewMID->Parent != PowerLinePreviewMaterial)
	{
		PowerLinePreviewMID = nullptr;
	}
	if (!PowerLinePreviewMID)
	{
		PowerLinePreviewMID = UMaterialInstanceDynamic::Create(PowerLinePreviewMaterial, this);
		if (PowerLinePreviewMID)
		{
			PowerLinePreviewMID->SetFlags(RF_Transient); // 레벨 dirty 방지(고스트 MID 패턴 미러).
		}
	}
}

void AOJJ_BuildController::HidePowerLinePreview()
{
	for (UStaticMeshComponent* Segment : PowerLinePreviewSegments)
	{
		if (Segment)
		{
			Segment->SetVisibility(false);
		}
	}
}

void AOJJ_BuildController::EnsurePowerRangeSphere()
{
	if (PowerRangeSphere)
	{
		return;
	}

	PowerRangeSphere = NewObject<UStaticMeshComponent>(this);
	if (!PowerRangeSphere)
	{
		return;
	}
	PowerRangeSphere->SetMobility(EComponentMobility::Movable);
	if (PowerRangeSphereMesh)
	{
		PowerRangeSphere->SetStaticMesh(PowerRangeSphereMesh);
	}
	PowerRangeSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PowerRangeSphere->SetCastShadow(false);
	PowerRangeSphere->RegisterComponent();
	PowerRangeSphere->SetVisibility(false);

	// 전기 플라즈마 동적 머티리얼(베이스 미존재 시 메시 기본 머티리얼로 표시 — 가드).
	if (PowerRangeMaterial)
	{
		PowerRangeMID = UMaterialInstanceDynamic::Create(PowerRangeMaterial, this);
		if (PowerRangeMID)
		{
			PowerRangeMID->SetFlags(RF_Transient);
			PowerRangeSphere->SetMaterial(0, PowerRangeMID);
		}
	}
}

void AOJJ_BuildController::UpdatePowerRangePreview(const FVector& Center)
{
	EnsurePowerRangeSphere();
	if (!PowerRangeSphere)
	{
		return;
	}

	// 송전탑(PowerGridNode) CDO의 공급 반경 — 구 기본 반경 50uu이므로 스케일=반경/50(차폐장 돔과 동일 공식).
	const APowerGridNode* NodeCDO = Cast<APowerGridNode>(PowerGridNodeClass ? PowerGridNodeClass->GetDefaultObject() : nullptr);
	const float Radius = NodeCDO ? NodeCDO->GetSupplyRadius() : 700.0f;
	const float Scale = (Radius > 0.0f ? Radius : 700.0f) / 50.0f;

	PowerRangeSphere->SetWorldLocation(Center);
	PowerRangeSphere->SetWorldScale3D(FVector(Scale));
	PowerRangeSphere->SetVisibility(true);

	if (PowerRangeMID)
	{
		PowerRangeMID->SetVectorParameterValue(TEXT("PlasmaColor"), PowerRangePlasmaColor);
		PowerRangeMID->SetScalarParameterValue(TEXT("Opacity"), PowerRangeOpacity);
	}
}

void AOJJ_BuildController::HidePowerRangePreview()
{
	if (PowerRangeSphere && PowerRangeSphere->IsVisible())
	{
		PowerRangeSphere->SetVisibility(false);
	}
}

FVector AOJJ_BuildController::ComputePowerLineSagPoint(const FVector& Start, const FVector& End, float Alpha, float SagDepth)
{
	// APowerLine::GetSagPoint 공식 복제(포물선): 4α(1-α)는 중앙(α=0.5)에서 1, 양끝에서 0.
	const FVector Point = FMath::Lerp(Start, End, Alpha);
	const float SagAlpha = 4.0f * Alpha * (1.0f - Alpha);
	return Point - FVector(0.0f, 0.0f, SagDepth * SagAlpha);
}

void AOJJ_BuildController::UpdatePowerLinePreview(const FHitResult& Hit)
{
	// 드래그 중 + 시작 머신 유효일 때만 그린다. 아니면 전부 숨김.
	AMachineBase* StartMachine = bIsDraggingPowerLine ? PowerLineStartMachine.Get() : nullptr;
	if (!StartMachine || !PowerLinePreviewMesh)
	{
		HidePowerLinePreview();
		return;
	}

	// 끝점 위치 — APowerLine 룩 복제(EndpointHeightOffset 20 + Node -90 / Plant -110).
	constexpr float EndpointHeightOffset = 20.0f;
	constexpr float NodeLowerOffset = 90.0f;
	constexpr float PlantLowerOffset = 110.0f;

	FVector StartLoc = APowerLine::GetEndpointLocationForActor(StartMachine, EndpointHeightOffset);
	if (StartMachine->IsA<APowerGridNode>())
	{
		StartLoc.Z -= NodeLowerOffset;
	}
	else if (StartMachine->IsA<APowerPlant>())
	{
		StartLoc.Z -= PlantLowerOffset;
	}

	// 조준 끝점 — 유효 단자 위면 그 단자, 아니면 커서 위치.
	AMachineBase* HoverMachine = Cast<AMachineBase>(Hit.GetActor());
	if (!IsPowerLineEndpoint(HoverMachine))
	{
		HoverMachine = FindPowerLineEndpointNearLocation(Hit.Location);
	}

	FVector EndLoc;
	if (HoverMachine)
	{
		EndLoc = APowerLine::GetEndpointLocationForActor(HoverMachine, EndpointHeightOffset);
		if (HoverMachine->IsA<APowerGridNode>())
		{
			EndLoc.Z -= NodeLowerOffset;
		}
		else if (HoverMachine->IsA<APowerPlant>())
		{
			EndLoc.Z -= PlantLowerOffset;
		}
	}
	else
	{
		EndLoc = Hit.Location + FVector(0.0f, 0.0f, EndpointHeightOffset);
	}

	// 색 3단계.
	FLinearColor PreviewColor;
	if (HoverMachine)
	{
		// 타겟 있음 — Chan 판정(거리/타입/중복 포함) 읽기. 가능=초록, 불가(범위밖 포함)=빨강.
		UGameInstance* GameInstance = GetGameInstance();
		UFactoryManagerSubsystem* FactoryManager = GameInstance ? GameInstance->GetSubsystem<UFactoryManagerSubsystem>() : nullptr;
		const bool bCanConnect = FactoryManager && FactoryManager->CanConnectPowerLineEndpoints(StartMachine, HoverMachine);
		PreviewColor = bCanConnect ? PowerLinePreviewColorValid : PowerLinePreviewColorInvalid;
	}
	else
	{
		// 타겟 없음 = 드래그 중(노랑). 단, 시작이 Node면 거리 > ConnectionRadius일 때 허공 범위밖(빨강).
		PreviewColor = PowerLinePreviewColorDragging;
		if (const APowerGridNode* StartNode = Cast<APowerGridNode>(StartMachine))
		{
			const float Radius = StartNode->GetConnectionRadius();
			if (Radius > 0.0f && FVector::Dist(StartLoc, EndLoc) > Radius)
			{
				PreviewColor = PowerLinePreviewColorInvalid;
			}
		}
	}

	const float Length = FVector::Dist(StartLoc, EndLoc);
	if (Length <= UE_KINDA_SMALL_NUMBER)
	{
		HidePowerLinePreview();
		return;
	}

	// sag 깊이 + 세그먼트 수(APowerLine 공식 복제 — Chan 무접촉).
	constexpr float SagRatio = 0.035f;
	constexpr float MaxSagDepth = 120.0f;
	constexpr float SegmentTargetLength = 150.0f;
	constexpr int32 MinSagSegments = 6;
	constexpr int32 MaxSagSegments = 24;
	const float HorizontalDistance = FVector::Dist2D(StartLoc, EndLoc);
	const float SagDepth = FMath::Clamp(HorizontalDistance * SagRatio, 0.0f, MaxSagDepth);
	const int32 SegmentCount = FMath::Clamp(FMath::CeilToInt(Length / SegmentTargetLength), MinSagSegments, MaxSagSegments);

	EnsurePowerLinePreviewMID();
	if (PowerLinePreviewMID)
	{
		PowerLinePreviewMID->SetVectorParameterValue(TEXT("TintColor"), PreviewColor);
		PowerLinePreviewMID->SetScalarParameterValue(TEXT("Opacity"), PowerLinePreviewOpacity);
	}

	// 실린더 메시 치수(스케일 환산용). 엔진 Cylinder는 피벗 중앙이라 SetWorldLocation=세그먼트 중점으로 정렬.
	const FBoxSphereBounds MeshBounds = PowerLinePreviewMesh->GetBounds();
	const FVector MeshSize = MeshBounds.BoxExtent * 2.0f;
	const float MeshDiameterX = FMath::Max(MeshSize.X, UE_KINDA_SMALL_NUMBER);
	const float MeshDiameterY = FMath::Max(MeshSize.Y, UE_KINDA_SMALL_NUMBER);
	const float MeshLength = FMath::Max(MeshSize.Z, UE_KINDA_SMALL_NUMBER);

	for (int32 i = 0; i < SegmentCount; ++i)
	{
		const float StartAlpha = static_cast<float>(i) / static_cast<float>(SegmentCount);
		const float EndAlpha = static_cast<float>(i + 1) / static_cast<float>(SegmentCount);
		const FVector SegStart = ComputePowerLineSagPoint(StartLoc, EndLoc, StartAlpha, SagDepth);
		const FVector SegEnd = ComputePowerLineSagPoint(StartLoc, EndLoc, EndAlpha, SagDepth);

		// 세그먼트 풀 재사용(없으면 1회 생성). 매 프레임 spawn/destroy 금지.
		UStaticMeshComponent* Segment = PowerLinePreviewSegments.IsValidIndex(i) ? PowerLinePreviewSegments[i] : nullptr;
		if (!Segment)
		{
			Segment = NewObject<UStaticMeshComponent>(this);
			Segment->SetMobility(EComponentMobility::Movable);
			Segment->SetStaticMesh(PowerLinePreviewMesh);
			Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Segment->SetCastShadow(false);
			Segment->RegisterComponent();
			PowerLinePreviewSegments.Add(Segment);
		}
		if (PowerLinePreviewMID && Segment->GetMaterial(0) != PowerLinePreviewMID)
		{
			Segment->SetMaterial(0, PowerLinePreviewMID);
		}

		const FVector Delta = SegEnd - SegStart;
		const float SegLength = Delta.Size();
		if (SegLength <= UE_KINDA_SMALL_NUMBER)
		{
			Segment->SetVisibility(false);
			continue;
		}
		const FVector Direction = Delta / SegLength;
		const FQuat Rotation = FQuat::FindBetweenNormals(FVector::UpVector, Direction);

		Segment->SetVisibility(true);
		Segment->SetWorldLocationAndRotation((SegStart + SegEnd) * 0.5f, Rotation);
		Segment->SetWorldScale3D(FVector(
			PowerLinePreviewThickness / MeshDiameterX,
			PowerLinePreviewThickness / MeshDiameterY,
			SegLength / MeshLength));
	}

	// 남는 세그먼트 숨김(길이가 짧아져 SegmentCount가 줄었을 때).
	for (int32 i = SegmentCount; i < PowerLinePreviewSegments.Num(); ++i)
	{
		if (PowerLinePreviewSegments[i])
		{
			PowerLinePreviewSegments[i]->SetVisibility(false);
		}
	}
}

bool AOJJ_BuildController::CancelPendingConnectAnchor()
{
	// [경로형 2클릭 공통 골격] 진행 중인 연결 앵커를 무른다(전선 먼저, 컨/파이프 확장 예정).
	// 무른 게 있으면 호버 미리보기도 정리하고 true 반환(호출부=Z가 모드 None 전환 대신 모드 유지).
	bool bCancelled = false;

	if (bIsDraggingPowerLine)
	{
		CancelPowerLineDrag();
		bCancelled = true;
	}

	// 컨베이어 2클릭 앵커(또는 진행 중 드래그)도 무름. CancelConveyorDrag가 ConveyorDragCells/프리뷰 정리.
	// (파이프 드래그도 같은 상태머신이라 함께 취소됨 — 앵커 무르고 모드 유지가 드래그 중 Z보다 자연스러움.)
	if (bIsDraggingConveyor)
	{
		CancelConveyorDrag();
		bCancelled = true;
	}

	if (bCancelled && TargetGrid)
	{
		TargetGrid->ClearHoverPreview();
	}
	return bCancelled;
}

void AOJJ_BuildController::AdjustBuildHeight(int32 Delta)
{
	// Foundation(Flat) 전용 — TPS(Q/E) + 탑다운(↑↓) 둘 다 동작(같은 BuildHeightOffset 공유 → 전환 시 높이 유지).
	// ⚠️ 램프=다리 미지원(UpdateLegVisual flat-only), 머신/기타 모드=대상 아님. None(빌드 아님)이면 무동작.
	// 입력: IA_BuildHeight를 IMC_BuildTPS(Q/E)·IMC_Build(↑↓) 양쪽에 매핑(에디터). 탑다운 Q/E는 IA_BuildRotate(카메라)라 ↑↓ 사용.
	if (!IsInBuildMode()
		|| PlacementMode != EOJJ_BuildPlacementMode::Foundation
		|| bRampFoundationSelected)
	{
		return;
	}

	constexpr int32 MaxBuildHeightOffset = 20; // 공중 Foundation 최대 층수(필요 시 여기만 조정)
	const int32 NewOffset = FMath::Clamp(BuildHeightOffset + Delta, 0, MaxBuildHeightOffset); // 0~20층(지면 아래 금지 / 상한)
	if (NewOffset == BuildHeightOffset)
	{
		return; // 0에서 더 내리기 등 — no-op
	}
	BuildHeightOffset = NewOffset;

	UE_LOG(LogTemp, Log, TEXT("[BuildController] BuildHeightOffset = %d층 (%.0fuu)"),
		BuildHeightOffset, BuildHeightOffset * AOJJ_Grid::OJJ_FoundationSnapStep);

	// 다음 Tick UpdateMouseHover가 같은 셀이면 스킵하므로, 높이만 바뀐 경우 고스트가 갱신되도록 sentinel로 강제 재렌더.
	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
}

float AOJJ_BuildController::GetFoundationHeightLiftZ(const AOJJ_Foundation* FoundationCDO, FIntPoint Origin, FIntPoint EffSize) const
{
	// [공중 Foundation] 빌드모드(TPS 또는 TopDown) && Flat && offset>0 일 때만 후보. None/램프 차단.
	// TPS↔TopDown은 BuildHeightOffset 공유라 전환해도 높이 유지 — 양 모드 동일하게 적용(고스트/스폰 일치).
	if (!IsInBuildMode() || bRampFoundationSelected
		|| BuildHeightOffset <= 0 || !TargetGrid || !FoundationCDO)
	{
		return 0.0f;
	}

	// ⚠️(b) 이웃 Foundation 접촉(상속)이면 coplanar 확장 — offset 무시. 씨앗(고립) 배치에만 높이 적용.
	// 스폰의 OJJ_ComputeSnapLift ① 상속과 동일 판정 소스(OJJ_GetNeighborFoundationSurfaceZ)라 프리뷰=배치 일치.
	const float SnapGridOriginZ = TargetGrid->GetActorLocation().Z + FoundationCDO->GetThickness();
	float NeighborZ = 0.0f;
	int32 ContactCells = 0;
	if (TargetGrid->OJJ_GetNeighborFoundationSurfaceZ(Origin, EffSize, SnapGridOriginZ, NeighborZ, ContactCells))
	{
		return 0.0f; // 이웃 접촉 → 평면 확장(높이 무시)
	}
	return BuildHeightOffset * AOJJ_Grid::OJJ_FoundationSnapStep;
}

AMachineBase* AOJJ_BuildController::GetPowerLineEndpointUnderCursor() const
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return nullptr;
	}

	FHitResult Hit;
	if (!ResolveBuildTraceHit(PC, Hit)) // 모드별 소스(탑다운=커서 / TPS=화면중앙 전방레이+클램프)
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
	NotifyTutorialQuestEvent(this, TEXT("CableConnected"));
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
