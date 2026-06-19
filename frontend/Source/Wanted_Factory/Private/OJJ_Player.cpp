// Fill out your copyright notice in the Description page of Project Settings.


#include "OJJ_Player.h"

#include "Camera/CameraComponent.h"
#include "FactoryAgentClientSubsystem.h"
#include "FactorySaveSubsystem.h"
#include "QuestManagerSubsystem.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "OJJ_BuildController.h"
#include "OJJ_BuildCamera.h"
#include "OJJ_Ladder.h"
#include "Components/CapsuleComponent.h"
#include "OJJ_Grid.h"
#include "Blueprint/UserWidget.h"
#include "MachineBase.h"
#include "UI/UI_MachineInteract.h"
#include "UI/UI_MainHUD.h"
#include "UI/UI_Inventory.h"
#include "UI/UI_WarehouseInteract.h"
#include "UI/UI_QuestWindow.h"
#include "Machines/MachineSubsystem.h"
#include "Machines/LiquidTank.h"
#include "Machines/WarehousePort.h"
#include "PlayerWarehouseSubsystem.h"

AOJJ_Player::AOJJ_Player()
{
	// step-off 안착 보간(#184)을 위해 Tick 사용. 평상시 Tick 본문은 bSteppingOff 가드로 즉시 반환.
	PrimaryActorTick.bCanEverTick = true;

	// 빌드 카메라 기본 클래스 = C++ 기본. BP 파생을 BP_OJJ_Player에서 지정하면 그걸로 spawn.
	BuildCameraClass = AOJJ_BuildCamera::StaticClass();

	// 카메라가 컨트롤러 회전을 따르고, 캐릭터 본체는 이동 방향으로 회전 (TPS 표준)
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 400.f;
	SpringArm->bUsePawnControlRotation = true; // 마우스 입력으로 카메라 회전
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 10.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false; // 회전은 SpringArm이 담당

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;          // 이동 방향으로 본체 회전
	Movement->RotationRate = FRotator(0.f, 540.f, 0.f);
	Movement->MaxWalkSpeed = WalkSpeed;                  // 기존 하드코딩 600 → 걷기 속도(단일 출처). BeginPlay에서 재확정.
	Movement->JumpZVelocity = 500.f;
	Movement->AirControl = 0.35f;
}

void AOJJ_Player::BeginPlay()
{
	Super::BeginPlay();

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UPlayerWarehouseSubsystem* WarehouseSubsystem = GameInstance->GetSubsystem<UPlayerWarehouseSubsystem>())
		{
			WarehouseSubsystem->GrantInitialItems(InitialWarehouseItems);
		}
	}

	// 걷기 속도를 권위 있게 적용(BP CharacterMovement의 MaxWalkSpeed 기본값을 덮음 — 단일 출처).
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = WalkSpeed;
	}

	// 로컬 플레이어 컨트롤러의 EnhancedInput 서브시스템에 매핑 컨텍스트 등록
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (IMC_Player)
			{
				Subsystem->AddMappingContext(IMC_Player, 0);
			}
		}

		// 카메라 상하 회전 제한 — 뒤집힘 방지 (bUsePawnControlRotation 카메라는 CameraManager가 pitch를 clamp)
		if (APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
		{
			CameraManager->ViewPitchMin = CameraPitchMin;
			CameraManager->ViewPitchMax = CameraPitchMax;
		}
	}

	// 레벨에 배치된 BuildController 인스턴스를 찾아 캐시 (spawn 하지 않음 —
	// 레벨 인스턴스에 와이어링된 TargetGrid/MachineClass 설정을 유지해야 하므로).
	BuildController = Cast<AOJJ_BuildController>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AOJJ_BuildController::StaticClass()));
	if (!BuildController)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[OJJ_Player] AOJJ_BuildController 인스턴스를 레벨에서 찾지 못함 — 빌드모드(B키) 비활성. ")
			TEXT("레벨에 AOJJ_BuildController가 배치되어 있고 TargetGrid/MachineClass가 설정됐는지 확인."));
	}

	// 빌드 탑다운 카메라는 spawn으로 생성 (수동 레벨 배치 불필요 — 진입 시 그리드 중심으로 자동 배치).
	if (UWorld* World = GetWorld())
	{
		const TSubclassOf<AOJJ_BuildCamera> SpawnClass =
			BuildCameraClass ? BuildCameraClass : TSubclassOf<AOJJ_BuildCamera>(AOJJ_BuildCamera::StaticClass());
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		BuildCamera = World->SpawnActor<AOJJ_BuildCamera>(SpawnClass, FTransform::Identity, SpawnParams);
	}
	if (!BuildCamera)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[OJJ_Player] AOJJ_BuildCamera spawn 실패 — 빌드모드 탑다운 전환 비활성."));
	}
	
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC && MainHUDWidgetClass)
	{
		MainHUDWidgetInstance = CreateWidget<UUserWidget>(PC, MainHUDWidgetClass);
		if (MainHUDWidgetInstance)
		{
			MainHUDWidgetInstance->AddToViewport();
		}
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
		{
			QuestManager->StartTutorialQuestTest();
		}

		if (UFactorySaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UFactorySaveSubsystem>())
		{
			SaveSubsystem->HandlePlayerReady(this);
		}
	}
	
	ConnectFactoryAgentClient();
}

void AOJJ_Player::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 폰 파괴/언포제스 시 열려 있던 머신 상호작용 위젯·입력모드 정리.
	// ⚠️ EndPlay 시점엔 PlayerController가 이미 무효일 수 있다 — Cast가 null을 반환하면
	//    CloseMachineInteractWidget의 PC null-가드가 입력모드 복원을 스킵하고 위젯 제거만 수행한다
	//    (컨트롤러가 없으면 복원할 대상도 없으므로 안전).
	CloseMachineInteractWidget(Cast<APlayerController>(GetController()));

	// 등반/step-off 중 폰 파괴·언포제스 시 비행/중력0 상태가 남지 않도록 청산(폰 재사용 안전).
	AbortClimb();

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFactorySaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UFactorySaveSubsystem>())
		{
			SaveSubsystem->SaveCurrentGame();
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AOJJ_Player::ConnectFactoryAgentClient()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFactoryAgentClientSubsystem* AgentClient = GameInstance->GetSubsystem<UFactoryAgentClientSubsystem>())
		{
			if (AgentClient->GetConnectionState() == EFactoryAgentConnectionState::Disconnected)
			{
				AgentClient->Connect("ws://127.0.0.1:18000/ws/agent");
			}
		}
	}
}

void AOJJ_Player::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[OJJ_Player] EnhancedInputComponent 캐스트 실패 — 프로젝트가 EnhancedInput을 사용하는지 확인"));
		return;
	}

	if (IA_Move)
	{
		EnhancedInput->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AOJJ_Player::Move);
	}
	if (IA_Sprint)
	{
		EnhancedInput->BindAction(IA_Sprint, ETriggerEvent::Started, this, &AOJJ_Player::StartSprint);
		EnhancedInput->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &AOJJ_Player::StopSprint);
		// Hold/Chord 트리거가 붙어 뗌이 Completed 대신 Canceled로 와도 질주가 안 남도록 함께 바인딩.
		EnhancedInput->BindAction(IA_Sprint, ETriggerEvent::Canceled, this, &AOJJ_Player::StopSprint);
	}
	if (IA_Look)
	{
		EnhancedInput->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AOJJ_Player::Look);
	}
	if (IA_Zoom)
	{
		EnhancedInput->BindAction(IA_Zoom, ETriggerEvent::Triggered, this, &AOJJ_Player::Zoom);
	}
	if (IA_Jump)
	{
		// ACharacter 내장 Jump/StopJumping 사용
		EnhancedInput->BindAction(IA_Jump, ETriggerEvent::Started, this, &AOJJ_Player::StartJumpAction);
		EnhancedInput->BindAction(IA_Jump, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}
	if (IA_Interact)
	{
		// 머신 상호작용 토글(F) — 누를 때 한 번(Started). 빌드모드 가드는 핸들러 내부에서.
		EnhancedInput->BindAction(IA_Interact, ETriggerEvent::Started, this, &AOJJ_Player::OnInteract);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[OJJ_Player] IA_Interact 미할당 — 머신 상호작용(F) 비활성. IMC_Player/BP_OJJ_Player에 IA_Interact 할당 필요."));
	}
	if (IA_Build)
	{
		EnhancedInput->BindAction(IA_Build, ETriggerEvent::Started, this, &AOJJ_Player::ToggleBuild);
	}
	else
	{
		// 바인딩이 조용히 스킵되면 B키가 무반응이라 원인 파악이 어려움 → 명시적 경고
		UE_LOG(LogTemp, Warning,
			TEXT("[OJJ_Player] IA_Build 미할당 — 빌드모드 토글(B키) 비활성. BP_OJJ_Player에 IA_Build 에셋 할당 필요."));
	}
	if (IA_BuildPlace)
	{
		// 누름(Started)=배치/드래그시작, 뗌(Completed)=드래그커밋, 취소(Canceled)=드래그취소.
		// 머신 모드에선 Completed/Canceled가 no-op(BuildController 내부 모드 가드).
		EnhancedInput->BindAction(IA_BuildPlace, ETriggerEvent::Started, this, &AOJJ_Player::BuildPlace);
		EnhancedInput->BindAction(IA_BuildPlace, ETriggerEvent::Completed, this, &AOJJ_Player::BuildPlaceReleased);
		EnhancedInput->BindAction(IA_BuildPlace, ETriggerEvent::Canceled, this, &AOJJ_Player::BuildPlaceCanceled);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[OJJ_Player] IA_BuildPlace 미할당 — 배치(좌클릭) 비활성. BP_OJJ_Player에 IA_BuildPlace 에셋 할당 필요."));
	}
	if (IA_SetMachineMode)
	{
		EnhancedInput->BindAction(IA_SetMachineMode, ETriggerEvent::Started, this, &AOJJ_Player::SetMachineMode);
	}
	if (IA_SetConveyorMode)
	{
		EnhancedInput->BindAction(IA_SetConveyorMode, ETriggerEvent::Started, this, &AOJJ_Player::SetConveyorMode);
	}
	if (IA_SetPipeMode)
	{
		EnhancedInput->BindAction(IA_SetPipeMode, ETriggerEvent::Started, this, &AOJJ_Player::SetPipeMode);
	}
	if (IA_SetTankMode)
	{
		EnhancedInput->BindAction(IA_SetTankMode, ETriggerEvent::Started, this, &AOJJ_Player::SetTankMode);
	}
	if (IA_SetFoundationMode)
	{
		EnhancedInput->BindAction(IA_SetFoundationMode, ETriggerEvent::Started, this, &AOJJ_Player::SetFoundationMode);
	}
	if (IA_SetRampMode)
	{
		EnhancedInput->BindAction(IA_SetRampMode, ETriggerEvent::Started, this, &AOJJ_Player::SetRampFoundationMode);
	}
	if (IA_SetPowerNodeMode)
	{
		EnhancedInput->BindAction(IA_SetPowerNodeMode, ETriggerEvent::Started, this, &AOJJ_Player::SetPowerNodeMode);
	}
	if (IA_SetShieldMode)
	{
		EnhancedInput->BindAction(IA_SetShieldMode, ETriggerEvent::Started, this, &AOJJ_Player::SetShieldMode);
	}
	if (IA_SetPowerLineMode)
	{
		EnhancedInput->BindAction(IA_SetPowerLineMode, ETriggerEvent::Started, this, &AOJJ_Player::SetPowerLineMode);
	}
	if (IA_SetPowerPlantMode)
	{
		EnhancedInput->BindAction(IA_SetPowerPlantMode, ETriggerEvent::Started, this, &AOJJ_Player::SetPowerPlantMode);
	}
	if (IA_SetGrinderMode)
	{
		EnhancedInput->BindAction(IA_SetGrinderMode, ETriggerEvent::Started, this, &AOJJ_Player::SetGrinderMode);
	}
	if (IA_SetMinerMode)
	{
		EnhancedInput->BindAction(IA_SetMinerMode, ETriggerEvent::Started, this, &AOJJ_Player::SetMinerMode);
	}
	if (IA_SetPumpMode)
	{
		EnhancedInput->BindAction(IA_SetPumpMode, ETriggerEvent::Started, this, &AOJJ_Player::SetPumpMode);
	}
	if (IA_SetSmelterMode)
	{
		EnhancedInput->BindAction(IA_SetSmelterMode, ETriggerEvent::Started, this, &AOJJ_Player::SetSmelterMode);
	}
	if (IA_SetWarehouseMode)
	{
		EnhancedInput->BindAction(IA_SetWarehouseMode, ETriggerEvent::Started, this, &AOJJ_Player::SetWarehouseMode);
	}
	if (IA_SetDemolishMode)
	{
		EnhancedInput->BindAction(IA_SetDemolishMode, ETriggerEvent::Started, this, &AOJJ_Player::SetDemolishMode);
	}
	if (IA_BuildPan)
	{
		// 매 프레임 입력(연속 이동) → Triggered
		EnhancedInput->BindAction(IA_BuildPan, ETriggerEvent::Triggered, this, &AOJJ_Player::BuildPan);
	}
	if (IA_BuildRotate)
	{
		EnhancedInput->BindAction(IA_BuildRotate, ETriggerEvent::Triggered, this, &AOJJ_Player::BuildRotate);
	}
	if (IA_MachineRotate)
	{
		// 머신 회전은 1회성 토글이라 Started (누를 때 한 번)
		EnhancedInput->BindAction(IA_MachineRotate, ETriggerEvent::Started, this, &AOJJ_Player::BuildRotateMachine);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[OJJ_Player] IA_MachineRotate 미할당 — 호버 머신 회전(R) 비활성. IMC_Build/BP_OJJ_Player에 IA_MachineRotate 할당 필요."));
	}

	PlayerInputComponent->BindKey(EKeys::M, IE_Pressed, this, &AOJJ_Player::SendOperatorGuideRequest);
	PlayerInputComponent->BindKey(EKeys::Slash, IE_Pressed, this, &AOJJ_Player::TriggerHUDQuestRequest);
	PlayerInputComponent->BindKey(EKeys::J, IE_Pressed, this, &AOJJ_Player::TriggerHUDQuestWindowToggle);
	PlayerInputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &AOJJ_Player::TriggerHUDAIGuideToggle);
	PlayerInputComponent->BindKey(EKeys::I, IE_Pressed, this, &AOJJ_Player::TriggerInventoryToggle);
	PlayerInputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AOJJ_Player::TriggerTutorialDialogueReveal);
	PlayerInputComponent->BindKey(EKeys::O, IE_Pressed, this, &AOJJ_Player::SetMoldingMachineModeShortcut);
	PlayerInputComponent->BindKey(EKeys::P, IE_Pressed, this, &AOJJ_Player::SetSynthesizerModeShortcut);
	PlayerInputComponent->BindKey(EKeys::T, IE_Pressed, this, &AOJJ_Player::SetTeleCommunicationTowerModeShortcut);
	PlayerInputComponent->BindKey(EKeys::X, IE_Pressed, this, &AOJJ_Player::SetDemolishModeShortcut);
	// [#184] C — 사다리 빌드 서브모드(레거시 BindKey, 핸들러에서 IsInBuildMode 가드. IMC 정석 전환은 후속 IMC 정리 때).
	PlayerInputComponent->BindKey(EKeys::C, IE_Pressed, this, &AOJJ_Player::SetLadderModeShortcut);
	// Foundation G/H는 #196에서 Enhanced Input(IA_SetFoundationMode/IA_SetRampMode)로 전환 —
	// 레거시 BindKey 제거(이중발화 차단). 바인딩은 위 BindAction 블록 + IMC_Build/BP 매핑(에디터).
}

void AOJJ_Player::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (!Controller || Axis.IsNearlyZero())
	{
		return;
	}

	// step-off 안착 보간 중엔 이동 입력 잠금(보간이 위치를 전담 → 진동/끼임 방지).
	if (bSteppingOff)
	{
		return;
	}

	// 등반 중(#184): 전후축(W/S)을 수직 이동으로 재해석, 좌우(D/A)는 무시(사다리 축 고정).
	// 상/하단 도달 시 등반 종료 — 상단은 step-off, 하단은 지면 복귀.
	if (CurrentLadder)
	{
		AddMovementInput(FVector::UpVector, Axis.Y);

		// 발 밑 Z로 상/하단 도달 판정. ClimbReachMargin 여유로 경계 떨림 방지(도달은 살짝 일찍).
		const float FeetZ = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		if (Axis.Y > 0.f && FeetZ >= CurrentLadder->GetClimbTopZ() - ClimbReachMargin)
		{
			EndClimb(/*bStepOffTop=*/true);
		}
		else if (Axis.Y < 0.f && FeetZ <= CurrentLadder->GetClimbBottomZ() + ClimbReachMargin)
		{
			EndClimb(/*bStepOffTop=*/false);
		}
		return;
	}

	// [#184] 등반 시작 — 진입-오버랩 의존 제거: 사다리 겹침 중 W(위) 누르면 시작(이미 트리거 안이어도).
	// BeginClimb 내부 가드(쿨다운·밑동 위치 FeetZ>Bottom+80)가 상면 재등반/직후 재진입을 막는다.
	if (Axis.Y > 0.f && OverlappingLadder.IsValid())
	{
		BeginClimb(OverlappingLadder.Get());
		if (CurrentLadder)
		{
			return; // 시작 성공 → 이번 프레임 걷기 입력 스킵(다음 프레임부터 등반 분기).
		}
	}

	// 카메라(컨트롤러) yaw 기준 전/후·좌/우 방향 산출 — pitch/roll은 이동에서 제외
	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, Axis.Y); // W/S
	AddMovementInput(Right, Axis.X);   // D/A

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
		{
			QuestManager->NotifyMainQuestInputAction(TEXT("Move"));
			QuestManager->NotifyTutorialEvent(TEXT("InputAction"), TEXT("Move"));
		}
	}
}

void AOJJ_Player::BeginClimb(AOJJ_Ladder* Ladder)
{
	if (!Ladder || CurrentLadder || bSteppingOff)
	{
		return; // 이미 등반 중·step-off 보간 중·잘못된 사다리 — 무시(단일 진실원).
	}

	// 재진입 쿨다운: step-off 직후 같은 트리거에 다시 잡혀 MOVE_Flying로 복귀하는 무한 토글(진동) 차단.
	const UWorld* World = GetWorld();
	if (World && World->GetTimeSeconds() < ClimbCooldownUntil)
	{
		return;
	}

	// 위치 기반 진입 가드(핵심): 발이 사다리 '밑동 근처'(하단 + 여유 이내)일 때만 등반 시작.
	// 상면에서 걸어다니는 캐릭터(발 Z ≈ 상단)가 전체높이 트리거에 닿아 등반으로 오인되는 것을 차단.
	const float FeetZ = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	if (FeetZ > Ladder->GetClimbBottomZ() + ClimbEntryZTolerance)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[Climb] 진입 거부 — 상면 높이(FeetZ=%.1f > Bottom=%.1f+%.1f)"),
			FeetZ, Ladder->GetClimbBottomZ(), ClimbEntryZTolerance);
		return;
	}

	CurrentLadder = Ladder;
	bClimbing = true;
	UE_LOG(LogTemp, Verbose, TEXT("[Climb] BeginClimb Bottom=%.1f Top=%.1f"),
		Ladder->GetClimbBottomZ(), Ladder->GetClimbTopZ());

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		// 중력 끄고 비행 모드로 수직 이동. 키를 떼면 빠르게 멈춰 사다리에서 호버하도록 제동 강하게.
		Movement->SetMovementMode(MOVE_Flying);
		Movement->GravityScale = 0.f;
		Movement->MaxFlySpeed = ClimbSpeed;
		Movement->BrakingDecelerationFlying = 2048.f;
		Movement->StopMovementImmediately();
	}
}

void AOJJ_Player::EndClimb(bool bStepOffTop)
{
	if (!CurrentLadder)
	{
		return;
	}

	AOJJ_Ladder* Ladder = CurrentLadder;
	CurrentLadder = nullptr;
	bClimbing = false;

	// 상단 도달: Foundation 상면으로 '부드럽게' 보간 안착(StepOffDuration). 즉시 텔레포트는 순간이동 느낌이라
	// 짧은 lerp로 부드럽게 + 보간 중 입력 잠금(진동 방지). 완료 시 Walking 복귀 + 쿨다운(Tick에서).
	if (bStepOffTop && Ladder && StepOffDuration > KINDA_SMALL_NUMBER)
	{
		const FVector LadderLoc = Ladder->GetActorLocation();
		const FVector Dir = Ladder->GetStepOffDirection();
		const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const float StepDist = GetCapsuleComponent()->GetScaledCapsuleRadius() + StepOffForward; // [#184] 캡슐반경 + 여유(기본 0)
		StepOffTarget = FVector(
			LadderLoc.X + Dir.X * StepDist,
			LadderLoc.Y + Dir.Y * StepDist,
			Ladder->GetClimbTopZ() + HalfHeight + StepOffZMargin);
		StepOffStart = GetActorLocation();
		StepOffElapsed = 0.f;
		bSteppingOff = true;

		// 보간 동안 낙하 방지(비행/중력0 유지). 위치는 Tick의 lerp가 전담.
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->SetMovementMode(MOVE_Flying);
			Movement->GravityScale = 0.f;
			Movement->StopMovementImmediately();
		}
		return; // Walking 복귀/쿨다운은 보간 완료 시(Tick)에서.
	}

	// 상단(보간 끔) 즉시 안착 또는 하단/기타 종료: 바로 걷기 복귀.
	if (bStepOffTop && Ladder)
	{
		const FVector LadderLoc = Ladder->GetActorLocation();
		const FVector Dir = Ladder->GetStepOffDirection();
		const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const float StepDist = GetCapsuleComponent()->GetScaledCapsuleRadius() + StepOffForward; // [#184] 캡슐반경 + 여유(기본 0)
		SetActorLocation(FVector(
			LadderLoc.X + Dir.X * StepDist,
			LadderLoc.Y + Dir.Y * StepDist,
			Ladder->GetClimbTopZ() + HalfHeight + StepOffZMargin),
			/*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
	}
	ResumeWalkingWithCooldown();
}

void AOJJ_Player::ResumeWalkingWithCooldown()
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->GravityScale = 1.f;
		Movement->StopMovementImmediately();
		Movement->SetMovementMode(MOVE_Walking);
	}

	// 재진입 쿨다운 개시 — step-off로 상면에 올라간 직후 같은 트리거에 다시 잡히는 진동 차단.
	if (const UWorld* World = GetWorld())
	{
		ClimbCooldownUntil = World->GetTimeSeconds() + ClimbReentryCooldown;
	}
}

void AOJJ_Player::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 안전망: 등반 중 표시인데 사다리가 사라졌으면(파괴/GC로 CurrentLadder=null) 걷기 복귀 — 비행/중력0 고착 방지.
	if (bClimbing && !CurrentLadder)
	{
		AbortClimb();
	}

	// [#184] 등반 중 X/Y를 사다리 등반 면으로 '부드럽게' 당김(즉시 SetActorLocation은 멀리서 시작 시 순간이동
	// → VInterpTo). Z는 등반(비행 수직)이 전담하므로 현재 Z 유지. 가까이서 W로 시작하면 거의 즉시 붙음.
	if (bClimbing && CurrentLadder)
	{
		const FVector Cur = GetActorLocation();
		const FVector Face = OJJ_GetClimbFaceLocation(CurrentLadder, Cur.Z);
		const FVector NewLoc = FMath::VInterpTo(Cur, FVector(Face.X, Face.Y, Cur.Z), DeltaSeconds, ClimbAttachInterpSpeed);
		SetActorLocation(NewLoc,
			/*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
	}

	if (!bSteppingOff)
	{
		return; // 평상시 무비용.
	}

	StepOffElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(StepOffElapsed / FMath::Max(StepOffDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);
	const float Smooth = FMath::SmoothStep(0.f, 1.f, Alpha); // ease in-out으로 자연스러운 안착
	SetActorLocation(FMath::Lerp(StepOffStart, StepOffTarget, Smooth),
		/*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);

	if (Alpha >= 1.f)
	{
		bSteppingOff = false;
		ResumeWalkingWithCooldown();
	}
}

void AOJJ_Player::AbortClimb()
{
	// 등반/step-off를 즉시 청산하고 걷기로 수렴(중력 복원). 빌드모드 진입·EndPlay·사다리 소멸 등
	// 비정상 종료 경로에서 MOVE_Flying/GravityScale=0 고착을 방지하는 단일 안전 청산점.
	if (!bClimbing && !bSteppingOff && !CurrentLadder)
	{
		return;
	}
	CurrentLadder = nullptr;
	bClimbing = false;
	bSteppingOff = false;
	ResumeWalkingWithCooldown();
}

FVector AOJJ_Player::OJJ_GetClimbFaceLocation(const AOJJ_Ladder* Ladder, float WorldZ) const
{
	// 등반 면 = Foundation 반대(바깥). 액터 전방(+X)=inward이므로 바깥 = -GetStepOffDirection.
	// 거리 = 캡슐 반경 + ClimbFaceGap(소량). 사다리가 벽면 라인에 있으니 캡슐이 면에 살짝 닿는 위치로 붙임.
	const FVector LadderLoc = Ladder->GetActorLocation();
	const FVector Outward = -Ladder->GetStepOffDirection();
	const float Dist = GetCapsuleComponent()->GetScaledCapsuleRadius() + ClimbFaceGap;
	return FVector(LadderLoc.X + Outward.X * Dist, LadderLoc.Y + Outward.Y * Dist, WorldZ);
}

void AOJJ_Player::NotifyLadderOverlap(AOJJ_Ladder* Ladder)
{
	// 근접 사다리 갱신만(시작은 W 입력에서). 등반 시작을 트리거 진입 순간에 묶지 않아, 이미 트리거 안이어도
	// W로 시작 가능 + 멀리서 강제 스냅(순간이동) 없음.
	if (Ladder)
	{
		OverlappingLadder = Ladder;
	}
}

void AOJJ_Player::NotifyLadderEndOverlap(AOJJ_Ladder* Ladder)
{
	// 이탈한 사다리가 현재 후보면 해제(다른 사다리 포인터는 안 건드림). 등반 중(CurrentLadder)은 별개라 영향 없음.
	if (OverlappingLadder.Get() == Ladder)
	{
		OverlappingLadder = nullptr;
	}
}

void AOJJ_Player::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	// 마우스 raw 델타가 그대로 회전량이 되지 않도록 감도 배율을 곱해 완화
	AddControllerYawInput(Axis.X * LookYawSensitivity);
	const float PitchSign = bInvertLookPitch ? -1.0f : 1.0f;
	AddControllerPitchInput(Axis.Y * LookPitchSensitivity * PitchSign);
}

void AOJJ_Player::Zoom(const FInputActionValue& Value)
{
	const float Scroll = Value.Get<float>();
	if (FMath::IsNearlyZero(Scroll))
	{
		return;
	}

	// 빌드모드면 뷰타겟인 BuildCamera를 줌(플레이어 SpringArm은 안 보이므로). 양쪽 모드 동일 휠 UX.
	if (BuildController && BuildController->IsInBuildMode())
	{
		if (BuildCamera)
		{
			BuildCamera->Zoom(Scroll);
		}
		return;
	}

	if (!SpringArm)
	{
		return;
	}

	// TPS: 스크롤 업(+) → 줌인(팔 길이 감소)
	const float NewLength = SpringArm->TargetArmLength - Scroll * ZoomStep;
	SpringArm->TargetArmLength = FMath::Clamp(NewLength, MinArmLength, MaxArmLength);
}

void AOJJ_Player::StartJumpAction(const FInputActionValue& Value)
{
	// 등반/step-off 중 점프 무시(상태 상호배제).
	if (CurrentLadder || bSteppingOff)
	{
		return;
	}

	Jump();

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
		{
			QuestManager->NotifyMainQuestInputAction(TEXT("Jump"));
			QuestManager->NotifyTutorialEvent(TEXT("InputAction"), TEXT("Jump"));
		}
	}
}

void AOJJ_Player::ToggleBuild(const FInputActionValue& Value)
{
	if (!BuildController)
	{
		return;
	}

	// 등반/step-off 중 빌드모드 진입 시 MOVE_Flying/중력0이 잔존하지 않도록 먼저 청산(걷기 복귀).
	AbortClimb();
	// Enter/Exit 자체 가드(같은 상태면 no-op) 덕분에 토글 라우팅만 하면 됨
	BuildController->ToggleBuildMode();

	// BuildController가 단일 진실원 — 실제 전환 결과(early-return 시 미전환)에 맞춰 플레이어측 적용.
	// 이로써 TargetGrid 미설정 등으로 Enter가 무산되면 카메라/가시성도 안 바뀜(half-state 방지).
	ApplyBuildModeView(BuildController->IsInBuildMode());

	if (BuildController->IsInBuildMode())
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
			{
				QuestManager->NotifyMainQuestBuildModeEntered();
				QuestManager->NotifyTutorialEvent(TEXT("BuildMode"));
			}
		}
	}
}

void AOJJ_Player::ApplyBuildModeView(bool bEntering)
{
	// 카메라 뷰타겟 블렌드 + 플레이어 가시성 + IMC 교체(Look 차단). (Pan/Rotate 핸들러는 3c에서 추가)
	APlayerController* PC = Cast<APlayerController>(GetController());
	UEnhancedInputLocalPlayerSubsystem* Subsystem = PC
		? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer())
		: nullptr;

	if (bEntering)
	{
		// 빌드모드 진입 시 머신 상호작용 위젯이 떠 있으면 닫는다 — F는 빌드모드 중 무시되지만
		// B(빌드 토글)는 위젯이 열려 있어도 동작하므로, 닫지 않으면 위젯+GameAndUI가 빌드모드 위에
		// 잔존하는 half-state가 된다(양방향 상호배제). IsValid()만 봐 자체 닫힘(미GC) 잔존 상태도 정리.
		if (MachineInteractWidgetInstance.IsValid())
		{
			CloseMachineInteractWidget(PC);
		}

		if (BuildCamera && BuildController)
		{
			// 진입 시 1회: 카메라 XY = 플레이어 현재 위치, Z = 그리드 평면(GetGridCenter().Z).
			// "선 데에서 빌드" — B 누른 순간 플레이어 위로 탑다운 배치. 빌드 중 추종 아님(WASD 패닝/QE 회전으로 이동).
			// Z를 그리드 평면에 고정해 플레이어가 높은 Foundation 위여도 탑다운 거리(화면 스케일) 일정.
			// 회전은 보존 — SpringArm pitch/arm이 높이·거리 담당.
			if (const AOJJ_Grid* Grid = BuildController->GetTargetGrid())
			{
				const FVector PlayerLoc = GetActorLocation();
				// 플레이어가 그리드 placement 범위 안이면 그 위로(선 데서 빌드), 밖이면 그리드 센터로 폴백.
				// off-grid 진입 시 커서가 무효 셀에서 시작해 hover/place가 막히는 것 방지(Codex 리뷰 2026-06-19).
				const FVector AnchorXY = Grid->IsValidGridCell(Grid->WorldToGrid(PlayerLoc))
					? PlayerLoc : Grid->GetGridCenter();
				const FVector CamLoc(AnchorXY.X, AnchorXY.Y, Grid->GetGridCenter().Z);
				BuildCamera->SetActorLocation(CamLoc);
			}
		}
		if (PC && BuildCamera)
		{
			PC->SetViewTargetWithBlend(BuildCamera, CameraBlendTime);
		}
		// 뷰타겟이 빌드캠이라 시각적 의미만 있지만, 탑다운에서 플레이어가 안 보이도록 숨김
		SetActorHiddenInGame(true);

		// 안전장치: 빌드모드 진입 시 아래에서 IMC_Player가 제거되면, Shift를 누른 채였을 경우
		// IA_Sprint의 Completed가 오지 않아 MaxWalkSpeed가 SprintSpeed에 고착된다(영구 질주).
		// 진입 시 걷기 속도로 강제 복귀해 복귀 후 질주 잔존을 방지.
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->MaxWalkSpeed = WalkSpeed;
		}

		// 입력: TPS IMC 제거 + 빌드 IMC 추가. 빌드 IMC엔 IA_Look이 없어 마우스 카메라 회전이 차단됨.
		// ⚠️ IMC_Build 미할당 시엔 절대 IMC_Player를 제거하지 않음 — 제거하면 입력이 전부 잠겨
		//    B키로 빌드모드를 빠져나올 수조차 없게 됨. 이 경우 IMC_Player 유지 + 경고만.
		if (Subsystem && IMC_Build)
		{
			Subsystem->RemoveMappingContext(IMC_Player);
			Subsystem->AddMappingContext(IMC_Build, 0);
		}
		else if (!IMC_Build)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[OJJ_Player] IMC_Build 미할당 — 빌드모드 Look 차단 불가(IMC_Player 유지). ")
				TEXT("BP_OJJ_Player에 IMC_Build 할당 필요."));
		}
		
		if (MainHUDWidgetInstance)
		{
			MainHUDWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
		}
		
		if (PC && BuildModeWidgetClass && !BuildModeWidgetInstance)
		{
			BuildModeWidgetInstance = CreateWidget<UUserWidget>(PC, BuildModeWidgetClass);
			if (BuildModeWidgetInstance)
			{
				BuildModeWidgetInstance->AddToViewport();
			}
		}

		// 재진입 정합(MainHUD의 Collapsed/Visible 토글과 대칭). Exit가 위젯을 Collapsed로 숨긴 채
		// 인스턴스를 유지하므로, 위 생성 가드(!BuildModeWidgetInstance)는 2회차+엔 스킵된다.
		// 여기서 Visible로 되돌리지 않으면 재진입 시 버튼 UI가 Collapsed로 방치돼 안 보인다
		// (1회차는 새로 생성돼 기본 Visible이라 정상). 최초 생성 직후엔 멱등.
		if (BuildModeWidgetInstance)
		{
			BuildModeWidgetInstance->SetVisibility(ESlateVisibility::Visible);
		}
	}
	else
	{
		if (PC)
		{
			// 복귀 뷰타겟은 플레이어 자신(소유 Pawn) — 빌드캠 없어도 안전하게 복귀
			PC->SetViewTargetWithBlend(this, CameraBlendTime);
		}
		SetActorHiddenInGame(false);

		// 입력 복귀: 빌드 IMC 제거 + TPS IMC 복원. 항상 IMC_Player 재추가(멱등) — 진입이
		// IMC_Build 미할당으로 스왑을 건너뛴 경우에도 안전하게 정상 상태로 수렴.
		if (Subsystem)
		{
			if (IMC_Build)
			{
				Subsystem->RemoveMappingContext(IMC_Build);
			}
			if (IMC_Player)
			{
				Subsystem->AddMappingContext(IMC_Player, 0);
			}
		}
		if (BuildModeWidgetInstance)
		{
			BuildModeWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (MainHUDWidgetInstance)
		{
			MainHUDWidgetInstance->SetVisibility(ESlateVisibility::Visible);
		}
	}
}

void AOJJ_Player::BuildPlace(const FInputActionValue& Value)
{
	if (!BuildController)
	{
		return;
	}
	// 빌드모드 밖이면 OnLeftClickPressed 내부 가드(bIsBuildMode)로 no-op
	BuildController->OnLeftClickPressed();
}

void AOJJ_Player::BuildPlaceReleased(const FInputActionValue& Value)
{
	if (!BuildController)
	{
		return;
	}
	// 좌클릭 뗌 — 컨베이어 모드면 드래그 커밋, 머신 모드면 내부 가드로 no-op.
	BuildController->OnLeftClickReleased();
}

void AOJJ_Player::BuildPlaceCanceled(const FInputActionValue& Value)
{
	if (!BuildController)
	{
		return;
	}
	// 좌클릭 입력 취소 — 진행 중 컨베이어 드래그 취소(머신 모드는 드래그 없어 no-op).
	BuildController->CancelConveyorDrag();
	BuildController->CancelPowerLineDrag();
}

void AOJJ_Player::SetMachineMode(const FInputActionValue& Value)
{
	if (!BuildController)
	{
		return;
	}
	BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Machine);
}

void AOJJ_Player::SetConveyorMode(const FInputActionValue& Value)
{
	if (!BuildController)
	{
		return;
	}
	BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Conveyor);

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
		{
			QuestManager->NotifyTutorialEvent(TEXT("SelectConveyorMode"));
		}
	}
}

void AOJJ_Player::SetPipeMode(const FInputActionValue& Value)
{
	if (!BuildController)
	{
		return;
	}
	BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Pipe);
}

// 물탱크 모드 직행(K키 — F4-4, IA 경로). 파이프 핸들러 미러 — 콘솔 OJJ_SetBuildMode tank와 동일 위임.
void AOJJ_Player::SetTankMode(const FInputActionValue& Value)
{
	if (!BuildController)
	{
		return;
	}
	BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::LiquidTank);
}

void AOJJ_Player::OJJ_SetBuildMode(const FString& ModeName)
{
	// [임시 진입로] 콘솔 exec — IA/UI 미와이어링 모드(pipe/tank) 전용. 빌드모드 여부는 검사하지
	// 않음 — 기존 모드 키(SetConveyorMode 등)와 동일 정책(빌드모드 밖 호출 = 다음 진입 모드 예약).
	if (!BuildController)
	{
		return;
	}
	const FString Lower = ModeName.ToLower();
	if (Lower == TEXT("pipe"))
	{
		BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Pipe);
	}
	else if (Lower == TEXT("tank") || Lower == TEXT("liquidtank"))
	{
		BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::LiquidTank);
	}
	else if (Lower == TEXT("tower") || Lower == TEXT("telecommunicationtower"))
	{
		BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::TeleCommunicationTower);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[OJJ_Player] OJJ_SetBuildMode: unknown mode '%s' (pipe|tank|tower)"), *ModeName);
	}
}

void AOJJ_Player::SetPowerNodeMode(const FInputActionValue& Value)
{
	if (!BuildController)
	{
		return;
	}
	BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::PowerNode);

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
		{
			QuestManager->NotifyTutorialEvent(TEXT("SelectPowerNodeMode"));
		}
	}
}

void AOJJ_Player::SetShieldMode(const FInputActionValue& Value)
{
	if (!BuildController)
	{
		return;
	}
	BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Shield);
}

// 전선 드래그 모드 진입만 추가 — 드래그 로직(BeginPowerLineDrag/CommitPowerLineDrag)은 팀원 구현 무수정.
void AOJJ_Player::SetPowerLineMode(const FInputActionValue& Value)
{
	if (!BuildController)
	{
		return;
	}
	BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::PowerLine);

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
		{
			QuestManager->NotifyTutorialEvent(TEXT("SelectPowerLineMode"));
		}
	}
}

void AOJJ_Player::SetPowerPlantMode(const FInputActionValue& Value)
{
	if (!BuildController)
	{
		return;
	}
	BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::PowerPlant);

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
		{
			QuestManager->NotifyTutorialEvent(TEXT("SelectPowerPlantMode"));
		}
	}
}

void AOJJ_Player::SetGrinderMode(const FInputActionValue& Value)
{
	if (!BuildController)
	{
		return;
	}
	BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Grinder);
}

void AOJJ_Player::SetMinerMode(const FInputActionValue& Value)
{
	if (!BuildController)
	{
		return;
	}
	BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Miner);

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
		{
			QuestManager->NotifyTutorialEvent(TEXT("SelectMinerMode"));
		}
	}
}

void AOJJ_Player::SetPumpMode(const FInputActionValue& Value)
{
	if (!BuildController)
	{
		return;
	}
	BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Pump);
}

void AOJJ_Player::SetSmelterMode(const FInputActionValue& Value)
{
	if (!BuildController)
	{
		return;
	}
	BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Smelter);

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
		{
			QuestManager->NotifyTutorialEvent(TEXT("SelectSmelterMode"));
		}
	}
}

void AOJJ_Player::SetMoldingMachineModeShortcut()
{
	if (!BuildController)
	{
		return;
	}
	BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::MoldingMachine);
}

void AOJJ_Player::SetSynthesizerModeShortcut()
{
	if (!BuildController)
	{
		return;
	}
	BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Synthesizer);
}

// TeleCommunicationTower build mode shortcut.
void AOJJ_Player::SetTeleCommunicationTowerModeShortcut()
{
	if (!BuildController)
	{
		return;
	}
	BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::TeleCommunicationTower);
}

void AOJJ_Player::SetDemolishModeShortcut()
{
	if (!BuildController || !BuildController->IsInBuildMode())
	{
		return;
	}

	BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Demolish);
}

// [#184] C키 — 사다리 빌드 서브모드 진입. 레거시 BindKey라 IMC 게이팅이 없으므로 빌드모드 가드 필수(Demolish 패턴).
void AOJJ_Player::SetLadderModeShortcut()
{
	if (!BuildController || !BuildController->IsInBuildMode())
	{
		return;
	}

	BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Ladder);
}

void AOJJ_Player::SetWarehouseMode(const FInputActionValue& Value)
{
	if (!BuildController)
	{
		return;
	}
	BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Warehouse);

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
		{
			QuestManager->NotifyTutorialEvent(TEXT("SelectWarehouseMode"));
		}
	}
}

// 철거 모드 진입(X키). 호버 대상 빨강 하이라이트 + 좌클릭 제거.
void AOJJ_Player::SetDemolishMode(const FInputActionValue& Value)
{
	if (!BuildController)
	{
		return;
	}
	BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Demolish);

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
		{
			QuestManager->NotifyTutorialEvent(TEXT("DemolishMode"));
		}
	}
}

// 평판 Foundation 모드 진입(G키 — #196 IA 전환). 다른 모드 핸들러(탱크/파이프)와 동일 구조 —
// BuildController 위임만. 모드 진입/호버 갱신/빌드모드 밖 무해성은 컨트롤러 소관.
void AOJJ_Player::SetFoundationMode(const FInputActionValue& Value)
{
	if (!BuildController)
	{
		return;
	}
	BuildController->OJJ_SelectFoundationKind(false);
}

// 램프 Foundation 모드 진입(H키 — #196 IA 전환, F3-2.5 T 토글 대체).
void AOJJ_Player::SetRampFoundationMode(const FInputActionValue& Value)
{
	if (!BuildController)
	{
		return;
	}
	BuildController->OJJ_SelectFoundationKind(true);
}

void AOJJ_Player::StartSprint(const FInputActionValue& Value)
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = SprintSpeed;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
		{
			QuestManager->NotifyMainQuestInputAction(TEXT("Sprint"));
			QuestManager->NotifyTutorialEvent(TEXT("InputAction"), TEXT("Sprint"));
		}
	}
}

void AOJJ_Player::StopSprint(const FInputActionValue& Value)
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = WalkSpeed;
	}
}

void AOJJ_Player::BuildPan(const FInputActionValue& Value)
{
	// IA_BuildPan은 IMC_Build에만 매핑되므로 빌드모드에서만 호출됨. Pan 내부에서 0입력/DeltaSeconds 처리.
	if (BuildCamera)
	{
		BuildCamera->Pan(Value.Get<FVector2D>());
	}
}

void AOJJ_Player::BuildRotate(const FInputActionValue& Value)
{
	const float RotateInput = Value.Get<float>() * -1;

	if (BuildCamera)
	{
		BuildCamera->Rotate(RotateInput);
	}

	if (FMath::IsNearlyZero(RotateInput))
	{
		return;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
		{
			QuestManager->NotifyTutorialEvent(
				RotateInput > 0.0f ? TEXT("RotateViewRight") : TEXT("RotateViewLeft"));
		}
	}
}

void AOJJ_Player::BuildRotateMachine(const FInputActionValue& Value)
{
	// 호버 머신 회전은 BuildController가 상태(step) 소유 — 빌드모드 가드도 거기서 처리.
	if (BuildController)
	{
		BuildController->RotateHoverClockwise();
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
		{
			QuestManager->NotifyTutorialEvent(TEXT("RotatePlacement"));
		}
	}
}

void AOJJ_Player::SendOperatorGuideRequest()
{
	UGameInstance* GameInstance = GetGameInstance();
	UFactoryAgentClientSubsystem* AgentClient = GameInstance
		? GameInstance->GetSubsystem<UFactoryAgentClientSubsystem>()
		: nullptr;
	if (!AgentClient)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OJJ_Player] FactoryAgentClientSubsystem not found."));
		return;
	}

	if (!AgentClient->IsConnected())
	{
		if (AgentClient->GetConnectionState() == EFactoryAgentConnectionState::Disconnected)
		{
			AgentClient->ConnectToDefaultServer();
		}
		UE_LOG(LogTemp, Warning, TEXT("[OJJ_Player] Factory agent WebSocket is not connected yet."));
		return;
	}

	const FString Question = TEXT("\uAE30\uC5B4 \uB9CC\uB4E4\uB824\uBA74 \uBB50\uAC00 \uD544\uC694\uD574?");
	if (AgentClient->SendOperatorGuideQuestion(Question, TEXT("unreal-ui-001")))
	{
		UE_LOG(LogTemp, Log, TEXT("[OJJ_Player] Sent operator guide request."));
	}
}

void AOJJ_Player::OnInteract(const FInputActionValue& Value)
{
	if (!IsLocallyControlled()) return;

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	if (BuildController && BuildController->IsInBuildMode()) return;
	
	FInputModeGameAndUI QuickFixMode;
	QuickFixMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	// 현재 포커스를 강제로 게임 뷰포트로 지정하여 굳어버린 키보드 입력을 깨웁니다.
	QuickFixMode.SetWidgetToFocus(nullptr); 
	PC->SetInputMode(QuickFixMode);

    // 일반 기계창, 창고창, 가방창 중 하나라도 열려 있으면 무조건 전부 다 닫습니다.
    if (bIsInventoryOpen || 
        (MachineInteractWidgetInstance.IsValid() && MachineInteractWidgetInstance->IsInViewport()) ||
        (WarehouseInteractWidgetInstance && WarehouseInteractWidgetInstance->IsInViewport())) // 창고 닫기 가드 추가
    {
       if (MachineInteractWidgetInstance.IsValid())
       {
          MachineInteractWidgetInstance->RemoveFromParent();
          MachineInteractWidgetInstance = nullptr;
       }

       // 띄워져 있던 창고 창을 부모로부터 제거하고 메모리 초기화
       if (WarehouseInteractWidgetInstance)
       {
          WarehouseInteractWidgetInstance->RemoveFromParent();
          WarehouseInteractWidgetInstance = nullptr;
       }

       if (InventoryWidgetInstance)
       {
          InventoryWidgetInstance->RemoveFromParent();
       }
       bIsInventoryOpen = false;
       GetWorldTimerManager().ClearTimer(InventoryRefreshTimerHandle);

       PC->SetInputMode(FInputModeGameOnly());
       PC->SetShowMouseCursor(false);
       return;
    }

    UWorld* World = GetWorld();
    if (!Camera || !World) return;

    const FVector TraceStart = Camera->GetComponentLocation();
    const FVector TraceEnd = TraceStart + Camera->GetForwardVector() * MaxInteractDistance;
    FHitResult Hit;
    FCollisionQueryParams TraceParams(FName(TEXT("OJJMachineInteract")), false, this);
    const bool bHit = World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, TraceParams);
    if (!bHit) return;

    AMachineBase* Machine = Cast<AMachineBase>(Hit.GetActor());
    if (!Machine) return;

    // 바라본 기계가 '창고' 계열인지 검사합니다
    if (Machine->IsA(AWarehousePort::StaticClass()) || Machine->GetName().Contains(TEXT("Warehouse")))
    {
       if (!WarehouseInteractWidgetClass)
       {
          UE_LOG(LogTemp, Warning, TEXT("[OJJ_Player] WarehouseInteractWidgetClass 미할당! BP에서 할당하세요."));
          return;
       }

       // 창고 전용 UI 창
       UUI_WarehouseInteract* WHWidget = CreateWidget<UUI_WarehouseInteract>(PC, WarehouseInteractWidgetClass);
       if (WHWidget)
       {
          WHWidget->SetTargetMachine(Machine);
          WarehouseInteractWidgetInstance = WHWidget;
          WHWidget->AddToViewport();
          WHWidget->OnClosed.AddDynamic(this, &AOJJ_Player::RestoreGameInputMode);
       }
    }
    else
    {
       // 일반 기계(제련기, 분쇄기 등)라면 기존 일반 상호작용 UI 창
       if (!MachineInteractWidgetClass) return;

       UUI_MachineInteract* Widget = CreateWidget<UUI_MachineInteract>(PC, MachineInteractWidgetClass);
       if (Widget)
       {
          Widget->SetTargetMachine(Machine);
          MachineInteractWidgetInstance = Widget;
          Widget->AddToViewport();
          Widget->OnClosed.AddDynamic(this, &AOJJ_Player::RestoreGameInputMode);
       }
    }

    // 창고 포트 계열일 때 우측에 유저 가방 창 세트로 동시 소환하는 로직 (기존 유지)
    if (Machine->IsA(AWarehousePort::StaticClass()) || Machine->GetName().Contains(TEXT("Warehouse")))
    {
       if (!InventoryWidgetInstance && InventoryWidgetClass)
       {
          InventoryWidgetInstance = CreateWidget<UUI_Inventory>(PC, InventoryWidgetClass);
       }

    	if (InventoryWidgetInstance)
    	{
    		InventoryWidgetInstance->AdjustInventoryLayout(true); 

    		InventoryWidgetInstance->AddToViewport();
    		InventoryWidgetInstance->RefreshInventoryWindow();
    		bIsInventoryOpen = true;

    		GetWorldTimerManager().SetTimer(
			   InventoryRefreshTimerHandle, 
			   this, 
			   &AOJJ_Player::UpdateInventoryRealtime, 
			   0.1f, 
			   true
			);
            
    		UE_LOG(LogTemp, Log, TEXT("[창고 F키 성공] 창고 전용 창과 가방 창을 완벽하게 동시 활성화했습니다!"));
    	}
    }

    FInputModeGameAndUI InputModeData;
    InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PC->SetInputMode(InputModeData);
    PC->SetShowMouseCursor(true);
}

void AOJJ_Player::CloseMachineInteractWidget(APlayerController* PC)
{
	// 1. 기존 일반 기계창 끄기
	if (UUI_MachineInteract* Widget = MachineInteractWidgetInstance.Get())
	{
		Widget->RemoveFromParent();
	}
	MachineInteractWidgetInstance = nullptr;

	// 2. 창고 전용 UI 창이 켜져 있었다면 부모에게서 떼어내고 클리어
	if (WarehouseInteractWidgetInstance)
	{
		WarehouseInteractWidgetInstance->RemoveFromParent();
		WarehouseInteractWidgetInstance = nullptr;
	}

	if (PC)
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
}

void AOJJ_Player::RestoreGameInputMode()
{
	// 스테일 브로드캐스트 가드 — 우리가 위젯 A를 닫고 새 위젯 B를 연 사이에 A의 지연된
	// NativeDestruct가 broadcast될 수 있다. 현재 살아있는 위젯이 열려 있으면 그 상태를 건드리지 않는다
	// (B의 weak 포인터/GameAndUI를 망가뜨리지 않도록).
	if (MachineInteractWidgetInstance.IsValid() && MachineInteractWidgetInstance->IsInViewport())
	{
		return;
	}

	// 멱등 — 우리 직접 닫기(CloseMachineInteractWidget)로 이미 복원된 뒤 지연 Destruct 브로드캐스트가
	// 한 번 더 들어와도 사실상 no-op. weak 인스턴스 정리.
	MachineInteractWidgetInstance = nullptr;

	// ⚠️ pawn 소멸 중 위젯 Destruct로 재진입할 수 있어 컨트롤러 유효성 체크(무효면 복원 스킵 —
	//    복원 대상 자체가 없으므로 안전).
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
}
void AOJJ_Player::TriggerHUDQuestRequest()
{
	// 빌드모드 중에는 화면에 메인 HUD가 꺼지므로 슬래시 키 작동을 막습니다.
	if (BuildController && BuildController->IsInBuildMode())
	{
		return;
	}
	if (UUI_MainHUD* MainHUD = Cast<UUI_MainHUD>(MainHUDWidgetInstance))
	{
		// 퀘스트 요청 함수 원격 실행
		MainHUD->OnRequestQuestsClicked();
	}
}
void AOJJ_Player::TriggerHUDQuestWindowToggle()
{
	// 빌드모드 내부에 있는 퀘스트 창을 찾아 토글합니다.
	if (BuildController && BuildController->IsInBuildMode())
	{
		if (BuildModeWidgetInstance)
		{
			UUI_QuestWindow* BuildQuestWindow = Cast<UUI_QuestWindow>(BuildModeWidgetInstance->GetWidgetFromName(TEXT("WBP_QuestWindow")));
			if (BuildQuestWindow)
			{
				BuildQuestWindow->ToggleQuestWindow();
			}
		}
		return; // 빌드모드 처리가 끝났으므로 아래 일반 HUD 로직으로 내려가지 못하게 차단
	}

	// 기존 메인 HUD 내부의 퀘스트 창 토글
	if (UUI_MainHUD* MainHUD = Cast<UUI_MainHUD>(MainHUDWidgetInstance))
	{
		MainHUD->ToggleQuestWindow();
	}
}

void AOJJ_Player::TriggerHUDAIGuideToggle()
{
	if (BuildController && BuildController->IsInBuildMode()) return;

	if (UUI_MainHUD* MainHUD = Cast<UUI_MainHUD>(MainHUDWidgetInstance))
	{
		// HUD 토글 함수 원격 호출
		MainHUD->ToggleAIGuideWindow();
	}
}

void AOJJ_Player::TriggerTutorialDialogueReveal()
{
	if (UUI_MainHUD* MainHUD = Cast<UUI_MainHUD>(MainHUDWidgetInstance))
	{
		if (MainHUD->IsGuideWindowOpen())
		{
			return;
		}
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
		{
			if (QuestManager->HasPendingTutorialStartDialogue())
			{
				QuestManager->RevealPendingTutorialStartDialogue();
			}
		}
	}
}

void AOJJ_Player::TriggerInventoryToggle()
{
    if (BuildController && BuildController->IsInBuildMode()) return;

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;

	// 1. 이미 열려 있다면 닫기
	if (bIsInventoryOpen)
	{
		if (InventoryWidgetInstance)
		{
			InventoryWidgetInstance->RemoveFromParent();
			bIsInventoryOpen = false;

			if (UGameInstance* GameInstance = GetGameInstance())
			{
				if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
				{
					QuestManager->NotifyTutorialEvent(TEXT("InventoryClose"));
				}
			}
		}
		
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);

       GetWorldTimerManager().ClearTimer(InventoryRefreshTimerHandle);
       return;
    }
	
    UWorld* World = GetWorld();
    if (!Camera || !World) return;

    FVector TraceStart = Camera->GetComponentLocation();
    FVector TraceEnd = TraceStart + Camera->GetForwardVector() * MaxInteractDistance;
    FHitResult Hit;
    FCollisionQueryParams TraceParams(FName(TEXT("OJJInventoryInteract")), false, this);

    bool bHit = World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, TraceParams);
    
    // 창고 컴포넌트 구조체 캐스팅
    bool bIsValidWarehouse = false;
    FName InventoryFormFilter = NAME_None;
    if (bHit && Hit.GetActor())
    {
        // AWarehousePort 뿐만 아니라 일반 머신 베이스 계열인지도 체크
        if (Hit.GetActor()->IsA(AWarehousePort::StaticClass()) || Hit.GetActor()->GetName().Contains(TEXT("Warehouse")))
        {
            bIsValidWarehouse = true;
            InventoryFormFilter = TEXT("solid");
        }
        else if (Hit.GetActor()->IsA(ALiquidTank::StaticClass()) || Hit.GetActor()->GetName().Contains(TEXT("LiquidTank")))
        {
            bIsValidWarehouse = true;
            InventoryFormFilter = TEXT("liquid");
        }
        else if (Hit.GetActor()->IsA(AMachineBase::StaticClass()))
        {
            bIsValidWarehouse = true;
        }
    }

    // 테스트 환경 편의를 위해, 바라보고 있는 액터가 감지가 안 되더라도 
    // 최소한의 방어선만 치고 UI가 무조건 생성되도록 우회 통과시킵니다.
    if (!bIsValidWarehouse)
    {
        UE_LOG(LogTemp, Warning, TEXT("[가방 경고] 시선 끝에 창고(WarehousePort)가 정확히 조준되지 않았습니다! (디버깅을 위해 생성을 강제 진행합니다)"));
    }

    // 3. 인벤토리 오픈 및 UI 생성
    if (!InventoryWidgetInstance && InventoryWidgetClass)
    {
       InventoryWidgetInstance = CreateWidget<UUI_Inventory>(PC, InventoryWidgetClass);
    }

	if (InventoryWidgetInstance)
	{
		// 화면에 뷰포트 업로드하기 직전에 연산 가동!
		// 바라보는 시선 끝에 창고가 없으면(bIsValidWarehouse = false) 왼쪽(중앙 방향)으로 이동합니다.
		InventoryWidgetInstance->AdjustInventoryLayout(bIsValidWarehouse);

		// 중복되던 슬롯 필터 및 뷰포트 추가 로직 하나로 깔끔하게 압축
		InventoryWidgetInstance->SetItemFormFilter(InventoryFormFilter);
		InventoryWidgetInstance->AddToViewport();
		InventoryWidgetInstance->RefreshInventoryWindow();
		bIsInventoryOpen = true;
       
		// UI 포커스 및 인풋 모드 설정
		FInputModeGameAndUI InputModeData;
		InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputModeData);
		PC->SetShowMouseCursor(true);

		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
			{
				QuestManager->NotifyTutorialEvent(TEXT("InventoryOpen"));
			}
		}
       
		// 위젯 자체에 강제로 마우스 포커스를 심어 드래그 스타트 신호 보호
		InventoryWidgetInstance->SetKeyboardFocus();

		GetWorldTimerManager().SetTimer(
		   InventoryRefreshTimerHandle, 
		   this, 
		   &AOJJ_Player::UpdateInventoryRealtime, 
		   0.1f, 
		   true
		);
	}
}

void AOJJ_Player::OJJ_TutorialAdvance()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
		{
			QuestManager->CompleteCurrentTutorialQuestForTest();
		}
	}
}

void AOJJ_Player::OJJ_TutorialLog()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
		{
			QuestManager->LogCurrentTutorialQuestTestState();
		}
	}
}

void AOJJ_Player::OJJ_SetMachineLevel(const FString& MachineTypeName, int32 NewLevel)
{
	if (MachineTypeName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[OJJ_SetMachineLevel] MachineTypeName is empty."));
		return;
	}

	if (NewLevel <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OJJ_SetMachineLevel] NewLevel must be greater than 0."));
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UMachineSubsystem* MachineSubsystem = GameInstance
		? GameInstance->GetSubsystem<UMachineSubsystem>()
		: nullptr;
	if (!MachineSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OJJ_SetMachineLevel] MachineSubsystem not found."));
		return;
	}

	const FName MachineType(*MachineTypeName);
	if (!MachineSubsystem->SetMachineLevel(MachineType, NewLevel))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[OJJ_SetMachineLevel] Failed to set %s to level %d."),
			*MachineTypeName,
			NewLevel);
		return;
	}

	UE_LOG(LogTemp, Log,
		TEXT("[OJJ_SetMachineLevel] %s level set to %d."),
		*MachineTypeName,
		NewLevel);
}

void AOJJ_Player::OJJ_UpgradeMachineLevel(const FString& MachineTypeName, int32 UpgradeCount)
{
	if (MachineTypeName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[OJJ_UpgradeMachineLevel] MachineTypeName is empty."));
		return;
	}

	if (UpgradeCount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OJJ_UpgradeMachineLevel] UpgradeCount must be greater than 0."));
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UMachineSubsystem* MachineSubsystem = GameInstance
		? GameInstance->GetSubsystem<UMachineSubsystem>()
		: nullptr;
	if (!MachineSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OJJ_UpgradeMachineLevel] MachineSubsystem not found."));
		return;
	}

	const FName MachineType(*MachineTypeName);
	int32 SuccessCount = 0;
	for (int32 Index = 0; Index < UpgradeCount; ++Index)
	{
		if (!MachineSubsystem->SetMachineLevel(MachineType, MachineSubsystem->GetMachineLevel(MachineType) + 1))
		{
			break;
		}

		++SuccessCount;
	}

	if (SuccessCount == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[OJJ_UpgradeMachineLevel] Failed to upgrade %s. Current level=%d."),
			*MachineTypeName,
			MachineSubsystem->GetMachineLevel(MachineType));
		return;
	}

	UE_LOG(LogTemp, Log,
		TEXT("[OJJ_UpgradeMachineLevel] %s upgraded by %d. Current level=%d."),
		*MachineTypeName,
		SuccessCount,
		MachineSubsystem->GetMachineLevel(MachineType));
}

void AOJJ_Player::OJJ_ResetGame()
{
	UGameInstance* GameInstance = GetGameInstance();
	UFactorySaveSubsystem* SaveSubsystem = GameInstance
		? GameInstance->GetSubsystem<UFactorySaveSubsystem>()
		: nullptr;
	if (!SaveSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OJJ_ResetGame] FactorySaveSubsystem not found."));
		return;
	}

	SaveSubsystem->ResetToNewGame();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FString CurrentMapName = UWorld::RemovePIEPrefix(World->GetMapName());
	UGameplayStatics::OpenLevel(this, FName(*CurrentMapName));
}

void AOJJ_Player::UpdateInventoryRealtime()
{
	if (bIsInventoryOpen && InventoryWidgetInstance)
	{
		InventoryWidgetInstance->UpdateSlotQuantitiesOnly();
	}
	else
	{
		GetWorldTimerManager().ClearTimer(InventoryRefreshTimerHandle);
	}
}
