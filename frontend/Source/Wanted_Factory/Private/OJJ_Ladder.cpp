// Fill out your copyright notice in the Description page of Project Settings.

#include "OJJ_Ladder.h"

#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "OJJ_Grid.h"
#include "OJJ_Player.h"
#include "UObject/ConstructorHelpers.h"

AOJJ_Ladder::AOJJ_Ladder()
{
	PrimaryActorTick.bCanEverTick = false;

	// 씬 루트 = 바닥 기준점(메시 오프셋/스케일과 분리 → GetActorLocation은 항상 하단).
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	LadderMesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LadderMesh"));
	LadderMesh->SetupAttachment(Root);

	// [#184] 1칸 사다리 메시(팀 에셋 구조 변경: Content/Assets/Platform/Ladder/StaticMeshes/Ladder).
	// ClimbHeight까지 수직 타일링(ApplyDimensions). 등반은 트리거+비행 이동이라 메시는 충돌 없음(플레이어를 막지 않음).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> LadderSegmentMesh(
		TEXT("/Game/Assets/Platform/Ladder/StaticMeshes/Ladder.Ladder"));
	if (LadderSegmentMesh.Succeeded())
	{
		LadderMesh->SetStaticMesh(LadderSegmentMesh.Object);
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
	// [#184] 1칸 메시를 ClimbHeight까지 수직 타일링. 회전(MeshLocalRotation)·스케일(MeshScaleMultiplier)을
	// 반영한 "유효 1칸 박스"의 세로(Z) 높이 = SegmentHeight → 회전/스케일을 바꿔도 칸 간격 자동 정합(연동).
	// N = ceil(ClimbHeight/Seg). 각 타일은 회전·스케일 적용 + 바닥(EffBox.Min.Z)을 i*SegmentHeight에 정렬
	// (피벗이 바닥이든 가운데든 자동 보정). 마지막 타일은 ClimbHeight를 다소 초과 가능(틈보다 초과가 안전).
	if (LadderMesh && LadderMesh->GetStaticMesh())
	{
		const float Scale = FMath::Max(MeshScaleMultiplier, 0.01f);
		const FTransform MeshXf(MeshLocalRotation, FVector::ZeroVector, FVector(Scale));

		const FBox LocalBox = LadderMesh->GetStaticMesh()->GetBoundingBox();
		const FBox EffBox = LocalBox.TransformBy(MeshXf); // 회전·스케일 반영 AABB
		const float SegZ = EffBox.GetSize().Z;
		SegmentHeight = (SegZ > KINDA_SMALL_NUMBER) ? SegZ : 100.f; // 무효 시 폴백(100uu).
		const FVector EffCenter = EffBox.GetCenter(); // 회전·스케일 후 박스 중심(피벗/회전 보정용).

		const float SafeHeight = FMath::Max(ClimbHeight, 1.f);
		const int32 N = FMath::Max(1, FMath::CeilToInt(SafeHeight / SegmentHeight));
		// [#184] 위 딱 맞춤: 올림(N)이라 총높이(N*Seg)가 ClimbHeight를 초과 → 전체 적층을 초과분만큼 아래로 내려
		// 꼭대기 칸 윗면 = ClimbHeight(Foundation 윗면)에 맞춘다. 남는 초과분은 맨 아래 칸이 지면 아래로 박힘(OK).
		const float Overshoot = N * SegmentHeight - SafeHeight; // ≥ 0

		LadderMesh->ClearInstances();
		for (int32 i = 0; i < N; ++i)
		{
			// XY: 타일을 액터 중심(=벽면 라인)에 정렬. Z: 바닥 i*SegmentHeight - 초과분(전체 아래로) → 위가 ClimbHeight에 딱.
			const FVector InstPos(-EffCenter.X, -EffCenter.Y, i * SegmentHeight - EffBox.Min.Z - Overshoot);
			LadderMesh->AddInstance(FTransform(MeshLocalRotation, InstPos, FVector(Scale)));
		}
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

void AOJJ_Ladder::OJJ_SetClimbHeight(float NewClimbHeight)
{
	// ClampMin=1.0과 동일 하한. 값 반영 후 즉시 ApplyDimensions로 메시/트리거 재사이징(스폰·런타임 공용).
	ClimbHeight = FMath::Max(NewClimbHeight, 1.f);
	ApplyDimensions();
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

void AOJJ_Ladder::OJJ_SetRegisteredGrid(AOJJ_Grid* InGrid)
{
	// 헤더선 AOJJ_Grid 전방선언뿐 → TWeakObjectPtr 대입을 완전 타입 보이는 여기서.
	RegisteredGrid = InGrid;
}

void AOJJ_Ladder::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// [#184 철거] 사다리 레이어(OJJ_LadderCells)에서 자동 해제 — 데모리시/cascade/세이브리셋/레벨종료 어느 경로로
	// 파괴돼도 맵을 청소(stale 누적 방지). RegisteredGrid는 OJJ_RegisterLadder가 주입(미등록이면 no-op).
	if (AOJJ_Grid* Grid = RegisteredGrid.Get())
	{
		Grid->OJJ_UnregisterLadder(this);
	}
	Super::EndPlay(EndPlayReason);
}

void AOJJ_Ladder::OnTriggerBeginOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*Sweep*/)
{
	// [#184] 감지/시작 분리: 진입 즉시 등반(BeginClimb) 대신 '근접 사다리'로만 등록. 실제 시작은 W 입력에서.
	// → 멀리서 강제 스냅(순간이동) 없음 + 이미 트리거 안이어도 W로 시작 가능.
	if (AOJJ_Player* Player = Cast<AOJJ_Player>(OtherActor))
	{
		Player->NotifyLadderOverlap(this);
	}
}

void AOJJ_Ladder::OnTriggerEndOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
	// [#184] 근접 후보에서 해제만. 등반 종료(EndClimb)는 여기서 하지 않는다 — 수직 등반은 수평 드리프트가 없어
	// 트리거를 '옆으로' 벗어날 일이 없고, '위/아래' 이탈은 Move의 상/하단 도달 판정이 전담(EndOverlap로 끊으면
	// 등반 도중 낙하 이력). 등반 중(CurrentLadder)은 이 해제와 별개라 영향 없음.
	if (AOJJ_Player* Player = Cast<AOJJ_Player>(OtherActor))
	{
		Player->NotifyLadderEndOverlap(this);
	}
}
