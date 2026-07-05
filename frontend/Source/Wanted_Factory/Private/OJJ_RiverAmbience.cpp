#include "OJJ_RiverAmbience.h"

#include "Components/AudioComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "WaterBodyRiverActor.h"
#include "WaterSplineComponent.h"

AOJJ_RiverAmbience::AOJJ_RiverAmbience()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("Audio"));
	AudioComponent->SetupAttachment(Root);
	// 상시 재생 — 루핑은 SoundCue의 Looping 설정에, 들리는 범위는 Attenuation에 위임.
	AudioComponent->bAutoActivate = true;
}

void AOJJ_RiverAmbience::BeginPlay()
{
	Super::BeginPlay();

	if (!TargetRiver)
	{
		for (TActorIterator<AWaterBodyRiver> It(GetWorld()); It; ++It)
		{
			TargetRiver = *It;
			break;
		}
	}
	if (!TargetRiver)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[RiverAmbience] WaterBodyRiver 미발견 — 위치 추종 비활성(배치 위치에서 고정 재생)."));
		return;
	}

	UpdateAudioLocation();
	GetWorldTimerManager().SetTimer(
		UpdateTimerHandle, this, &AOJJ_RiverAmbience::UpdateAudioLocation, UpdateInterval, true);
}

void AOJJ_RiverAmbience::UpdateAudioLocation()
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const UWaterSplineComponent* Spline = TargetRiver ? TargetRiver->GetWaterSpline() : nullptr;
	if (!Player || !Spline || !AudioComponent)
	{
		return;
	}

	const FVector Closest = Spline->FindLocationClosestToWorldLocation(
		Player->GetActorLocation(), ESplineCoordinateSpace::World);
	AudioComponent->SetWorldLocation(Closest);
}
