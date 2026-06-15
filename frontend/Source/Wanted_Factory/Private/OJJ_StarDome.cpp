// Fill out your copyright notice in the Description page of Project Settings.

#include "OJJ_StarDome.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AOJJ_StarDome::AOJJ_StarDome()
{
	// 매 틱 카메라를 따라가야 하므로 Tick 필요.
	PrimaryActorTick.bCanEverTick = true;

	DomeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DomeMesh"));
	SetRootComponent(DomeMesh);

	// 기본 엔진 구 메시. 안쪽에서 보이게 하려면 머티리얼이 Two-Sided여야 한다(M_Stars가 그렇게 생성됨).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		DomeMesh->SetStaticMesh(SphereMesh.Object);
	}

	// 절차적 별 머티리얼. 미발견 시 에디터에서 수동 지정 가능(UPROPERTY는 아니지만 인스턴스에서 교체 가능).
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> StarMat(TEXT("/Game/OJJ/Sky/M_Stars.M_Stars"));
	if (StarMat.Succeeded())
	{
		DomeMesh->SetMaterial(0, StarMat.Object);
	}

	// 빌드 레이캐스트/이동을 가리지 않도록 충돌 제거, 그림자/라이팅 영향 제거(순수 표시용).
	DomeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DomeMesh->SetCastShadow(false);
	DomeMesh->bReceivesDecals = false;
	DomeMesh->SetCanEverAffectNavigation(false);
}

void AOJJ_StarDome::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 회전은 고정(별자리 고정). 크기만 크게 — 에디터 뷰포트/스폰 양쪽에서 즉시 반영.
	SetActorScale3D(FVector(DomeScale));
}

void AOJJ_StarDome::BeginPlay()
{
	Super::BeginPlay();
}

void AOJJ_StarDome::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bFollowCamera)
	{
		return;
	}

	// 0번 플레이어 카메라 위치로 이동(회전은 유지 → 별자리 고정, 시차 없음).
	if (const APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		SetActorLocation(CamMgr->GetCameraLocation());
	}
}
