#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_OJJ_Footstep.generated.h"

class USoundBase;

// 발소리 표면 강제 지정 — Auto면 그리드 질의로 판별, 그 외엔 질의 생략(사다리 등반 애니 등
// 발밑 셀이 표면과 무관한 컨텍스트용. 예: 사다리 = 항상 Metal).
UENUM()
enum class EOJJFootstepSurface : uint8
{
	Auto,
	Sand,
	Metal
};

/**
 * 발소리 노티파이 — 걷기/뛰기 시퀀스의 발 닿는 프레임에 심는다.
 *
 * 표면 판별(우선순위): PhysMat 인프라 없이 OJJ_Grid 질의 2종.
 *  ① 얕은 물(발 위치 침수 — OJJ_QueryWaterBodyAt, 수영 감지와 동일 소스) → Wet.
 *     수영 중(수심 ≥ SwimEnterWaterDepth)엔 수영 애니로 전환돼 발소리 노티파이 자체가 안 돈다 —
 *     여기 걸리는 건 걷기 유지되는 얕은 물뿐이라 별도 수심 상한 불필요.
 *  ② Foundation 커버 셀(WorldToGrid → IsCellOnFoundation) → Metal.
 *  ③ 그 외 → Sand. 그리드 부재(프리뷰/타 레벨)나 off-grid도 모래 폴백.
 *
 * ⚠️ BS_Man_Locomotion 블렌드 중 walk/run 시퀀스 노티파이 이중 발화 가능 — 엔진 기본
 *    Trigger Weight Threshold(0.5)가 걸러주는 것에 의존. PIE에서 walk↔run 전환 구간 확인 필요.
 * ⚠️ Meshy 애니 재임포트 시 시퀀스에 심은 노티파이가 소실된다(Man_Idle hips 보정 소실 전례) — 재임포트 후 재심기.
 */
UCLASS(meta = (DisplayName = "OJJ Footstep"))
class WANTED_FACTORY_API UAnimNotify_OJJ_Footstep : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	// 지면(모래) 발소리. 미지정이면 해당 표면에서 무동작(에셋 나중 지정 대비).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	TObjectPtr<USoundBase> SandSound;

	// Foundation(금속) 발소리. 미지정이면 해당 표면에서 무동작.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	TObjectPtr<USoundBase> MetalSound;

	// 얕은 물(철벅) 발소리 — 발이 물에 잠겼지만 수영 전환 전 수심. 미지정이면 해당 표면에서 무동작.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	TObjectPtr<USoundBase> WetSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep", meta = (ClampMin = "0.0"))
	float VolumeMultiplier = 1.0f;

	// Auto = 그리드 판별(①물 ②Foundation ③모래). Sand/Metal = 질의 생략, 해당 사운드 고정(사다리 등반용).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	EOJJFootstepSurface SurfaceOverride = EOJJFootstepSurface::Auto;
};
