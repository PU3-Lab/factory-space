// Fill out your copyright notice in the Description page of Project Settings.

#include "OJJ_DemolishEffectActor.h"

#include "Components/SceneComponent.h"
#include "OJJ_HologramBuildUpComponent.h"

AOJJ_DemolishEffectActor::AOJJ_DemolishEffectActor()
{
	// 연출은 빌드업 컴포넌트 틱으로 구동 — 액터 틱 불필요.
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void AOJJ_DemolishEffectActor::StartBuildDown(UStaticMeshComponent* SourceMesh, UMaterialInterface* HologramMaterial, const FLinearColor& Color, float Duration)
{
	// 연출 불가(머티리얼/메시 무효)면 즉시 소멸 — 빈 프록시 잔존 방지.
	if (!SourceMesh || !HologramMaterial)
	{
		Destroy();
		return;
	}

	Effect = NewObject<UOJJ_HologramBuildUpComponent>(this);
	if (!Effect)
	{
		Destroy();
		return;
	}
	Effect->HologramMaterial = HologramMaterial;
	Effect->Duration = (Duration > 0.0f) ? Duration : 1.0f;
	Effect->bDissolveOut = true;           // 위→아래로 메시 소멸(Z 스왑 + Progress 0→1).
	Effect->bOverrideColor = true;         // 철거색(빨강 등).
	Effect->OverrideColor = Color;
	Effect->bDestroyOwnerOnFinish = true;  // 완료 시 이 프록시 액터 자체 소멸.
	Effect->RegisterComponent();
	Effect->StartBuildUp(SourceMesh);

	// 머티리얼/메시 무효 등으로 시작 못 했으면(틱 미가동 → 완료 콜백도 안 옴) 자체 소멸로 잔존 방지.
	if (!Effect->IsRunning())
	{
		Destroy();
	}
}
