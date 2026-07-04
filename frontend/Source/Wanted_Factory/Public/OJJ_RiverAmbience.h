#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OJJ_RiverAmbience.generated.h"

class AWaterBodyRiver;
class UAudioComponent;

/**
 * 강물 소리 스플라인 추종 액터 — AmbientSound 여러 개 대신 AudioComponent 하나를
 * 타이머(UpdateInterval)마다 플레이어의 강 스플라인 최근접점으로 옮긴다.
 * 강(WaterBodyRiver) 스플라인을 그대로 읽으므로 강 모양 수정에 자동 추종.
 *
 * 사운드/루핑/감쇠는 전부 SoundCue + Attenuation 에셋에 위임 — 액터는 위치만 책임.
 * TargetRiver 미지정이면 BeginPlay에서 월드 첫 WaterBodyRiver 자동 탐색(L_Planet엔 1개).
 */
UCLASS()
class WANTED_FACTORY_API AOJJ_RiverAmbience : public AActor
{
	GENERATED_BODY()

public:
	AOJJ_RiverAmbience();

protected:
	virtual void BeginPlay() override;

	// 루핑 물소리 컴포넌트 — 사운드(Cue)/감쇠는 에디터에서 이 컴포넌트에 직접 할당.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RiverAmbience")
	TObjectPtr<UAudioComponent> AudioComponent;

	// 추종할 강. 미지정이면 BeginPlay에서 월드 첫 WaterBodyRiver 자동 탐색.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RiverAmbience")
	TObjectPtr<AWaterBodyRiver> TargetRiver;

	// 위치 갱신 주기(초). 발원지 점프가 눈에 띄면 낮추기 — 감쇠 반경 대비 플레이어 이동속도면 0.2s 충분.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RiverAmbience", meta = (ClampMin = "0.05"))
	float UpdateInterval = 0.2f;

private:
	FTimerHandle UpdateTimerHandle;

	// 플레이어 → 강 스플라인 최근접점으로 AudioComponent 이동(타이머 콜백).
	void UpdateAudioLocation();
};
