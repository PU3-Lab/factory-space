// Fill out your copyright notice in the Description page of Project Settings.

#include "OJJ_RiverLiquidResource.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/DataTable.h"
#include "UObject/ConstructorHelpers.h"

AOJJ_RiverLiquidResource::AOJJ_RiverLiquidResource()
{
	PrimaryActorTick.bCanEverTick = false;

	bIsInfinite = true;                          // 무한 수원 — 펌프가 ConsumeResource로 차감해도 고갈 안 됨(WaterArea 스펙 동일).
	bUseMeshBoundsForGridRegistration = false;   // mesh 바운즈 자동등록 안 함(그리드가 강 셀 명시 등록).

	// ResourceData = DT_ResourceData "water"(form=liquid). 헤드리스 덤프로 확인된 정확명(BP_WaterArea와 동일 행).
	static ConstructorHelpers::FObjectFinder<UDataTable> ResourceTable(
		TEXT("/Game/DataTable/DT_ResourceData.DT_ResourceData"));
	if (ResourceTable.Succeeded())
	{
		ResourceData.DataTable = ResourceTable.Object;
		ResourceData.RowName = FName(TEXT("water"));
	}

	// 비가시 — 강은 WaterBodyRiver가 렌더하므로 자체 메시/콜리전 불필요.
	if (Mesh)
	{
		Mesh->SetVisibility(false);
		Mesh->SetHiddenInGame(true);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCanEverAffectNavigation(false);
	}
}
