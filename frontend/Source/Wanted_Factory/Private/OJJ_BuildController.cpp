// Fill out your copyright notice in the Description page of Project Settings.


#include "OJJ_BuildController.h"

#include "Components/StaticMeshComponent.h"
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
#include "Machines/MoldingMachine.h"
#include "Machines/Pump.h"
#include "Machines/LiquidTank.h"
#include "Machines/Smelter.h"
#include "Machines/Synthesizer.h"
#include "Machines/TeleCommunicationTower.h"
#include "Machines/WarehousePort.h"
#include "Machines/EscapePod.h"
#include "OJJ_Foundation.h"
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
	// [#184] ?щ떎由?湲곕낯 ?대옒??BP 誘몄????? ??C++ AOJJ_Ladder. BP ?놁씠??C??諛곗튂 ?숈옉.
	LadderClass = AOJJ_Ladder::StaticClass();
}

void AOJJ_BuildController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Enter/Exit?먯꽌 Tick??on/off?섏?留? 諛⑹뼱?곸쑝濡?紐⑤뱶 媛?쒕룄 ?좎?(UpdateMouseHover ?대??먮룄 媛???덉쓬).
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
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] TargetGrid 誘몄꽕????EnterBuildMode 以묐떒"));
		return;
	}

	// 紐⑤뱶蹂??대옒??誘몄꽕??媛????癒몄떊 紐⑤뱶??MachineClass, 而⑤쿋?댁뼱 紐⑤뱶??ConveyorClass ?꾩슂.
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
	UpdateGridColorForCurrentMode();

	TargetGrid->SetVisualizationVisible(true);

	// 吏꾩엯 利됱떆 諛곗튂 癒몄떊 ?ы듃 ?붿궡???쒖떆(泥??몃쾭 ?꾩씠?쇰룄 蹂댁씠?꾨줉). ?몃쾭 ?붿궡?쒕뒗 泥?UpdateMouseHover?먯꽌.
	TargetGrid->RefreshPlacedMachineArrows();

	bIsBuildMode = true;

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
	if (!bIsBuildMode)
	{
		return;
	}

	if (TargetGrid)
	{
		TargetGrid->SetVisualizationVisible(false);
		TargetGrid->ClearHoverPreview();         // ?몃쾭 ? + ?몃쾭 ?붿궡???쒓굅
		TargetGrid->OJJ_UpdateCharacterCellOverlay(TArray<FIntPoint>());  // 罹먮┃??? ?쒖떆 ?쒓굅(F2-4 ?꾩냽 ??
	}
	CharacterOverlayCells.Reset();

	bIsBuildMode = false;

	// 而⑤쿋?댁뼱 ?쒕옒洹??곹깭 ?뺣━.
	bIsDraggingConveyor = false;
	ConveyorDragCells.Reset();
	bIsDraggingPowerLine = false;
	PowerLineStartMachine.Reset();

	// ?몃쾭 Tick ?뺤? (鍮뚮뱶紐⑤뱶 諛?0鍮꾩슜)
	SetActorTickEnabled(false);

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
	// R? IMC_Build ?꾩슜?대씪 鍮뚮뱶紐⑤뱶?먯꽌留?諛쒕룞?섏?留? 諛⑹뼱?곸쑝濡?媛??
	// ?뚯쟾? 癒몄떊 + Foundation(F3-0 ?????⑦봽 諛⑺뼢???鍮? ?몃쾭 ?꾩슜 ??而⑤쿋?댁뼱 紐⑤뱶?먯꽌??臾댁떆(Dummy parity).
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
			&& PlacementMode != EOJJ_BuildPlacementMode::MoldingMachine
			&& PlacementMode != EOJJ_BuildPlacementMode::Synthesizer
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
	case EOJJ_BuildPlacementMode::None:
		// 별도 액터/중립 — constructible(buildable OR Foundation) 위에 동작 → raw 평지·Foundation 초록.
		bRaw = true;
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
	if (!bIsBuildMode)
	{
		return;
	}

	if (!TargetGrid)
	{
		return;
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
	const bool bHit = PC->GetHitResultUnderCursorByChannel(
		UEngineTypes::ConvertToTraceType(ECC_Visibility),
		/*bTraceComplex=*/ false,
		Hit);

	if (!bHit)
	{
		// ?몃젅?댁뒪 ?ㅽ뙣 ??stale 誘몃━蹂닿린/罹먯떆媛 ?ㅼ쓬 ?대┃???섎せ ?곸슜?섏? ?딅룄濡?紐낆떆??由ъ뀑
		TargetGrid->ClearHoverPreview();
		CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
		return;
	}

	const FIntPoint CursorCell = ResolveCursorCellOverWater(Hit.Location); // #182 臾????⑤윺?숈뒪 蹂댁젙(?몃쾭=?대┃ ?숈씪 ?)

	// Conveyor/Pipe 紐⑤뱶: ?쒕옒洹??⑥씪 ? 誘몃━蹂닿린濡?遺꾧린 (癒몄떊 寃쎈줈? ?낅┰ ???뚯씠?꾨뒗 ?꾨━酉곕쭔 遺꾧린).
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
		// ?꾩꽑 ?쒕옒洹?誘몃━蹂닿린 ???쎄린 ?꾩슜 ?쒓컖??????곌껐 濡쒖쭅 鍮꾩묠踰? ?몃쾭??BuildController ?곸뿭).
		// Shipping ??ENABLE_DRAW_DEBUG=0 鍮뚮뱶?먯꽑 釉붾줉 ?꾩껜媛 而댄뙆???꾩썐 ???고???鍮꾩슜 0.
		// ?ш린 ?꾨떖 ?쒖젏????(!bHit) 媛?쒕? ?대? ?듦낵???곹깭?대?濡?Hit / Hit.Location ?좏슚.
		if (bIsDraggingPowerLine)
		{
			if (UWorld* World = GetWorld())
			{
				if (AMachineBase* StartMachine = PowerLineStartMachine.Get())
				{
					// ?꾩꽦??APowerLine::LineHeightOffset 湲곕낯媛?350)怨??믪씠瑜?留욎땄. LineHeightOffset??
					// protected쨌寃뚰꽣 ?놁쓬 ???곸닔 ?ъ슜. ??먯씠 洹?湲곕낯媛믪쓣 諛붽씀硫??ш린???숆린???꾩슂.
					constexpr float PreviewEndpointHeightOffset = 20.0f;
					const FVector StartLoc = APowerLine::GetEndpointLocationForActor(StartMachine, PreviewEndpointHeightOffset);

					// ?쒖옉 ?몃뱶(StartLoc) ??而ㅼ꽌(CursorLoc)濡?誘몃━蹂닿린 ??留??꾨젅??鍮꾩쁺??.
					// 而ㅼ꽌 ?꾨옒 ?몃뱶媛 ?곌껐 媛?ν븯硫?珥덈줉, ?꾨땲硫?鍮④컯. HoverNode媛 non-null???뚮쭔
					// CanConnect ?됯?(?⑤씫 ?됯?) ???몃뱶 ?꾧? ?꾨땲硫?留??꾨젅??洹몃옒???쒗쉶 鍮꾩슜 ?놁쓬.
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
		// [?듭뀡쨌誘멸뎄?? 鍮꾨뱶?섍렇 ?곹깭?먯꽌 而ㅼ꽌 ?꾨옒 ?몃뱶瑜??ㅽ뵾?대줈 媛뺤“?섎㈃ "?좏깮 媛?? ?뚰듃媛 ?섏?留?
		// ?붿껌 踰붿쐞(?쒕옒洹?以??쇰뱶諛?瑜??섏뼱 ?앸왂. ?꾩슂 ????if 諛붽묑??HoverNode 媛뺤“瑜?異붽?.
#endif
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

	if (!bIsBuildMode)
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
	NotifyMainQuestMachinePlaced(this, GetQuestPlacementTargetId(PlacementMode));

	UE_LOG(LogTemp, Log, TEXT("[BuildController] origin %s 癒몄떊 諛곗튂 ?깃났"),
		*Origin.ToString());

	// 吏곸쟾 origin???댁젣 ?먯쑀?????ㅼ쓬 UpdateMouseHover?먯꽌 鍮④컯?쇰줈 媛뺤젣 ?ы몴??
	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
}

// === Foundation 紐⑤뱶 (F1-b ??癒몄떊 寃쎈줈? ?낅┰, 而ㅻ쾭由ъ? 諛곗튂) ===

TSubclassOf<AOJJ_Foundation> AOJJ_BuildController::GetActiveFoundationClass() const
{
	// F3-2.5: 醫낅쪟 ?곹깭???곕씪 ?됲뙋/?⑦봽 ?좏깮. ?⑦봽 ?좏깮 ?곹깭?몃뜲 ?대옒?ㅺ? 鍮꾩뼱 ?덈뒗 寃쎌슦??
	// OJJ_SelectFoundationKind 寃뚯씠?멸? 李⑤떒?섎?濡??뺤긽 ?먮쫫?먯꽑 ?꾨떖 遺덇? ??洹몃옒??null?대㈃
	// ?ъ슜泥??몃쾭/諛곗튂)??湲곗〈 null 媛?쒓? ?숈옉?쒕떎.
	return bRampFoundationSelected ? RampFoundationClass : FlatFoundationClass;
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
	TargetGrid->OJJ_UpdateFoundationHoverPreview(Fit.Origin, Fit.EffSize, !Fit.bValid);

	// 怨좎뒪???꾨━酉?#187): ?됲뙋 ?꾩슜. ???먯젙?먯? ?몃쾭 ??쇨낵 ?숈씪(Fit.bValid AND CanPlaceFoundation)濡??쇱튂.
	// ?⑦봽???꾩냽 踰붿쐞??怨좎뒪??誘명몴????OJJ_HideGhost濡??꾪솚 ?붿〈 諛⑹?(ClearHoverPreview???④린吏留?紐낆떆??.
	if (!bRampFoundationSelected)
	{
		FString GhostReason;
		const bool bGhostValid = Fit.bValid && TargetGrid->CanPlaceFoundation(Fit.Origin, Fit.EffSize, GhostReason);
		TargetGrid->OJJ_ShowGhostForFoundation(
			ActiveClass.GetDefaultObject(), Fit.Origin, Fit.EffSize, bGhostValid);
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
	const FVector SnappedLocation = PlaceLocation + FVector(0.0f, 0.0f, SnapLift);
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
	FVector BottomLoc;
	float ClimbHeight = 0.0f;
	FRotator Rot = FRotator::ZeroRotator;
	if (ComputeLadderPlacement(CursorCell, BottomLoc, ClimbHeight, Rot))
	{
		TargetGrid->OJJ_ShowGhostForLadder(BottomLoc, ClimbHeight, Rot, /*bValid=*/true);
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
			FVector(CursorWorld.X, CursorWorld.Y, GroundZ), /*ClimbHeight=*/100.0f, FRotator::ZeroRotator, /*bValid=*/false);
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
	case EOJJ_BuildPlacementMode::MoldingMachine: ModeName = TEXT("MoldingMachine"); break;
	case EOJJ_BuildPlacementMode::Synthesizer: ModeName = TEXT("Synthesizer"); break;
	case EOJJ_BuildPlacementMode::TeleCommunicationTower: ModeName = TEXT("TeleCommunicationTower"); break;
	case EOJJ_BuildPlacementMode::Ladder:    ModeName = TEXT("Ladder");     break;
	case EOJJ_BuildPlacementMode::None:      ModeName = TEXT("None");       break;
	}
	UE_LOG(LogTemp, Log, TEXT("[BuildController] Placement mode changed to %s"), ModeName);

	// [그리드 색상 2단계] 든 머신/모드 바뀜 → 필드 색상 규칙 갱신(시그니처 동일하면 그리드가 스킵).
	UpdateGridColorForCurrentMode();

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
