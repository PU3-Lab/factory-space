#include "OJJ_MinimapCapture.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "OJJ_Grid.h"
#include "TimerManager.h"

AOJJ_MinimapCapture::AOJJ_MinimapCapture()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	CaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	CaptureComponent->SetupAttachment(Root);
	// 탑다운 규약(헤더 참조): pitch -90, yaw 0 → +U=월드+Y, 이미지 위=월드+X(북).
	CaptureComponent->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));

	CaptureComponent->ProjectionType = ECameraProjectionMode::Orthographic;
	CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	// 요청식 캡처만 — 상시 캡처 비용 차단.
	CaptureComponent->bCaptureEveryFrame = false;
	CaptureComponent->bCaptureOnMovement = false;

	// 미니맵 가독성: 안개/대기/파티클은 지도를 덮는 노이즈라 끔.
	CaptureComponent->ShowFlags.SetFog(false);
	CaptureComponent->ShowFlags.SetVolumetricFog(false);
	CaptureComponent->ShowFlags.SetAtmosphere(false);
	CaptureComponent->ShowFlags.SetParticles(false);
}

void AOJJ_MinimapCapture::BeginPlay()
{
	Super::BeginPlay();

	if (RenderTarget)
	{
		CaptureComponent->TextureTarget = RenderTarget;
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[MinimapCapture] RenderTarget 미할당 — 미니맵 배경 캡처 비활성. RT 에셋(2048²)을 만들어 할당 필요."));
	}

	AlignToGrid();

	if (InitialCaptureDelay > 0.0f)
	{
		GetWorldTimerManager().SetTimer(
			InitialCaptureTimerHandle, this, &AOJJ_MinimapCapture::RequestCapture, InitialCaptureDelay, false);
	}
	else
	{
		RequestCapture();
	}

	if (RecaptureInterval > 0.0f)
	{
		GetWorldTimerManager().SetTimer(
			RecaptureTimerHandle, this, &AOJJ_MinimapCapture::RequestCapture, RecaptureInterval, true);
	}
}

void AOJJ_MinimapCapture::AlignToGrid()
{
	AOJJ_Grid* Grid = nullptr;
	for (TActorIterator<AOJJ_Grid> It(GetWorld()); It; ++It)
	{
		Grid = *It;
		break;
	}
	if (!Grid)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[MinimapCapture] AOJJ_Grid 미발견 — 배치 위치/OrthoWidth 그대로 사용(자동 정렬 스킵)."));
		return;
	}

	FVector2D BoundsMin, BoundsMax;
	Grid->GetGridWorldBounds(BoundsMin, BoundsMax);
	const FVector2D Extent = BoundsMax - BoundsMin;

	const FVector Center = Grid->GetGridCenter();
	SetActorLocation(FVector(Center.X, Center.Y, Center.Z + CaptureHeight));
	// 정사각 RT에 비정사각 그리드면 max 변 기준으로 여유 있게 덮는다(빈 여백은 지도 밖).
	CaptureComponent->OrthoWidth = FMath::Max(Extent.X, Extent.Y);
}

void AOJJ_MinimapCapture::RequestCapture()
{
	if (!CaptureComponent->TextureTarget)
	{
		return;
	}
	CaptureComponent->CaptureScene();
}

FVector2D AOJJ_MinimapCapture::GetCaptureWorldCenter() const
{
	const FVector Location = GetActorLocation();
	return FVector2D(Location.X, Location.Y);
}

float AOJJ_MinimapCapture::GetCaptureWorldSize() const
{
	return CaptureComponent->OrthoWidth;
}
