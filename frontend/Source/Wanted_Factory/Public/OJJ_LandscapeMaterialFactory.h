// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OJJ_LandscapeMaterialFactory.generated.h"

class UMaterial;
class ULandscapeLayerInfoObject;

/**
 * [지형] M_Landscape_FactoryGround 머티리얼을 코드로 생성하는 에디터 전용 유틸리티.
 * 홀로그램 머티리얼 팩토리(OJJ_HologramMaterialFactory)와 같은 패턴 — UMaterialEditingLibrary로
 * 노드 그래프를 구성하고, 강제 저장하지 않는다(사용자가 Ctrl+S).
 *
 * 2레이어 LandscapeLayerBlend (WeightBlend):
 *   - "Ground" = wkjncjv(Arid Gravel 바닥)   /Game/OJJ/Textures/T_wkjncjv_2K_B/N/ORM
 *   - "Gravel" = vd4pbdt(Rocky Sand 자갈)     /Game/Fab/Megascans/Surfaces/Rocky_Sand_vd4pbdt/.../T_vd4pbdt_2K_B/N/ORM
 *   두 세트 모두 ORM 합본(R:AO G:Rough B:Metal) — 동일 패킹.
 * 매크로 베리에이션: 큰 스케일 Noise를 BaseColor에 곱해 타일 반복(균일감)을 깬다.
 * 타일링: LandscapeLayerCoords × per-layer ScalarParameter(GroundTiling/GravelTiling).
 *
 * 사용법(에디터 안에서만): Editor Utility Blueprint/Widget에서 GenerateFactoryGroundMaterial 노드 호출,
 * 또는 CallInEditor 버튼. 생성 후 Content Browser에서 머티리얼/LayerInfo를 열어 확인하고 Ctrl+S로 저장.
 *
 * ⚠️ 에디터 전용(WITH_EDITOR). 런타임/패키지 빌드에는 포함되지 않는다.
 */
UCLASS()
class WANTED_FACTORY_API UOJJ_LandscapeMaterialFactory : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	// 지형 2레이어 블렌드 머티리얼 + (선택) LayerInfo 2개를 생성한다.
	//  bOverwriteExisting=true면 기존 머티리얼 노드만 비우고 in-place 재구성(패키지 삭제 없음).
	//  bCreateLayerInfo=true면 /Game/OJJ/Landscape/LI_FactoryGround, LI_FactoryGravel(없으면) 생성.
	//  실패 시 null.
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "OJJ|Landscape")
	static UMaterial* GenerateFactoryGroundMaterial(bool bOverwriteExisting = false, bool bCreateLayerInfo = true);

private:
	// LayerName을 가진 ULandscapeLayerInfoObject를 /Game/OJJ/Landscape/{AssetName}에 생성(있으면 반환).
	static ULandscapeLayerInfoObject* EnsureLayerInfo(const FString& AssetName, FName LayerName, const FLinearColor& DebugColor);
#endif
};
