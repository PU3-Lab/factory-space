// Fill out your copyright notice in the Description page of Project Settings.

#include "OJJ_HologramPathBuildUpComponent.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

UOJJ_HologramPathBuildUpComponent::UOJJ_HologramPathBuildUpComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UOJJ_HologramPathBuildUpComponent::StartBuildUp(const TArray<UInstancedStaticMeshComponent*>& SourceISMs, const FVector& PathStart, const FVector& PathEnd)
{
	if (!PathHologramMaterial)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[HologramPath] PathHologramMaterial 미지정 — 빌드업 skip(배치 정상)."));
		return;
	}
	AActor* Owner = GetOwner();
	if (!Owner || SourceISMs.Num() == 0)
	{
		return;
	}

	// 재진입 안전 — 이전 프록시 제거.
	Finish();

	MID = UMaterialInstanceDynamic::Create(PathHologramMaterial, this);
	if (!MID)
	{
		return;
	}
	const FVector Delta = PathEnd - PathStart;
	const float Length = FMath::Max(1.0f, (float)Delta.Size());
	const FVector Dir = Delta.GetSafeNormal();
	MID->SetVectorParameterValue(TEXT("PathStart"), FLinearColor((float)PathStart.X, (float)PathStart.Y, (float)PathStart.Z, 0.0f));
	MID->SetVectorParameterValue(TEXT("PathDir"), FLinearColor((float)Dir.X, (float)Dir.Y, (float)Dir.Z, 0.0f));
	MID->SetScalarParameterValue(TEXT("PathLength"), Length);
	MID->SetScalarParameterValue(TEXT("Progress"), 0.0f);

	// 소스 ISM 1개당 프록시 ISM 1개 — 메시 + 인스턴스 전부 복제(실제는 무수정).
	for (UInstancedStaticMeshComponent* Src : SourceISMs)
	{
		if (!Src || !Src->GetStaticMesh() || Src->GetInstanceCount() == 0)
		{
			continue;
		}
		UInstancedStaticMeshComponent* Proxy = NewObject<UInstancedStaticMeshComponent>(Owner);
		if (!Proxy)
		{
			continue;
		}
		Proxy->SetStaticMesh(Src->GetStaticMesh());
		Proxy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Proxy->SetCastShadow(false);
		Proxy->SetMobility(EComponentMobility::Movable);
		Proxy->RegisterComponent();

		const int32 NumMats = Proxy->GetNumMaterials();
		for (int32 m = 0; m < NumMats; ++m)
		{
			Proxy->SetMaterial(m, MID);
		}

		// 인스턴스 전부 복제(월드 트랜스폼) + z-fighting 방지 스케일.
		const int32 Count = Src->GetInstanceCount();
		for (int32 i = 0; i < Count; ++i)
		{
			FTransform T;
			if (!Src->GetInstanceTransform(i, T, /*bWorldSpace=*/true))
			{
				continue;
			}
			if (ProxyScaleMultiplier > 1.0f)
			{
				T.SetScale3D(T.GetScale3D() * ProxyScaleMultiplier);
			}
			Proxy->AddInstance(T, /*bWorldSpace=*/true);
		}

		Proxies.Add(Proxy);
	}

	if (Proxies.Num() == 0)
	{
		MID = nullptr;
		return;
	}

	Elapsed = 0.0f;
	bRunning = true;
	SetComponentTickEnabled(true);
}

void UOJJ_HologramPathBuildUpComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
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

	// 정리 안전망 ①완료 ②timeout(Tick 누락/멈춤 대비).
	if (P >= 1.0f || Elapsed > Duration + 2.0f)
	{
		Finish();
	}
}

void UOJJ_HologramPathBuildUpComponent::Finish()
{
	for (UInstancedStaticMeshComponent* Proxy : Proxies)
	{
		if (Proxy)
		{
			Proxy->DestroyComponent();
		}
	}
	Proxies.Empty();
	MID = nullptr;
	bRunning = false;
	SetComponentTickEnabled(false);
}

void UOJJ_HologramPathBuildUpComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Finish(); // ③ 레벨/게임 종료 시 프록시 제거(실제 ISM은 무수정이라 원복 불필요).
	Super::EndPlay(EndPlayReason);
}
