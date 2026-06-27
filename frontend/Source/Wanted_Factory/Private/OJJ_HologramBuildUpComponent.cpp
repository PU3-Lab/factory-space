// Fill out your copyright notice in the Description page of Project Settings.

#include "OJJ_HologramBuildUpComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

UOJJ_HologramBuildUpComponent::UOJJ_HologramBuildUpComponent()
{
	// 빌드업 진행 중에만 틱 — 기본 off, StartBuildUp에서 켜고 완료 시 끈다(평상시 0비용).
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UOJJ_HologramBuildUpComponent::StartBuildUp(UStaticMeshComponent* Source)
{
	// 머티리얼 미지정(에디터 제작 전) → 효과 skip. 배치 자체는 정상.
	if (!HologramMaterial)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[Hologram] HologramMaterial 미지정 — 빌드업 skip(배치 정상)."));
		return;
	}
	if (!Source || !Source->GetStaticMesh())
	{
		return;
	}
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// 재진입(연속 배치 등) 안전 — 이전 프록시 정리.
	Finish();

	Proxy = NewObject<UStaticMeshComponent>(Owner);
	if (!Proxy)
	{
		return;
	}
	Proxy->SetStaticMesh(Source->GetStaticMesh());
	Proxy->SetWorldTransform(Source->GetComponentTransform());
	// z-fighting 방지: 실제보다 약간 크게 → 경계 위 영역이 실제 표면을 확실히 가린다(머티리얼 WPO 쓰면 1.0으로 둘 것).
	if (ProxyScaleMultiplier > 1.0f)
	{
		Proxy->SetWorldScale3D(Source->GetComponentTransform().GetScale3D() * ProxyScaleMultiplier);
	}
	Proxy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Proxy->SetCastShadow(false);
	Proxy->RegisterComponent();

	MID = UMaterialInstanceDynamic::Create(HologramMaterial, this);
	if (MID)
	{
		const int32 NumMats = Proxy->GetNumMaterials();
		for (int32 i = 0; i < NumMats; ++i)
		{
			Proxy->SetMaterial(i, MID);
		}
		// Z 마스크 범위 = 소스 메시 월드 바운드(머티리얼은 WorldPosition.Z를 MinZ~MaxZ로 정규화).
		const FBoxSphereBounds B = Source->Bounds;
		MID->SetScalarParameterValue(TEXT("MinZ"), B.Origin.Z - B.BoxExtent.Z);
		MID->SetScalarParameterValue(TEXT("MaxZ"), B.Origin.Z + B.BoxExtent.Z);
		MID->SetScalarParameterValue(TEXT("Progress"), 0.0f);
	}

	Elapsed = 0.0f;
	bRunning = true;
	SetComponentTickEnabled(true);
}

void UOJJ_HologramBuildUpComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bRunning)
	{
		return;
	}

	Elapsed += DeltaTime;
	const float P = FMath::Clamp(Duration > 0.0f ? Elapsed / Duration : 1.0f, 0.0f, 1.0f);
	if (MID)
	{
		MID->SetScalarParameterValue(TEXT("Progress"), P);
	}
	if (P >= 1.0f)
	{
		Finish(); // 완료 — 프록시 제거(실제 메시만 100% 표시 상태로 귀결).
	}
}

void UOJJ_HologramBuildUpComponent::Finish()
{
	if (Proxy)
	{
		Proxy->DestroyComponent();
		Proxy = nullptr;
	}
	MID = nullptr;
	bRunning = false;
	SetComponentTickEnabled(false);
}

void UOJJ_HologramBuildUpComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Finish();
	Super::EndPlay(EndPlayReason);
}
