// Fill out your copyright notice in the Description page of Project Settings.


#include "OJJ_BuildCamera.h"

#include "Camera/CameraComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/KismetSystemLibrary.h"

AOJJ_BuildCamera::AOJJ_BuildCamera()
{
	// 입력 위임(Pan/Rotate)은 DeltaSeconds를 직접 받아 적용하지만, [높은 메시 위 자동 상승]을 위한
	// Root Z 보간이 매 틱 필요하므로 Tick을 켠다.
	PrimaryActorTick.bCanEverTick = true;

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

	// [빌드 작업등] 카메라에 부착 → 카메라 forward(하향, SpringArm pitch -90)를 그대로 따라 아래를 비춘다.
	// 상대 회전 0이면 스폿 forward = 카메라 시선 = 빌드 영역. 줌(ArmLength)에 따라 높이가 변해도 자동 정합.
	// 밤 분위기 유지를 위해 NightSpotLight와 동일하게 비물리 falloff로 은은하게.
	WorkLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("WorkLight"));
	WorkLight->SetupAttachment(Camera);
	WorkLight->SetRelativeRotation(FRotator::ZeroRotator);
	WorkLight->Intensity = WorkLightIntensity;
	WorkLight->AttenuationRadius = WorkLightAttenuationRadius;
	WorkLight->InnerConeAngle = WorkLightInnerCone;
	WorkLight->OuterConeAngle = WorkLightOuterCone;
	WorkLight->bUseInverseSquaredFalloff = false;
	WorkLight->LightFalloffExponent = 2.5f;
	WorkLight->SetVisibility(false); // 기본 off — L키 토글로만 켠다.
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

	// 작업등 튜닝값도 인스턴스 값으로 재반영(에디터에서 바뀌었을 수 있음). 가시성은 건드리지 않음(기본 off 유지).
	if (WorkLight)
	{
		WorkLight->SetIntensity(WorkLightIntensity);
		WorkLight->SetAttenuationRadius(WorkLightAttenuationRadius);
		WorkLight->SetInnerConeAngle(WorkLightInnerCone);
		WorkLight->SetOuterConeAngle(WorkLightOuterCone);
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

	// [높은 메시 위 자동 상승] XY가 바뀌면 보는 영역도 바뀜 → 다음 throttle 간격에 오버랩 재계산.
	bViewChanged = true;
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

	// [높은 메시 위 자동 상승] 줌이 바뀌면 보는 영역 반경(= ArmLength * ViewExtentFactor)도 바뀜 → 재계산.
	bViewChanged = true;
}

void AOJJ_BuildCamera::SetWorkLightEnabled(bool bEnabled)
{
	if (WorkLight && WorkLight->IsVisible() != bEnabled)
	{
		WorkLight->SetVisibility(bEnabled);
	}
}

void AOJJ_BuildCamera::BecomeViewTarget(APlayerController* PC)
{
	Super::BecomeViewTarget(PC);

	// 진입 직전 플레이어가 SetActorLocation(XY=플레이어, Z=그리드 평면)을 세팅한 직후 호출된다.
	// 그 Z를 기준 지면 높이로 캡처한다(영역에 키 큰 메시가 없으면 이 높이로 복귀). 진입 전환 코드는 건드리지 않음.
	BaselineGroundZ = GetActorLocation().Z;
	TargetRootZ = BaselineGroundZ;
	bIsActiveBuildView = true;
	bViewChanged = true;
	FramesSinceOverlap = OverlapThrottleFrames; // 다음 틱에 즉시 1회 오버랩 계산
}

void AOJJ_BuildCamera::EndViewTarget(APlayerController* PC)
{
	// 빌드모드 이탈/TPS 전환 — Z 자동 조정 중단(불필요한 오버랩/보간 차단).
	bIsActiveBuildView = false;
	Super::EndViewTarget(PC);
}

void AOJJ_BuildCamera::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 빌드 뷰타겟일 때만 동작. 그 외엔 무동작(일반 플레이/TPS 빌드에서 부하 0).
	if (!bIsActiveBuildView)
	{
		return;
	}

	// 오버랩 재계산은 throttle: 보는 영역이 바뀌었고(bViewChanged) 간격이 찼을 때만.
	++FramesSinceOverlap;
	if (bViewChanged && FramesSinceOverlap >= OverlapThrottleFrames)
	{
		RecomputeTargetRootZ();
		FramesSinceOverlap = 0;
		bViewChanged = false;
	}

	// 보간은 매 틱: 올라갈 땐 빠르게(Rise), 내려올 땐 천천히(Fall) — 경계 출렁임 감소.
	const FVector Loc = GetActorLocation();
	const float Speed = (TargetRootZ > Loc.Z) ? RiseInterpSpeed : FallInterpSpeed;
	const float NewZ = FMath::FInterpTo(Loc.Z, TargetRootZ, DeltaSeconds, Speed);
	if (!FMath::IsNearlyEqual(NewZ, Loc.Z, 0.01f))
	{
		// XY는 Pan이 갱신한 현재값 유지, Z만 보간값으로 덮어쓴다.
		SetActorLocation(FVector(Loc.X, Loc.Y, NewZ));
	}
}

void AOJJ_BuildCamera::RecomputeTargetRootZ()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 보는 영역 반경 = 현재 줌(ArmLength) × 비율. 정탑다운이라 XY 중심 ± HalfExtentXY가 화면 영역.
	const float CurrentArm = SpringArm ? SpringArm->TargetArmLength : ArmLength;
	const float HalfExtentXY = FMath::Max(1.f, CurrentArm * ViewExtentFactor);

	// 박스: XY는 영역 한정, Z는 넉넉히(±OverlapBoxHalfHeightZ) 잡아 어떤 높이의 메시도 포함. 중심 Z는 기준 지면.
	const FVector ActorLoc = GetActorLocation();
	const FVector BoxPos(ActorLoc.X, ActorLoc.Y, BaselineGroundZ);
	const FVector BoxExtent(HalfExtentXY, HalfExtentXY, OverlapBoxHalfHeightZ);

	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));

	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(this); // 카메라 자신(및 부착 WorkLight)은 제외

	TArray<AActor*> Overlapped;
	UKismetSystemLibrary::BoxOverlapActors(World, BoxPos, BoxExtent, ObjectTypes, nullptr, IgnoreActors, Overlapped);

	// 겹친 액터들의 메시 바운드 상단 Z 최댓값. 단, 거대 배경(바운드 높이 > MaxConsiderMeshHeight)은 제외.
	float MaxMeshTopZ = TNumericLimits<float>::Lowest();
	int32 NumConsidered = 0;
	for (const AActor* A : Overlapped)
	{
		if (!A)
		{
			continue;
		}
		FVector Origin, Extent;
		A->GetActorBounds(/*bOnlyCollidingComponents=*/false, Origin, Extent);

		// 액터 바운드 높이가 상한을 넘으면 산/벽 같은 거대 배경으로 간주 → 후보에서 스킵(배치 위치 무관, 크기 기준).
		const float MeshHeight = Extent.Z * 2.f;
		if (MeshHeight > MaxConsiderMeshHeight)
		{
			continue;
		}

		MaxMeshTopZ = FMath::Max(MaxMeshTopZ, Origin.Z + Extent.Z);
		++NumConsidered;
	}

	// 통과한 메시가 있으면 그 최고점 + 여유로, 없으면(빈 곳/배경만) 기준 지면으로 복귀.
	float NewTarget = BaselineGroundZ;
	if (NumConsidered > 0)
	{
		NewTarget = FMath::Max(BaselineGroundZ, MaxMeshTopZ + MeshClearance);
	}
	TargetRootZ = NewTarget;
}
