// Fill out your copyright notice in the Description page of Project Settings.


#include "OJJ_Player.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "OJJ_BuildController.h"
#include "OJJ_BuildCamera.h"
#include "OJJ_Grid.h"

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
	Movement->MaxWalkSpeed = 600.f;
	Movement->JumpZVelocity = 500.f;
	Movement->AirControl = 0.35f;
}

void AOJJ_Player::BeginPlay()
{
	Super::BeginPlay();

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
		EnhancedInput->BindAction(IA_BuildPlace, ETriggerEvent::Started, this, &AOJJ_Player::BuildPlace);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[OJJ_Player] IA_BuildPlace 미할당 — 배치(좌클릭) 비활성. BP_OJJ_Player에 IA_BuildPlace 에셋 할당 필요."));
	}
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
	AddControllerPitchInput(Axis.Y * LookPitchSensitivity);
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
