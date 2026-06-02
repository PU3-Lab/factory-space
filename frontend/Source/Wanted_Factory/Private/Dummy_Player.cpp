// Fill out your copyright notice in the Description page of Project Settings.

#include "Dummy_Player.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Dummy_BuildController.h"
#include "Dummy_Grid.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "OJJ_BuildCamera.h"

ADummyPlayer::ADummyPlayer()
{
	PrimaryActorTick.bCanEverTick = false;

	BuildCameraClass = AOJJ_BuildCamera::StaticClass();

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 400.f;
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 10.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 540.f, 0.f);
	Movement->MaxWalkSpeed = 600.f;
	Movement->JumpZVelocity = 500.f;
	Movement->AirControl = 0.35f;
}

void ADummyPlayer::BeginPlay()
{
	Super::BeginPlay();

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

		if (APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
		{
			CameraManager->ViewPitchMin = CameraPitchMin;
			CameraManager->ViewPitchMax = CameraPitchMax;
		}
	}

	BuildController = Cast<ADummyBuildController>(
		UGameplayStatics::GetActorOfClass(GetWorld(), ADummyBuildController::StaticClass()));
	if (!BuildController)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[DummyPlayer] ADummyBuildController instance was not found in the level. ")
			TEXT("Place ADummyBuildController and set TargetGrid/MachineClass/ConveyorClass."));
	}

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
		UE_LOG(LogTemp, Warning, TEXT("[DummyPlayer] AOJJ_BuildCamera spawn failed"));
	}
}

void ADummyPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogTemp, Error, TEXT("[DummyPlayer] EnhancedInputComponent cast failed"));
		return;
	}

	if (IA_Move)
	{
		EnhancedInput->BindAction(IA_Move, ETriggerEvent::Triggered, this, &ADummyPlayer::Move);
	}
	if (IA_Look)
	{
		EnhancedInput->BindAction(IA_Look, ETriggerEvent::Triggered, this, &ADummyPlayer::Look);
	}
	if (IA_Zoom)
	{
		EnhancedInput->BindAction(IA_Zoom, ETriggerEvent::Triggered, this, &ADummyPlayer::Zoom);
	}
	if (IA_Jump)
	{
		EnhancedInput->BindAction(IA_Jump, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInput->BindAction(IA_Jump, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}
	if (IA_Build)
	{
		EnhancedInput->BindAction(IA_Build, ETriggerEvent::Started, this, &ADummyPlayer::ToggleBuild);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[DummyPlayer] IA_Build is not assigned"));
	}
	if (IA_BuildPlace)
	{
		EnhancedInput->BindAction(IA_BuildPlace, ETriggerEvent::Started, this, &ADummyPlayer::BuildPlace);
		EnhancedInput->BindAction(IA_BuildPlace, ETriggerEvent::Completed, this, &ADummyPlayer::BuildPlaceReleased);
		EnhancedInput->BindAction(IA_BuildPlace, ETriggerEvent::Canceled, this, &ADummyPlayer::BuildPlaceCanceled);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[DummyPlayer] IA_BuildPlace is not assigned"));
	}
	if (IA_BuildPan)
	{
		EnhancedInput->BindAction(IA_BuildPan, ETriggerEvent::Triggered, this, &ADummyPlayer::BuildPan);
	}
	if (IA_BuildRotate)
	{
		EnhancedInput->BindAction(IA_BuildRotate, ETriggerEvent::Triggered, this, &ADummyPlayer::BuildRotate);
	}
	if (IA_MachineRotate)
	{
		EnhancedInput->BindAction(IA_MachineRotate, ETriggerEvent::Started, this, &ADummyPlayer::BuildRotateMachine);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[DummyPlayer] IA_MachineRotate is not assigned"));
	}
	if (IA_SetMachineMode)
	{
		EnhancedInput->BindAction(IA_SetMachineMode, ETriggerEvent::Started, this, &ADummyPlayer::SetMachineMode);
	}
	if (IA_SetConveyorMode)
	{
		EnhancedInput->BindAction(IA_SetConveyorMode, ETriggerEvent::Started, this, &ADummyPlayer::SetConveyorMode);
	}
}

void ADummyPlayer::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (!Controller || Axis.IsNearlyZero())
	{
		return;
	}

	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, Axis.Y);
	AddMovementInput(Right, Axis.X);
}

void ADummyPlayer::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X * LookYawSensitivity);
	AddControllerPitchInput(Axis.Y * LookPitchSensitivity);
}

void ADummyPlayer::Zoom(const FInputActionValue& Value)
{
	const float Scroll = Value.Get<float>();
	if (!SpringArm || FMath::IsNearlyZero(Scroll))
	{
		return;
	}

	const float NewLength = SpringArm->TargetArmLength - Scroll * ZoomStep;
	SpringArm->TargetArmLength = FMath::Clamp(NewLength, MinArmLength, MaxArmLength);
}

void ADummyPlayer::ToggleBuild(const FInputActionValue& Value)
{
	if (!BuildController)
	{
		return;
	}

	BuildController->ToggleBuildMode();
	ApplyBuildModeView(BuildController->IsInBuildMode());
}

void ADummyPlayer::ApplyBuildModeView(bool bEntering)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	UEnhancedInputLocalPlayerSubsystem* Subsystem = PC
		? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer())
		: nullptr;

	if (bEntering)
	{
		if (BuildCamera && BuildController)
		{
			if (const ADummyGrid* Grid = BuildController->GetTargetGrid())
			{
				BuildCamera->SetActorLocation(Grid->GetGridCenter());
			}
		}
		if (PC && BuildCamera)
		{
			PC->SetViewTargetWithBlend(BuildCamera, CameraBlendTime);
		}
		SetActorHiddenInGame(true);

		if (Subsystem && IMC_Build)
		{
			Subsystem->RemoveMappingContext(IMC_Player);
			Subsystem->AddMappingContext(IMC_Build, 0);
		}
		else if (!IMC_Build)
		{
			UE_LOG(LogTemp, Warning, TEXT("[DummyPlayer] IMC_Build is not assigned"));
		}
	}
	else
	{
		if (PC)
		{
			PC->SetViewTargetWithBlend(this, CameraBlendTime);
		}
		SetActorHiddenInGame(false);

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

void ADummyPlayer::BuildPlace(const FInputActionValue& Value)
{
	if (BuildController)
	{
		BuildController->OnLeftClickPressed();
	}
}

void ADummyPlayer::BuildPlaceReleased(const FInputActionValue& Value)
{
	if (BuildController)
	{
		BuildController->OnLeftClickReleased();
	}
}

void ADummyPlayer::BuildPlaceCanceled(const FInputActionValue& Value)
{
	if (BuildController)
	{
		BuildController->CancelConveyorDrag();
	}
}

void ADummyPlayer::BuildPan(const FInputActionValue& Value)
{
	if (BuildCamera)
	{
		BuildCamera->Pan(Value.Get<FVector2D>());
	}
}

void ADummyPlayer::BuildRotate(const FInputActionValue& Value)
{
	if (BuildCamera)
	{
		BuildCamera->Rotate(Value.Get<float>());
	}
}

void ADummyPlayer::BuildRotateMachine(const FInputActionValue& Value)
{
	if (BuildController)
	{
		BuildController->RotateHoverClockwise();
	}
}

void ADummyPlayer::SetMachineMode(const FInputActionValue& Value)
{
	if (BuildController)
	{
		BuildController->SetPlacementMode(EDummyBuildPlacementMode::Machine);
	}
}

void ADummyPlayer::SetConveyorMode(const FInputActionValue& Value)
{
	if (BuildController)
	{
		BuildController->SetPlacementMode(EDummyBuildPlacementMode::Conveyor);
	}
}
