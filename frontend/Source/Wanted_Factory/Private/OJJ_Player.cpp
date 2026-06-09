// Fill out your copyright notice in the Description page of Project Settings.


#include "OJJ_Player.h"

#include "Camera/CameraComponent.h"
#include "FactoryAgentClientSubsystem.h"
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
#include "OJJ_Grid.h"
#include "Blueprint/UserWidget.h"
#include "MachineBase.h"
#include "UI/UI_MachineInteract.h"
#include "UI/UI_MainHUD.h"
#include "UI/UI_Inventory.h"
#include "Machines/WarehousePort.h"

AOJJ_Player::AOJJ_Player()
{
	PrimaryActorTick.bCanEverTick = false;

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
	
	ConnectFactoryAgentClient();
}

void AOJJ_Player::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 폰 파괴/언포제스 시 열려 있던 머신 상호작용 위젯·입력모드 정리.
	// ⚠️ EndPlay 시점엔 PlayerController가 이미 무효일 수 있다 — Cast가 null을 반환하면
	//    CloseMachineInteractWidget의 PC null-가드가 입력모드 복원을 스킵하고 위젯 제거만 수행한다
	//    (컨트롤러가 없으면 복원할 대상도 없으므로 안전).
	CloseMachineInteractWidget(Cast<APlayerController>(GetController()));

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
		EnhancedInput->BindAction(IA_Jump, ETriggerEvent::Started, this, &ACharacter::Jump);
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
}

void AOJJ_Player::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (!Controller || Axis.IsNearlyZero())
	{
		return;
	}

	// 카메라(컨트롤러) yaw 기준 전/후·좌/우 방향 산출 — pitch/roll은 이동에서 제외
	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, Axis.Y); // W/S
	AddMovementInput(Right, Axis.X);   // D/A
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
	if (!SpringArm || FMath::IsNearlyZero(Scroll))
	{
		return;
	}

	// 스크롤 업(+) → 줌인(팔 길이 감소)
	const float NewLength = SpringArm->TargetArmLength - Scroll * ZoomStep;
	SpringArm->TargetArmLength = FMath::Clamp(NewLength, MinArmLength, MaxArmLength);
}

void AOJJ_Player::ToggleBuild(const FInputActionValue& Value)
{
	if (!BuildController)
	{
		return;
	}
	// Enter/Exit 자체 가드(같은 상태면 no-op) 덕분에 토글 라우팅만 하면 됨
	BuildController->ToggleBuildMode();

	// BuildController가 단일 진실원 — 실제 전환 결과(early-return 시 미전환)에 맞춰 플레이어측 적용.
	// 이로써 TargetGrid 미설정 등으로 Enter가 무산되면 카메라/가시성도 안 바뀜(half-state 방지).
	ApplyBuildModeView(BuildController->IsInBuildMode());
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
			// 진입할 때마다 그리드 중심으로 카메라 재배치 — 그리드가 동적으로 커져도 매 진입 시 맞춰짐.
			// XY/Z만 이동(회전은 보존). SpringArm pitch/arm이 높이·거리 담당.
			if (const AOJJ_Grid* Grid = BuildController->GetTargetGrid())
			{
				BuildCamera->SetActorLocation(Grid->GetGridCenter());
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
}

void AOJJ_Player::SetPowerNodeMode(const FInputActionValue& Value)
{
	if (!BuildController)
	{
		return;
	}
	BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::PowerNode);
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
}

void AOJJ_Player::SetPowerPlantMode(const FInputActionValue& Value)
{
	if (!BuildController)
	{
		return;
	}
	BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::PowerPlant);
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
}

// 창고 모드 진입(1키, generic Machine 진입 키 대체). generic SetMachineMode는 보존(미바인딩 시 dead) — 코드 삭제 없음.
void AOJJ_Player::SetWarehouseMode(const FInputActionValue& Value)
{
	if (!BuildController)
	{
		return;
	}
	BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Warehouse);
}

// 철거 모드 진입(X키). 호버 대상 빨강 하이라이트 + 좌클릭 제거.
void AOJJ_Player::SetDemolishMode(const FInputActionValue& Value)
{
	if (!BuildController)
	{
		return;
	}
	BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Demolish);
}

void AOJJ_Player::StartSprint(const FInputActionValue& Value)
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = SprintSpeed;
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
	if (BuildCamera)
	{
		BuildCamera->Rotate(Value.Get<float>());
	}
}

void AOJJ_Player::BuildRotateMachine(const FInputActionValue& Value)
{
	// 호버 머신 회전은 BuildController가 상태(step) 소유 — 빌드모드 가드도 거기서 처리.
	if (BuildController)
	{
		BuildController->RotateHoverClockwise();
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
	// UI는 로컬 전용 — 멀티플레이에서 비로컬 폰의 입력으로 위젯을 띄우지 않도록 가드.
	if (!IsLocallyControlled())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	// 빌드모드 중에는 F 무시 — 빌드 입력/탑다운 카메라와 상호작용 UI를 상호배제(half-state 방지).
	if (BuildController && BuildController->IsInBuildMode())
	{
		return;
	}

	// 토글 닫기: 뷰포트에 떠 있는 위젯이 있으면 닫는다.
	// IsValid()에 더해 IsInViewport()까지 보는 이유 — 위젯이 자체 BTN_Close(RemoveFromParent)로
	// 닫혀도 객체는 GC 전까지 살아 있어 IsValid()만으론 "열림"으로 오판하기 때문.
	// (자체 닫기 시 입력모드/커서 즉시 복원은 위젯 OnClosed 델리게이트 → RestoreGameInputMode가 처리.)
	if (MachineInteractWidgetInstance.IsValid() && MachineInteractWidgetInstance->IsInViewport())
	{
		CloseMachineInteractWidget(PC);
		return;
	}

	UWorld* World = GetWorld();
	if (!Camera || !World)
	{
		return;
	}

	// 카메라 전방 라인 트레이스 — 빌드모드 호버와 동일 채널(ECC_Visibility), 거리만 MaxInteractDistance.
	const FVector TraceStart = Camera->GetComponentLocation();
	const FVector TraceEnd = TraceStart + Camera->GetForwardVector() * MaxInteractDistance;
	FHitResult Hit;
	FCollisionQueryParams TraceParams(FName(TEXT("OJJMachineInteract")), /*bTraceComplex=*/ false, this);
	const bool bHit = World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, TraceParams);
	if (!bHit)
	{
		return;
	}

	// 히트 액터가 머신(또는 파생)일 때만 상호작용.
	AMachineBase* Machine = Cast<AMachineBase>(Hit.GetActor());
	if (!Machine)
	{
		return;
	}

	if (!MachineInteractWidgetClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[OJJ_Player] MachineInteractWidgetClass 미할당 — 머신 상호작용(F) 비활성. ")
			TEXT("BP_OJJ_Player에 WBP_MachineInteract 할당 필요."));
		return;
	}

	UUI_MachineInteract* Widget = CreateWidget<UUI_MachineInteract>(PC, MachineInteractWidgetClass);
	if (!Widget)
	{
		return;
	}
	Widget->AddToViewport();
	// 위젯의 모든 닫힘 경로(특히 자체 BTN_Close) 통지 구독 — 닫히면 입력모드/커서 즉시 복원.
	Widget->OnClosed.AddDynamic(this, &AOJJ_Player::RestoreGameInputMode);
	// 머신 참조 전달 — 위젯의 모든 실데이터(입출력/상태/진행도/내구도) 표시가 이 참조에 의존.
	Widget->SetTargetMachine(Machine);
	MachineInteractWidgetInstance = Widget;

	// 열 때: 마우스로 위젯과 상호작용 가능하도록 GameAndUI + 커서 표시.
	PC->SetInputMode(FInputModeGameAndUI());
	PC->SetShowMouseCursor(true);
}

void AOJJ_Player::CloseMachineInteractWidget(APlayerController* PC)
{
	if (UUI_MachineInteract* Widget = MachineInteractWidgetInstance.Get())
	{
		Widget->RemoveFromParent();
	}
	MachineInteractWidgetInstance = nullptr;

	// 닫을 때: 게임 전용 입력 복원 + 커서 숨김.
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
	// 빌드모드 중에는 우측 퀘스트 레이아웃이 꺼지므로 단축키 작동 차단
	if (BuildController && BuildController->IsInBuildMode())
	{
		return;
	}

	// 캐릭터가 들고 있던 위젯 인스턴스를 MainHUD 타입으로 캐스팅하여 애니메이션 함수 작동
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
		}
		
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);

		GetWorldTimerManager().ClearTimer(InventoryRefreshTimerHandle);
		return;
	}

	// 2. 레이저 검사 (창고 포트인지 확인)
	UWorld* World = GetWorld();
	if (!Camera || !World) return;

	FVector TraceStart = Camera->GetComponentLocation();
	FVector TraceEnd = TraceStart + Camera->GetForwardVector() * MaxInteractDistance;
	FHitResult Hit;
	FCollisionQueryParams TraceParams(FName(TEXT("OJJInventoryInteract")), false, this);

	bool bHit = World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, TraceParams);
	if (!bHit) return;

	AWarehousePort* WarehouseMachine = Cast<AWarehousePort>(Hit.GetActor());
	if (!WarehouseMachine) return;

	// 3. 창고 포트 확인 완료 시 인벤토리 오픈
	if (!InventoryWidgetInstance && InventoryWidgetClass)
	{
		InventoryWidgetInstance = CreateWidget<UUI_Inventory>(PC, InventoryWidgetClass);
	}

	if (InventoryWidgetInstance)
	{
		InventoryWidgetInstance->RefreshInventoryWindow();
		InventoryWidgetInstance->AddToViewport();
		bIsInventoryOpen = true;
		
		PC->SetInputMode(FInputModeGameAndUI());
		PC->SetShowMouseCursor(true);

		GetWorldTimerManager().SetTimer(
			InventoryRefreshTimerHandle, 
			this, 
			&AOJJ_Player::UpdateInventoryRealtime, 
			0.1f, 
			true
		);
	}
}

void AOJJ_Player::UpdateInventoryRealtime()
{
	if (bIsInventoryOpen && InventoryWidgetInstance)
	{
		InventoryWidgetInstance->RefreshInventoryWindow();
	}
	else
	{
		GetWorldTimerManager().ClearTimer(InventoryRefreshTimerHandle);
	}
}