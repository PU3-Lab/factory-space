// Fill out your copyright notice in the Description page of Project Settings.

#include "OJJ_PortraitCapture.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimationAsset.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"

AOJJ_PortraitCapture::AOJJ_PortraitCapture()
{
	// 캡처는 매 프레임 갱신(bCaptureEveryFrame)으로 처리하므로 액터 Tick은 불필요.
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// --- 로봇 메시 ---
	RobotMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("RobotMesh"));
	RobotMesh->SetupAttachment(SceneRoot);
	RobotMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// 단일노드 애니로 idle만 재생(AnimBP 불필요).
	RobotMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	// 카메라는 +X에서 -X를 바라본다(아래 SpringArm). 로봇 정면이 카메라를 향하도록 Yaw 회전.
	// 정면(-90)에서 반대쪽으로 18° 틀어 3/4 측면 앵글(로봇이 화면 오른쪽을 향함).
	// Meshy 임포트 메시의 정면 축이 케이스마다 달라 측면으로 보일 수 있다 — 측면이면 ±90, 후면이면 180 추가.
	RobotMesh->SetRelativeRotation(FRotator(0.f, -108.f, 0.f));

	// 로봇 SkeletalMesh 기본 로드.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> RobotMeshFinder(
		TEXT("/Game/OJJ/Character/Robot/Meshy_AI_Character_output__2_.Meshy_AI_Character_output__2_"));
	if (RobotMeshFinder.Succeeded())
	{
		RobotMesh->SetSkeletalMeshAsset(RobotMeshFinder.Object);
	}

	// idle 애니메이션 기본 로드(에디터에서 교체 가능).
	static ConstructorHelpers::FObjectFinder<UAnimationAsset> IdleAnimFinder(
		TEXT("/Game/OJJ/Character/Robot/Meshy_AI_Lumen_Sentinel_biped_Animation_Idle_11_without_skin.Meshy_AI_Lumen_Sentinel_biped_Animation_Idle_11_without_skin"));
	if (IdleAnimFinder.Succeeded())
	{
		IdleAnimation = IdleAnimFinder.Object;
	}

	// RenderTarget 기본 로드 — 자동 스폰(서브시스템) 시에도 RT가 연결되도록.
	// 에디터 배치 시 PortraitRenderTarget을 다른 RT로 덮어쓸 수 있다.
	static ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> RTFinder(
		TEXT("/Game/OJJ/Character/Robot/RT_RobotPortrait.RT_RobotPortrait"));
	if (RTFinder.Succeeded())
	{
		PortraitRenderTarget = RTFinder.Object;
	}

	// --- 키 라이트 ---
	// 카메라(+X)와 같은 정면 위쪽에서 강하게 비춰 로봇 정면을 밝힌다. ShowOnlyActors는 메시
	// 프리미티브만 제한하고 라이팅은 씬 전체 라이트가 적용되므로, 같은 액터에 둔 라이트로 로봇이 밝아진다.
	KeyLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(SceneRoot);
	KeyLight->SetRelativeLocation(FVector(160.f, 50.f, 200.f));
	KeyLight->SetIntensity(50000.f);
	KeyLight->SetAttenuationRadius(1200.f);
	KeyLight->SetCastShadows(false);

	// --- 필 라이트 ---
	// 키 라이트 반대편(-Y 측) 아래쪽에서 비춰 그림자로 묻히는 면을 살린다.
	FillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(SceneRoot);
	FillLight->SetRelativeLocation(FVector(120.f, -130.f, 120.f));
	FillLight->SetIntensity(15000.f);
	FillLight->SetAttenuationRadius(1200.f);
	FillLight->SetCastShadows(false);

	// --- 스프링암(카메라 거리/각도) ---
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(SceneRoot);
	// 피벗 높이/거리는 BeginPlay에서 메시 바운드 기반으로 자동 설정(아래 값은 에디터 프리뷰용 fallback).
	SpringArm->SetRelativeLocation(FVector(0.f, 0.f, 130.f));
	// 카메라가 로봇 정면(+X)에서 -X 방향을 바라보도록 암을 180° 돌리고 거의 수평으로 본다.
	SpringArm->SetRelativeRotation(FRotator(-2.f, 180.f, 0.f));
	SpringArm->TargetArmLength = 100.f;
	SpringArm->bDoCollisionTest = false;        // 벽 충돌로 줌인되는 것 방지
	SpringArm->bEnableCameraLag = false;
	SpringArm->bUsePawnControlRotation = false;

	// --- 씬 캡처 ---
	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	Capture->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	// 캡처는 BeginPlay 워밍업 지연 후 켠다(BeginContinuousCapture) — 첫 프레임 셰이더 컴파일/스트림인 글리치 회피.
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	// 투명 배경: SCS_SceneColorHDR은 알파에 "역불투명도"(빈 배경=1, 로봇=0)를 담는다 → 아래 AlphaInvert로
	// 뒤집어 로봇=불투명/배경=투명으로 만든다. 전역 r.PostProcessing.PropagateAlpha 없이 동작(전역 영향 없음).
	Capture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
	// 지정한 액터(self)만 렌더 — 배경/타 메시 제외.
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->CompositeMode = ESceneCaptureCompositeMode::SCCM_Overwrite;
	// 렌더된 불투명 픽셀이 없는 곳을 완전 투명으로 — 안개/환경 잔여 억제.
	Capture->bConsiderUnrenderedOpaquePixelAsFullyTranslucent = true;
	Capture->bAlwaysPersistRenderingState = true;
	Capture->PostProcessBlendWeight = 0.f;       // 월드 포스트프로세스 격리
	// 포트레이트용 화각(원근 왜곡 줄임). 자동 프레이밍이 이 FOV로 카메라 거리를 계산한다.
	Capture->FOVAngle = 55.f;

	// 빈 배경이 회색(스카이/대기/안개 환경광)으로 오염되는 것을 차단. 로봇은 KeyLight/FillLight로만 조명한다.
	// 알파 반전(SceneColorHDR의 역불투명도 → 로봇=불투명)은 UI 머티리얼 M_Portrait_UI(1-Alpha)에서
	// 한 번만 처리한다. ShowFlags.AlphaInvert는 SceneColorHDR 경로에 적용되지 않아(무동작) 여기서 켜지 않는다
	// — 켜두면 향후 엔진/설정 변화로 적용될 때 UI 측과 이중 반전될 위험만 남는다.
	FEngineShowFlags& SF = Capture->ShowFlags;
	SF.SetAtmosphere(false);
	SF.SetFog(false);
	SF.SetVolumetricFog(false);
	SF.SetCloud(false);
	SF.SetSkyLighting(false);                // 배경 환경광 기여 차단
	SF.SetAmbientOcclusion(false);
	SF.SetScreenSpaceReflections(false);
	SF.SetReflectionEnvironment(false);
	SF.SetBloom(false);
	SF.SetMotionBlur(false);
	SF.SetDepthOfField(false);
	SF.SetTemporalAA(false);
	SF.SetVignette(false);
}

void AOJJ_PortraitCapture::BeginPlay()
{
	Super::BeginPlay();

	// 캡처 대상은 이 액터(로봇 메시)뿐.
	if (Capture)
	{
		Capture->ShowOnlyActors.Empty();
		Capture->ShowOnlyActors.Add(this);

		// 에디터에서 할당한 RenderTarget을 캡처 출력으로 연결.
		if (PortraitRenderTarget)
		{
			// 배경 투명을 위해 클리어 색 알파를 0으로(런타임 보장).
			PortraitRenderTarget->ClearColor = FLinearColor(0.f, 0.f, 0.f, 0.f);
			Capture->TextureTarget = PortraitRenderTarget;
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[OJJ_PortraitCapture] PortraitRenderTarget이 비어 있음 — 에디터에서 RT_RobotPortrait를 할당하세요."));
		}
	}

	// --- 카메라 자동 프레이밍 ---
	// 메시의 실제 높이(로컬 바운드 Z — Yaw 회전에 불변)를 읽어, 크기와 무관하게 상반신이
	// 칸 중앙에 적당히 차도록 SpringArm 피벗 높이와 거리를 계산한다.
	if (RobotMesh && RobotMesh->GetSkeletalMeshAsset() && SpringArm && Capture)
	{
		const FBoxSphereBounds LB = RobotMesh->GetSkeletalMeshAsset()->GetBounds();
		const float ExtentZ = LB.BoxExtent.Z;
		const float TopZ = LB.Origin.Z + ExtentZ;                 // 머리 꼭대기(메시 로컬 = 액터 로컬, 메시는 루트 상대 0)
		const float FullHeight = ExtentZ * 2.f;
		const float FrameHeight = FMath::Max(FullHeight * UpperBodyRatio, 1.f); // 위에서부터 상반신만
		const float FrameCenterZ = TopZ - FrameHeight * 0.5f;     // 상반신 구간의 중심

		// 피벗을 상반신 중심 높이로 → 그 지점이 화면 중앙.
		SpringArm->SetRelativeLocation(FVector(0.f, 0.f, FrameCenterZ));

		// 프레임 높이가 세로 화각을 (여백 포함) 채우는 카메라 거리. (정사각 RT라 가로=세로 FOV)
		const float HalfFrame = FrameHeight * 0.5f * FramePadding;
		const float HalfFovRad = FMath::DegreesToRadians(Capture->FOVAngle * 0.5f);
		const float Dist = HalfFrame / FMath::Max(FMath::Tan(HalfFovRad), 0.01f);
		SpringArm->TargetArmLength = Dist;

		UE_LOG(LogTemp, Log,
			TEXT("[OJJ_PortraitCapture] 자동프레이밍 — 메시높이 %.1f, 피벗Z %.1f, 거리 %.1f"),
			FullHeight, FrameCenterZ, Dist);
	}
	else if (!RobotMesh || !RobotMesh->GetSkeletalMeshAsset())
	{
		// 메시 미할당 시 자동 프레이밍 스킵 → 생성자 fallback 값(피벗/거리)으로 캡처된다.
		UE_LOG(LogTemp, Warning,
			TEXT("[OJJ_PortraitCapture] RobotMesh/SkeletalMesh 없음 — 자동 프레이밍 스킵(fallback 카메라값 사용). 메시 경로를 확인하세요."));
	}

	// idle 애니 루프 재생.
	if (RobotMesh && IdleAnimation)
	{
		RobotMesh->PlayAnimation(IdleAnimation, /*bLooping=*/true);
	}
	else if (!IdleAnimation)
	{
		// 애니 미할당 시 정지 포즈로 퇴화 — 원인 파악용 경고.
		UE_LOG(LogTemp, Warning,
			TEXT("[OJJ_PortraitCapture] IdleAnimation 없음 — 정지 포즈로 표시됨. 애니 경로/할당을 확인하세요."));
	}

	// --- 캡처 워밍업 지연 ---
	// BeginPlay 직후 몇 프레임은 M_Robot 셰이더 첫 컴파일/텍스처 스트림인으로 깨진 채 렌더된다(에디터 첫 PIE).
	// 매 프레임 캡처가 그 깨진 프레임을 RT에 찍지 않도록, CaptureWarmupDelay 뒤에 연속 캡처를 켠다(0이면 즉시).
	if (Capture)
	{
		if (CaptureWarmupDelay > 0.f)
		{
			GetWorldTimerManager().SetTimer(
				CaptureWarmupTimer, this, &AOJJ_PortraitCapture::BeginContinuousCapture, CaptureWarmupDelay, /*bLoop=*/false);
		}
		else
		{
			BeginContinuousCapture();
		}
	}
}

void AOJJ_PortraitCapture::BeginContinuousCapture()
{
	if (!Capture)
	{
		return;
	}

	// 워밍업 끝 — 이제 깨지지 않은 프레임이 나오므로 연속 캡처를 켜고 즉시 한 장 갱신한다.
	Capture->bCaptureEveryFrame = true;
	Capture->CaptureScene();
}
