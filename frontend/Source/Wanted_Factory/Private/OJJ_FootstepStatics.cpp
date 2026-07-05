#include "OJJ_FootstepStatics.h"

#include "Kismet/GameplayStatics.h"
#include "OJJ_Grid.h"
#include "Sound/SoundBase.h"

void OJJ_FootstepStatics::PlaySurfaceFootstep(UWorld* World, const FVector& Location,
	USoundBase* SandSound, USoundBase* MetalSound, USoundBase* WetSound, float VolumeMultiplier)
{
	if (!World)
	{
		return;
	}

	// 표면 판별 — 그리드 질의(수영 감지의 그리드 접근 패턴 미러: GetActorOfClass, 실패 시 모래 폴백).
	bool bInWater = false;
	bool bOnFoundation = false;
	if (const AOJJ_Grid* Grid =
			Cast<AOJJ_Grid>(UGameplayStatics::GetActorOfClass(World, AOJJ_Grid::StaticClass())))
	{
		float WaterSurfaceZ = 0.0f;
		bInWater = Grid->OJJ_QueryWaterBodyAt(Location, WaterSurfaceZ);
		if (!bInWater)
		{
			bOnFoundation = Grid->IsCellOnFoundation(Grid->WorldToGrid(Location));
		}
	}

	USoundBase* Sound = bInWater ? WetSound : (bOnFoundation ? MetalSound : SandSound);
	if (!Sound)
	{
		return; // 사운드 미지정 — 무동작(에셋 나중 지정 대비).
	}
	UGameplayStatics::PlaySoundAtLocation(World, Sound, Location, VolumeMultiplier);
}
