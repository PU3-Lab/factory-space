// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Resource/ResourceBase.h"
#include "OJJ_RiverLiquidResource.generated.h"

/**
 * [#3] 강(WaterBodyRiver)을 펌프용 액체 자원으로 노출하는 비가시 무한 수원.
 *
 * WaterBodyRiver는 UE Water 플러그인 액터라 AResourceBase가 아니다 → 펌프 FindAdjacentWater
 * (GetLiquidResourceAtCell)가 강에서 수원을 못 찾아 배치 불가였다. 이 액터를 그리드가 1개 스폰하고
 * 강 WaterCells를 자원 레이어(OJJ_ResourceCellToActor)에 등록하면, 펌프가 강을 수원으로 인식한다.
 *
 * - ResourceData = DT_ResourceData "water"(form=liquid), bIsInfinite=true (무한 생산).
 * - 비주얼/콜리전 없음(강은 WaterBodyRiver가 렌더). 자동 그리드 등록 안 함 — 그리드가 강 셀을 명시 등록한다.
 * - 기존 WaterArea 경로와 독립(추가만) — 회귀 0.
 */
UCLASS()
class WANTED_FACTORY_API AOJJ_RiverLiquidResource : public AResourceBase
{
	GENERATED_BODY()

public:
	AOJJ_RiverLiquidResource();

protected:
	// 자동 등록 비활성 — 사각 mesh-bounds 등록을 막고, 그리드가 강 WaterCells를 명시 등록(OJJ_EnsureRiverLiquidResource).
	virtual void RegisterToGrid() override {}
};
