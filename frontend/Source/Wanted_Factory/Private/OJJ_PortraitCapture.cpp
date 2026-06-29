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
#if WITH_EDITOR
#include "ShaderCompiler.h"   // GShaderCompilingManager — 에디터 첫 로드 셰이더 컴파일 완료 대기용(패키징 무관)
#endif
#include "Engine/Texture2D.h"            // 로봇 텍스처 mip resident 폴링 + 강제 resident
#include "Materials/MaterialInterface.h" // GetUsedTextures

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

	// 로봇 텍스처를 수집하고 전체 밉을 강제 resident로 요청(스트리밍 우선순위). 격리 배치(Z=-5000)라
	// 화면 커버리지 기반 스트리밍이 저밉에서 멈출 수 있어, 강제 force가 필요하다.
	CacheAndForceRobotTextures();

	// --- 캡처 워밍업: 셰이더 컴파일 + 텍스처 스트림인 완료 대기 ---
	// 첫 PIE는 ① 셰이더 컴파일 미완(에디터, 디폴트 머티리얼로 깨짐) ② 텍스처 mip 스트림인 미완(공통, 저해상도)
	// 으로 깨진 첫 프레임이 캡처될 수 있다. CaptureWarmupDelay 뒤부터 둘 다 끝났는지 폴링해(TryBeginCapture)
	// 완료 시 캡처를 켠다 — 고정 지연이 아니라 실제 완료를 기다려 머신/DDC/스트리밍 상태에 강건.
	if (Capture)
	{
		CaptureWarmupElapsed = 0.f;
		if (CaptureWarmupDelay > 0.f)
		{
			GetWorldTimerManager().SetTimer(
				CaptureWarmupTimer, this, &AOJJ_PortraitCapture::TryBeginCapture, CaptureWarmupDelay, /*bLoop=*/false);
		}
		else
		{
			TryBeginCapture();
		}
	}
}

void AOJJ_PortraitCapture::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 워밍업 폴링 중 파괴되면 보류 중인 타이머를 정리(콜백이 파괴된 액터에 도달하는 것 방지).
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CaptureWarmupTimer);
	}

	// 강제 force한 텍스처의 bForceMiplevelsToBeResident를 원래 값으로 복원 — 액터(Subsystem이 Destroy)가
	// 사라진 뒤 공유 텍스처에 강제 풀밉이 영구히 남지 않게 한다. blind false가 아니라 원본 복원(타 시스템 force 보존).
	for (int32 i = 0; i < RobotTextures.Num(); ++i)
	{
		if (UTexture2D* Tex2D = RobotTextures[i])
		{
			Tex2D->bForceMiplevelsToBeResident =
				RobotTexturesPrevForceResident.IsValidIndex(i) ? RobotTexturesPrevForceResident[i] : false;
		}
	}
	RobotTextures.Reset();
	RobotTexturesPrevForceResident.Reset();

	Super::EndPlay(EndPlayReason);
}

void AOJJ_PortraitCapture::TryBeginCapture()
{
	// 캡처 시작 전 두 가지를 기다린다(둘 중 하나라도 미완이면 폴링 계속):
	//  ① (에디터) 셰이더 컴파일 완료 — 미완이면 로봇이 디폴트 머티리얼로 깨져 캡처됨. 패키징은 쿡되어 무관.
	//  ② (에디터+패키징) 로봇 텍스처 mip full-resident — 미완이면 저해상도로 캡처됨. 스트리밍은 런타임 공통.
	bool bWaiting = false;

#if WITH_EDITOR
	if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling())
	{
		bWaiting = true;
	}
#endif

	if (!AreRobotTexturesFullyResident())
	{
		bWaiting = true;
	}

	// 미완이고 최대 대기 안 넘었으면 폴링 간격으로 재확인.
	if (bWaiting && CaptureWarmupElapsed < CaptureMaxWarmupWait)
	{
		CaptureWarmupElapsed += CaptureWarmupPollInterval;
		GetWorldTimerManager().SetTimer(
			CaptureWarmupTimer, this, &AOJJ_PortraitCapture::TryBeginCapture, CaptureWarmupPollInterval, /*bLoop=*/false);
		return;
	}

	if (bWaiting)   // 최대대기 초과로 빠져나온 경우(안전망 — 무한 대기 방지)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[OJJ_PortraitCapture] 워밍업 대기 %.1fs 초과 — 미완 상태로 캡처 시작(셰이더/텍스처 미완 가능)."),
			CaptureMaxWarmupWait);
	}

	BeginContinuousCapture();
}

void AOJJ_PortraitCapture::CacheAndForceRobotTextures()
{
	RobotTextures.Reset();
	RobotTexturesPrevForceResident.Reset();
	if (!RobotMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OJJ_PortraitCapture] RobotMesh 없음 — 텍스처 force/대기 스킵(저해상도 첫 캡처 가능)."));
		return;
	}

	// 로봇 머티리얼이 쓰는 모든 텍스처를 모아 캐시하고, 전체 밉을 강제 resident로 표시한다.
	// 포트레이트는 격리 배치(Z=-5000)라 화면 커버리지 기반 스트리밍이 저밉에서 멈출 수 있어, 강제 force가 필요.
	// 원래 플래그 값을 함께 저장해 EndPlay에서 원복한다(공유 텍스처에 강제 풀밉이 영구히 남지 않게).
	const TArray<UMaterialInterface*> Mats = RobotMesh->GetMaterials();
	for (UMaterialInterface* Mat : Mats)
	{
		if (!Mat)
		{
			continue;
		}
		TArray<UTexture*> UsedTextures;
		Mat->GetUsedTextures(UsedTextures);
		for (UTexture* Tex : UsedTextures)
		{
			if (UTexture2D* Tex2D = Cast<UTexture2D>(Tex))
			{
				if (RobotTextures.Contains(Tex2D))   // 중복 제외(원본값 배열과 인덱스 정렬 유지)
				{
					continue;
				}
				RobotTextures.Add(Tex2D);
				RobotTexturesPrevForceResident.Add(Tex2D->bForceMiplevelsToBeResident);  // 원래 값 저장
				Tex2D->bForceMiplevelsToBeResident = true;  // 렌더 여부와 무관하게 전체 밉 스트림인
			}
		}
	}

	if (RobotTextures.Num() == 0)
	{
		// "텍스처 없음(의도)"과 "수집 실패"를 구분하기 위한 경고 — 비면 대기를 건너뛰어 저해상도 첫 캡처 재발 가능.
		UE_LOG(LogTemp, Warning,
			TEXT("[OJJ_PortraitCapture] 로봇 머티리얼 텍스처 0개 수집 — 텍스처 대기 스킵. 머티리얼/텍스처 할당을 확인하세요."));
	}
}

bool AOJJ_PortraitCapture::AreRobotTexturesFullyResident() const
{
	// 캐시가 비어 있으면(텍스처 없음/수집 실패) 대기하지 않는다.
	for (const UTexture2D* Tex2D : RobotTextures)
	{
		if (Tex2D && Tex2D->GetNumResidentMips() < Tex2D->GetNumMips())
		{
			return false;
		}
	}
	return true;
}

void AOJJ_PortraitCapture::BeginContinuousCapture()
{
	if (!Capture)
	{
		return;
	}

	// 워밍업 끝(셰이더 컴파일 + 텍스처 스트림인 완료) — 연속 캡처를 켜고 즉시 한 장 갱신한다.
	Capture->bCaptureEveryFrame = true;
	Capture->CaptureScene();
}
