// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OJJ_Ladder.generated.h"

class UInstancedStaticMeshComponent;
class UBoxComponent;
class AOJJ_Player;

/**
 * 사다리 — 지면 → Foundation 수직 등반 (#184, 부유 슬래브 아래 접근).
 *
 * ⚠️ MVP 결정 (이슈 #184 결정점):
 *  - 등반 이동 = 커스텀 무브먼트(플레이어가 MOVE_Flying+중력0로 수직 이동). 텔레포트 아님.
 *  - 지면 셀 점유 = 시각+이동 전용, 그리드 장부 '미등록'. → 충돌 규칙 없음, 다른 배치와 겹칠 여지 허용.
 *    겹침 문제가 실제로 생기면 장부 등록(이슈 옵션 2)으로 승격할 것.
 *  - 높이 = 수동 ClimbHeight 프로퍼티. 양 끝 자동 산출(GroundZ/GetFoundationSurfaceZ 재사용)은 후속.
 *
 * 구성:
 *  - LadderMesh: 1칸 메시(Ladder.Ladder)를 ClimbHeight까지 수직 타일링한 ISM. 충돌 없음(등반은 트리거+비행 이동으로 처리).
 *  - ClimbTrigger: 사다리 전 높이를 덮는 오버랩 박스. 플레이어 진입 시 BeginClimb, 이탈 시 EndClimb(안전망).
 *
 * 등반 축/끝점:
 *  - Bottom = 액터 위치 Z, Top = Bottom + ClimbHeight. 액터를 지면에 두고 ClimbHeight를 Foundation 상면까지로.
 *  - StepOff 방향 = 액터 전방(+X). 배치 시 +X가 Foundation 위를 향하도록 회전.
 *  - ⚠️ 액터/루트 '스케일 1'로 배치할 것 — 끝점 산식(Top=Z+ClimbHeight)이 액터 스케일을 반영하지 않아
 *    스케일을 주면 비주얼/트리거 높이와 도달 판정이 어긋난다(MVP). 크기 조절은 ClimbHeight로.
 */
UCLASS()
class WANTED_FACTORY_API AOJJ_Ladder : public AActor
{
	GENERATED_BODY()

public:
	AOJJ_Ladder();

	virtual void OnConstruction(const FTransform& Transform) override;

	// 등반 끝점(월드 Z). 플레이어가 상/하단 도달 판정에 사용.
	float GetClimbBottomZ() const { return GetActorLocation().Z; }
	float GetClimbTopZ() const { return GetActorLocation().Z + ClimbHeight; }

	// 상단 도달 시 Foundation 위로 내딛는 수평 방향(정규화). 액터 전방(+X).
	FVector GetStepOffDirection() const { return GetActorForwardVector().GetSafeNormal2D(); }

	// [#184 배치] 스폰 시 ClimbHeight 주입 — SpawnActorDeferred → FinishSpawning 전에 호출하면 OnConstruction이
	// 이 값으로 ApplyDimensions(메시/트리거 사이징). 런타임 변경도 즉시 반영되도록 여기서도 ApplyDimensions 호출.
	void OJJ_SetClimbHeight(float NewClimbHeight);

protected:
	virtual void BeginPlay() override;

	// 씬 루트 = 사다리 '바닥' 기준점. GetActorLocation().Z = 하단(끝점 산식의 단일 기준).
	// 메시 오프셋/스케일이 GetActorLocation을 오염시키지 않도록 루트를 메시와 분리.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder")
	TObjectPtr<USceneComponent> Root;

	// [#184] 1칸 메시(Ladder.Ladder)를 ClimbHeight까지 수직 타일링한 ISM. 충돌 없음(등반은 트리거+비행).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder")
	TObjectPtr<UInstancedStaticMeshComponent> LadderMesh;

	// 트리거는 루트 직속(스케일 1) — 메시 스케일과 무관하게 박스 크기를 의도대로. 전 높이 + 상단 여유 커버.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder")
	TObjectPtr<UBoxComponent> ClimbTrigger;

	// 등반 높이(uu). 하단(액터 Z)부터 이만큼 위가 상단(Foundation 상면). MVP 수동값.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder", meta = (ClampMin = "1.0"))
	float ClimbHeight = 300.f;

	// [#184] 1칸 메시 로컬 회전 보정(deg). 메시는 원래 자세에서 세로(Z)·크기 OK였고 면만 벽을 안 봐서 Z축
	// (Yaw) 90°만 돌리면 된다(Pitch/Roll 불필요). 액터가 이미 변별 inward(+X=Foundation)로 회전돼 있어 이 로컬
	// Yaw가 그 위에 합성 → 4방향 변 모두 동일하게 면이 벽 향함(변별 추가 보정 불필요). 반대면 -90, 등지면 +180.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	FRotator MeshLocalRotation = FRotator(0.f, 90.f, 0.f);

	// [#184] 1칸 메시 균일 스케일 배율 — 1칸 191cm는 과대 → ~0.18로 ~34cm/칸(N≈6). 액터 스케일 금지(끝점 산식
	// 미반영)라 ISM 인스턴스 스케일로 적용. SegmentHeight는 스케일 반영분으로 재산출(N 정합).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder", meta = (ClampMin = "0.01"))
	float MeshScaleMultiplier = 0.18f;

	// [#184] 1칸 메시 높이(uu) — 회전·스케일 반영된 1칸 박스의 세로(Z) 높이에서 자동 산출(ApplyDimensions). 타일 수 N 기준.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Ladder")
	float SegmentHeight = 0.f;

	// 트리거 박스 가로/세로 반경(uu). 사다리 주변 근접 감지 폭. [#184] 60→40으로 좁힘 — W-시작이라 가까이서만
	// 감지하면 충분(멀리서 진입 스냅 순간이동 방지). 사다리 앞인데 감지 안 되면 살짝 넓힐 것.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder", meta = (ClampMin = "1.0"))
	float TriggerHalfWidth = 40.f;

	// 트리거를 상단 위로 더 덮는 여유(uu). 상단 step-off 판정 전에 캐릭터가 트리거를 벗어나
	// EndOverlap이 먼저 나는 것을 방지(캡슐 높이만큼 여유 권장).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder", meta = (ClampMin = "0.0"))
	float TriggerTopMargin = 120.f;

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
	// 메시/트리거를 ClimbHeight에 맞춰 배치(에디터/스폰 공통). 큐브(100uu) 기준 스케일/오프셋 계산.
	void ApplyDimensions();
};
