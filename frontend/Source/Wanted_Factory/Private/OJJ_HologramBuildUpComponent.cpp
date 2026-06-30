// Fill out your copyright notice in the Description page of Project Settings.

#include "OJJ_HologramBuildUpComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
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

	// 머티리얼 WorldPosition.Z 정규화 범위(바닥~꼭대기). 단일메시=Source->Bounds, ISM=인스턴스 월드 AABB(아래 계산).
	float HologramMinZ = 0.0f;
	float HologramMaxZ = 0.0f;

	// 프록시 생성 — 소스가 ISM(사다리 등 여러 칸 적층)이면 프록시도 ISM으로 만들어 인스턴스를 그대로 복제하고,
	// 단일 SMC(머신/Foundation)면 기존 단일메시 경로. 둘 다 UStaticMeshComponent 파생이라 Proxy 멤버·하류 로직 공용.
	if (UInstancedStaticMeshComponent* SourceISM = Cast<UInstancedStaticMeshComponent>(Source))
	{
		// === ISM 경로(제네릭 — 사다리에 비결합. 컨베이어/파이프 등 임의 ISM에도 동일 적용) ===
		UInstancedStaticMeshComponent* ProxyISM = NewObject<UInstancedStaticMeshComponent>(Owner);
		if (!ProxyISM)
		{
			return;
		}
		ProxyISM->SetStaticMesh(SourceISM->GetStaticMesh());
		ProxyISM->SetWorldTransform(SourceISM->GetComponentTransform());
		// z-fighting 방지: 실제보다 약간 크게(단일메시 경로와 동일 정책).
		if (ProxyScaleMultiplier > 1.0f)
		{
			ProxyISM->SetWorldScale3D(SourceISM->GetComponentTransform().GetScale3D() * ProxyScaleMultiplier);
		}
		ProxyISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ProxyISM->SetCastShadow(false);
		ProxyISM->RegisterComponent();

		// 인스턴스는 로컬 공간 그대로 복사(프록시가 소스와 같은 월드 트랜스폼) + 동시에 월드 Z AABB를 누적.
		// ⚠️ ISM의 component Bounds는 인스턴스 추가 직후/타이밍에 따라 적층 전체를 안 담을 수 있어(범위 붕괴 →
		//    한꺼번에 점등) MinZ/MaxZ를 component Bounds 대신 인스턴스 월드 박스로 명시 계산한다(바닥→꼭대기 보장).
		const int32 NumInst = SourceISM->GetInstanceCount();
		const FBox MeshLocalBox = SourceISM->GetStaticMesh() ? SourceISM->GetStaticMesh()->GetBoundingBox() : FBox(ForceInit);
		FBox WorldBox(ForceInit);
		for (int32 i = 0; i < NumInst; ++i)
		{
			FTransform InstLocal;
			if (SourceISM->GetInstanceTransform(i, InstLocal, /*bWorldSpace=*/false))
			{
				ProxyISM->AddInstance(InstLocal, /*bWorldSpace=*/false);
			}
			FTransform InstWorld;
			if (SourceISM->GetInstanceTransform(i, InstWorld, /*bWorldSpace=*/true) && MeshLocalBox.IsValid)
			{
				WorldBox += MeshLocalBox.TransformBy(InstWorld);
			}
		}
		Proxy = ProxyISM;

		if (WorldBox.IsValid)
		{
			HologramMinZ = WorldBox.Min.Z;
			HologramMaxZ = WorldBox.Max.Z;
		}
		else
		{
			// 폴백: component Bounds(인스턴스 트랜스폼 조회 실패 등 비상시).
			const FBoxSphereBounds B = SourceISM->Bounds;
			HologramMinZ = B.Origin.Z - B.BoxExtent.Z;
			HologramMaxZ = B.Origin.Z + B.BoxExtent.Z;
		}
	}
	else
	{
		// === 단일메시 경로(머신/Foundation) — 분기 밖, 기존 동작 글자 그대로 불변 ===
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

		// 단일메시 Z 범위 = 소스 메시 월드 바운드(기존과 동일 — 회귀 0).
		const FBoxSphereBounds B = Source->Bounds;
		HologramMinZ = B.Origin.Z - B.BoxExtent.Z;
		HologramMaxZ = B.Origin.Z + B.BoxExtent.Z;
	}

	MID = UMaterialInstanceDynamic::Create(HologramMaterial, this);
	if (MID)
	{
		const int32 NumMats = Proxy->GetNumMaterials();
		for (int32 i = 0; i < NumMats; ++i)
		{
			Proxy->SetMaterial(i, MID);
		}
		// Z 마스크 범위(머티리얼은 WorldPosition.Z를 MinZ~MaxZ로 정규화) — 단일메시/ISM 공용.
		MID->SetScalarParameterValue(TEXT("MinZ"), HologramMinZ);
		MID->SetScalarParameterValue(TEXT("MaxZ"), HologramMaxZ);
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
