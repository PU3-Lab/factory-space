// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ResourceBase.h"
#include "WaterArea.generated.h"

class AOJJ_Grid;

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

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 다중 셀 등록(부모 단일 셀 경로 대체 — override). 광맥 경로는 영향 없음.
	virtual void RegisterToGrid() override;

	// 점유할 직사각형 영역 크기(셀 단위). 액터 위치 셀을 좌하단 origin으로 X×Y 셀을 등록. 강은 길쭉하게.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water", meta = (ClampMin = "1"))
	FIntPoint AreaSizeInCells = FIntPoint(3, 1);

private:
	// EndPlay 대칭 해제용. RegisterToGrid 성공 시 저장(전체 해제는 액터의 아무 셀로나 가능 — OJJ_RemoveActorAt).
	FIntPoint RegisteredOrigin = FIntPoint(0, 0);
	bool bRegistered = false;
};
