#pragma once

#include "CoreMinimal.h"

class USoundBase;
class UWorld;

/**
 * 발소리 표면 판별+재생 공용 유틸 — AnimNotify_OJJ_Footstep(걸음/등반)과 AOJJ_Player::Landed(착지)가 공유.
 * 판별 순서: ① 발 침수(OJJ_QueryWaterBodyAt) → Wet ② Foundation 셀(IsCellOnFoundation) → Metal
 * ③ 그 외/그리드 부재(프리뷰/타 레벨)/off-grid → Sand. 선택된 표면의 사운드가 null이면 무동작.
 */
class WANTED_FACTORY_API OJJ_FootstepStatics
{
public:
	static void PlaySurfaceFootstep(UWorld* World, const FVector& Location,
		USoundBase* SandSound, USoundBase* MetalSound, USoundBase* WetSound, float VolumeMultiplier);
};
