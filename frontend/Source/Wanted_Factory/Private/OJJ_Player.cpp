// Fill out your copyright notice in the Description page of Project Settings.


#include "OJJ_Player.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputActionValue.h"

AOJJ_Player::AOJJ_Player()
{
	PrimaryActorTick.bCanEverTick = false;

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
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
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
