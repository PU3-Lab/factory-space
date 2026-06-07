// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ResourceBase.h"
#include "WaterArea.generated.h"

class AOJJ_Grid;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UBoxComponent;

/**
 * 흐르는 강(Water) 영역 (OJJ 소유). AResourceBase를 다중 셀로 확장 — 직사각형 셀 영역을 그리드 점유에
 * 등록한다. DataTable에서 form="liquid"로 설정하면 펌프(APump)의 수원 인접 판정 대상이 된다(펌프는 Claim
 * 없이 무제한 사용 — 광맥과 다름). 무한 수원은 생성자에서 bIsInfinite=true로 강제.
 *
 * 설계:
 *  - 점유: RegisterToGrid를 override해 AreaSizeInCells 범위의 셀을 일괄 등록(OJJ_RegisterActorCells).
 *    광맥의 단일 셀 경로(부모 기본 구현)는 건드리지 않음 — 리그레션 0.
 *  - 해제: EndPlay에서 등록 origin 셀로 OJJ_RemoveActorAt 호출 → 액터 전체 셀 대칭 해제.
 *  - 머신 배치 거부: 등록된 Water 셀은 CanPlaceMachine의 점유 검사로 자동 거부(광맥과 동일 경로 — 공짜).
 *  - 비주얼(플레인 메시/머티리얼)은 별도 커밋. 본 클래스 1차에는 점유 등록만 담당.
 *
 * 맵 사전 배치: 광맥과 동일하게 BeginPlay(부모)에서 RegisterToGrid 호출.
 */
UCLASS()
class WANTED_FACTORY_API AWaterArea : public AResourceBase
{
	GENERATED_BODY()

public:
	AWaterArea();

	// 에디터에서 배치/AreaSizeInCells 변경 시 플레인이 즉시 따라오도록 비주얼 갱신.
	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 다중 셀 등록(부모 단일 셀 경로 대체 — override). 광맥 경로는 영향 없음.
	virtual void RegisterToGrid() override;

	// 점유할 직사각형 영역 크기(셀 단위). 액터 위치 셀을 좌하단 origin으로 X×Y 셀을 등록. 강은 길쭉하게.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water", meta = (ClampMin = "1"))
	FIntPoint AreaSizeInCells = FIntPoint(3, 1);

	// 강 비주얼 플레인 — 영역 전체를 한 장으로 스케일(셀별 분할 금지: 후속 패닝 머티리얼 UV 이음새 방지).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Water|Visual")
	TObjectPtr<UStaticMeshComponent> WaterPlaneMesh;

	// 물 머티리얼(M_River 등). 런타임에 MID로 래핑돼 FlowVelocity가 주입된다. 미지정 시 플레인 기본 머티리얼.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Visual")
	TObjectPtr<UMaterialInterface> WaterMaterial;

	// 강 흐름 방향+속도(머티리얼 UV 패닝). XY만 사용(Z 무시). 강마다 방향이 달라 인스턴스별 조정 — M_River의
	// "FlowVelocity" VectorParameter로 주입(MID). 머티리얼에 동명 파라미터가 없으면 무시될 뿐 에러 없음.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Visual")
	FVector2D FlowVelocity = FVector2D(0.05f, 0.0f);

	// 그리드 바닥/머신·화살표와의 z-fighting 방지용 높이 오프셋(액터 z 기준 상대). 에디터에서 조정.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Visual")
	float VisualZOffset = 2.0f;

	// 캐릭터(Pawn) 진입 차단 볼륨 — 영역 전체를 덮는 보이지 않는 벽. Pawn만 Block, 그 외 전 채널 Ignore
	// (⚠️ 빌드 커서/베이크 ↓트레이스가 이 박스에 걸리면 안 됨 — Visibility 등 Ignore 필수).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Water|Collision")
	TObjectPtr<UBoxComponent> WaterBlockingVolume;

	// 차단 벽 높이(uu, 액터 바닥에서 위로). 점프로 못 넘게 충분히. 에디터 조정.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Collision", meta = (ClampMin = "1.0"))
	float BlockingWallHeight = 250.0f;

private:
	// AreaSizeInCells/그리드 CellSize에 맞춰 플레인 스케일·위치 갱신(에디터 OnConstruction + 등록 후).
	void UpdateWaterVisual();

	// WaterMaterial을 래핑한 동적 인스턴스(FlowVelocity 주입용). 부모가 바뀔 때만 재생성. Transient — 직렬화 제외.
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WaterMID;

	// EndPlay 대칭 해제용. RegisterToGrid 성공 시 저장(전체 해제는 액터의 아무 셀로나 가능 — OJJ_RemoveActorAt).
	FIntPoint RegisteredOrigin = FIntPoint(0, 0);
	bool bRegistered = false;
};
