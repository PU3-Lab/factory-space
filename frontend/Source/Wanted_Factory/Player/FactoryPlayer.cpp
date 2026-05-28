// Fill out your copyright notice in the Description page of Project Settings.


#include "FactoryPlayer.h"

#include "GameFramework/CharacterMovementComponent.h"


// Sets default values
AFactoryPlayer::AFactoryPlayer()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm-> SetupAttachment(RootComponent);
	SpringArm->TargetArmLength(900.f);
	SpringArm->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	SpringArm->bUsePawnControlRotation = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->MaxWalkSpeed = 600.f;
}

// Called when the game starts or when spawned
void AFactoryPlayer::BeginPlay()
{
	Super::BeginPlay();

}

// Called every frame
void AFactoryPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void AFactoryPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &AFactoryPlayer::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &AFactoryPlayer::MoveRight);
}

void AFactoryPlayer::MoveForward(float Value)
{
}

void AFactoryPlayer::MoveRight(float Value)
{
}
