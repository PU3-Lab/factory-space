// Fill out your copyright notice in the Description page of Project Settings.


#include "OJJ_BuildCamera.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

AOJJ_BuildCamera::AOJJ_BuildCamera()
{
	// 입력 위임(Pan/Rotate)이 DeltaSeconds를 직접 받아 적용하므로 자체 Tick 불필요.
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(Root);
	SpringArm->TargetArmLength = ArmLength;
	SpringArm->SetRelativeRotation(FRotator(CameraPitch, 0.f, 0.f)); // 아래를 내려다봄(상대 pitch)
	SpringArm->bDoCollisionTest = false;        // 지형/머신에 카메라가 끌려오지 않도록
	SpringArm->bUsePawnControlRotation = false; // possess 안 함 — 회전은 액터 yaw로 직접 제어
	// 액터 yaw를 상속해야 Q/E(AddActorWorldRotation yaw)로 카메라가 함께 회전(3c).
	// pitch/roll은 상속 끄고 상대 pitch(-70)만 유지 → 어떤 경우에도 하향 틸트 보존.
	SpringArm->bInheritYaw = true;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritRoll = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
}

void AOJJ_BuildCamera::BeginPlay()
{
	Super::BeginPlay();

	// 에디터에서 튜닝한 값을 런타임 컴포넌트에 반영 (생성자 값이 인스턴스에서 바뀌었을 수 있음)
	if (SpringArm)
	{
		SpringArm->TargetArmLength = ArmLength;
		SpringArm->SetRelativeRotation(FRotator(CameraPitch, 0.f, 0.f));
	}
}

void AOJJ_BuildCamera::Pan(const FVector2D& Axis)
{
	if (Axis.IsNearlyZero())
	{
		return;
	}

	const float Delta = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	if (Delta <= 0.f)
	{
		return;
	}

	// 카메라 yaw 기준 평면 이동: 액터 yaw로 전/우 방향을 산출(pitch/roll 제외 → 수평 패닝).
	const FRotator YawRotation(0.f, GetActorRotation().Yaw, 0.f);
	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	const FVector Move = (Forward * Axis.Y + Right * Axis.X) * PanSpeed * Delta;
	AddActorWorldOffset(Move);
}

void AOJJ_BuildCamera::Rotate(float Axis)
{
	if (FMath::IsNearlyZero(Axis))
	{
		return;
	}

	const float Delta = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	if (Delta <= 0.f)
	{
		return;
	}

	AddActorWorldRotation(FRotator(0.f, Axis * RotateSpeed * Delta, 0.f));
}

void AOJJ_BuildCamera::Zoom(float ScrollDelta)
{
	if (!SpringArm || FMath::IsNearlyZero(ScrollDelta))
	{
		return;
	}

	// 스크롤 업(+) → 줌인(팔 길이 감소). ArmLength를 현재값으로 갱신해 BeginPlay 재적용/재진입과 정합.
	ArmLength = FMath::Clamp(SpringArm->TargetArmLength - ScrollDelta * ZoomStep, MinArmLength, MaxArmLength);
	SpringArm->TargetArmLength = ArmLength;
}
