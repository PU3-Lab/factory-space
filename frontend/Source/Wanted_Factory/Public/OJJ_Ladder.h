// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OJJ_Ladder.generated.h"

class UStaticMeshComponent;
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
 *  - LadderMesh: 프로토 비주얼(엔진 큐브를 얇고 길게). 충돌 없음(등반은 트리거+비행 이동으로 처리).
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

	// [#184 배치] 고스트 프리뷰용 메시 컴포넌트(에셋은 LadderMesh의 StaticMesh). AOJJ_Foundation::GetSlabMesh 미러 —
	// 빌드 고스트가 실제 사다리와 같은 메시를 쓰게 해 "미리보기=배치" 비주얼 정합(후속 Meshy 교체도 자동 추종).
	UStaticMeshComponent* GetLadderMesh() const { return LadderMesh; }

	// [#184 배치] 스폰 시 ClimbHeight 주입 — SpawnActorDeferred → FinishSpawning 전에 호출하면 OnConstruction이
	// 이 값으로 ApplyDimensions(메시/트리거 사이징). 런타임 변경도 즉시 반영되도록 여기서도 ApplyDimensions 호출.
	void OJJ_SetClimbHeight(float NewClimbHeight);

protected:
	virtual void BeginPlay() override;

	// 씬 루트 = 사다리 '바닥' 기준점. GetActorLocation().Z = 하단(끝점 산식의 단일 기준).
	// 메시 오프셋/스케일이 GetActorLocation을 오염시키지 않도록 루트를 메시와 분리.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder")
	TObjectPtr<UStaticMeshComponent> LadderMesh;

	// 트리거는 루트 직속(스케일 1) — 메시 스케일과 무관하게 박스 크기를 의도대로. 전 높이 + 상단 여유 커버.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder")
	TObjectPtr<UBoxComponent> ClimbTrigger;

	// 등반 높이(uu). 하단(액터 Z)부터 이만큼 위가 상단(Foundation 상면). MVP 수동값.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder", meta = (ClampMin = "1.0"))
	float ClimbHeight = 300.f;

	// 트리거 박스 가로/세로 반경(uu). 사다리 주변 진입 판정 폭.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder", meta = (ClampMin = "1.0"))
	float TriggerHalfWidth = 60.f;

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
