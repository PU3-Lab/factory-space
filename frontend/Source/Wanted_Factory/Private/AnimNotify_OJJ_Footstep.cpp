#include "AnimNotify_OJJ_Footstep.h"

#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "OJJ_FootstepStatics.h"
#include "Sound/SoundBase.h"

void UAnimNotify_OJJ_Footstep::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}
	UWorld* World = MeshComp->GetWorld();
	if (!World)
	{
		return;
	}

	// 캐릭터 메시 컴포넌트는 캡슐 바닥에 오프셋돼 있어 컴포넌트 위치 ≈ 발 위치.
	const FVector FootLocation = MeshComp->GetComponentLocation();

	// 표면 강제 지정(사다리 등반 등) — 그리드 질의 생략, 해당 사운드 즉시 재생.
	if (SurfaceOverride != EOJJFootstepSurface::Auto)
	{
		USoundBase* OverrideSound =
			(SurfaceOverride == EOJJFootstepSurface::Metal) ? MetalSound.Get() : SandSound.Get();
		if (OverrideSound)
		{
			UGameplayStatics::PlaySoundAtLocation(World, OverrideSound, FootLocation, VolumeMultiplier);
		}
		return;
	}

	// 표면 판별+재생 — 공용 유틸(OJJ_FootstepStatics, 착지 Landed와 공유). 우선순위: ①물 ②Foundation ③모래.
	// 수영 수심이면 발소리 노티파이 자체가 안 돈다(헤더 참조).
	OJJ_FootstepStatics::PlaySurfaceFootstep(
		World, FootLocation, SandSound.Get(), MetalSound.Get(), WetSound.Get(), VolumeMultiplier);
}
