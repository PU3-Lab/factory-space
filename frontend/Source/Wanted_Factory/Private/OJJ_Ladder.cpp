// Fill out your copyright notice in the Description page of Project Settings.

#include "OJJ_Ladder.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "OJJ_Player.h"
#include "UObject/ConstructorHelpers.h"

AOJJ_Ladder::AOJJ_Ladder()
{
	PrimaryActorTick.bCanEverTick = false;

	// 씬 루트 = 바닥 기준점(메시 오프셋/스케일과 분리 → GetActorLocation은 항상 하단).
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	LadderMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LadderMesh"));
	LadderMesh->SetupAttachment(Root);

	// 프로토 비주얼: 엔진 큐브를 얇고 길게. 등반은 트리거+비행 이동이라 메시는 충돌 없음(플레이어를 막지 않음).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		LadderMesh->SetStaticMesh(CubeMesh.Object);
	}
	LadderMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LadderMesh->SetCanEverAffectNavigation(false);

	// 등반 트리거: 루트 직속(스케일 1). 오버랩 전용(Pawn 오버랩만 생성, Block 없음).
	ClimbTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("ClimbTrigger"));
	ClimbTrigger->SetupAttachment(Root);
	ClimbTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ClimbTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	ClimbTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ClimbTrigger->SetGenerateOverlapEvents(true);
	ClimbTrigger->SetCanEverAffectNavigation(false);
}

void AOJJ_Ladder::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyDimensions();
}

void AOJJ_Ladder::ApplyDimensions()
{
	// 엔진 Cube는 변당 100uu. 사다리 몸체: 가로/세로 얇게(0.2 = 20uu), 높이 = ClimbHeight.
	// 루트(바닥) 기준 위로 ClimbHeight/2 올려 큐브 중심을 사다리 중앙에 둠 → 하단이 액터 원점.
	if (LadderMesh && LadderMesh->GetStaticMesh())
	{
		const float ZScale = FMath::Max(ClimbHeight, 1.f) / 100.f;
		LadderMesh->SetRelativeScale3D(FVector(0.2f, 0.2f, ZScale));
		LadderMesh->SetRelativeLocation(FVector(0.f, 0.f, ClimbHeight * 0.5f));
	}

	if (ClimbTrigger)
	{
		// 루트 직속(스케일 1)이라 BoxExtent = 월드 크기 그대로. 0~ClimbHeight 전 높이 + 상단 여유 커버.
		// 중심 = (ClimbHeight + TriggerTopMargin)/2 위, 반높이 = 동일 → 바닥(0)부터 상단+여유까지 빈틈없이.
		const float CoverTop = ClimbHeight + TriggerTopMargin;
		ClimbTrigger->SetBoxExtent(FVector(TriggerHalfWidth, TriggerHalfWidth, CoverTop * 0.5f), false);
		ClimbTrigger->SetRelativeLocation(FVector(0.f, 0.f, CoverTop * 0.5f));
	}
}

void AOJJ_Ladder::BeginPlay()
{
	Super::BeginPlay();

	if (ClimbTrigger)
	{
		ClimbTrigger->OnComponentBeginOverlap.AddDynamic(this, &AOJJ_Ladder::OnTriggerBeginOverlap);
		ClimbTrigger->OnComponentEndOverlap.AddDynamic(this, &AOJJ_Ladder::OnTriggerEndOverlap);
	}
}

void AOJJ_Ladder::OnTriggerBeginOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*Sweep*/)
{
	if (AOJJ_Player* Player = Cast<AOJJ_Player>(OtherActor))
	{
		Player->BeginClimb(this);
	}
}

void AOJJ_Ladder::OnTriggerEndOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
	// ⚠️ 등반 종료를 여기서 하지 않는다 — 수직 등반은 수평 드리프트가 없어(Move가 D/A 무시) 트리거를
	// '옆으로' 벗어날 일이 없고, '위/아래'로 벗어나는 건 Move의 상/하단 도달 판정이 전담한다.
	// EndOverlap로 끊으면 등반 도중(트리거 경계)에서 EndClimb→중력 복귀→낙하가 발생했음. 진단 로그만 남김.
	if (Cast<AOJJ_Player>(OtherActor))
	{
		UE_LOG(LogTemp, Verbose, TEXT("[Ladder] 트리거 EndOverlap (등반 종료는 Move의 도달 판정이 전담 — 무동작)"));
	}
}
