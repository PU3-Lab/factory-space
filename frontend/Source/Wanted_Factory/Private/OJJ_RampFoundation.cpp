// Fill out your copyright notice in the Description page of Project Settings.

#include "OJJ_RampFoundation.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "OJJ_Grid.h"
#include "UObject/ConstructorHelpers.h"

AOJJ_RampFoundation::AOJJ_RampFoundation()
{
	// 계단 박스 ISM — SlabMesh(베이스 생성자)와 동일 충돌 프로파일: 걷기(Pawn Block) +
	// 커서 스냅(Visibility Block) + 카메라 간섭 0. 베이크 ↓트레이스는 TActorIterator<AOJJ_Foundation>
	// ignore가 파생 클래스도 커버(OJJ_Grid 베이크 :720).
	StepMeshISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("StepMeshISM"));
	StepMeshISM->SetupAttachment(RootComponent);
	StepMeshISM->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	StepMeshISM->SetCollisionObjectType(ECC_WorldStatic);
	StepMeshISM->SetCollisionResponseToAllChannels(ECR_Ignore);
	StepMeshISM->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	StepMeshISM->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	StepMeshISM->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	StepMeshISM->SetCanEverAffectNavigation(false);
	StepMeshISM->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		StepMeshISM->SetStaticMesh(CubeMesh.Object);
	}
}

bool AOJJ_RampFoundation::OJJ_BuildPerCellSurfaceZ(FIntPoint EffSize, int32 RotationSteps, float BaseSurfaceZ,
	int32 RiseSteps, TArray<float>& OutCellZs) const
{
	// R = 오르는 방향(로컬 +X) 길이 = EffSize의 해당 월드 축(step 0/2 → X, 1/3 → Y — F3-0 스왑 규칙).
	// F3.6-0 일반화: 고정 램프에선 FoundationSize.X와 동치(스왑이 정확히 상쇄 — 동작 비트 동일)이고,
	// 동적 풋프린트(F3.6-1 자동 맞춤)에선 틈 길이 D를 그대로 따른다. r 산출은 월드 축 기준 switch.
	const int32 Step = ((RotationSteps % 4) + 4) % 4;
	const int32 R = (Step % 2 == 0) ? EffSize.X : EffSize.Y;
	if (R < 2 || RiseSteps < 1 || EffSize.X < 1 || EffSize.Y < 1)
	{
		// 1행 램프는 경사 불능, RiseSteps<1은 평지 퇴화(㉿) — 평판 단일값 경로 폴백.
		return false;
	}

	OutCellZs.SetNumUninitialized(EffSize.X * EffSize.Y);
	for (int32 LX = 0; LX < EffSize.X; ++LX)
	{
		for (int32 LY = 0; LY < EffSize.Y; ++LY)
		{
			// 풋프린트 로컬(월드 축) → 오르는 방향 행 r — f3 계획 §보강 산식.
			int32 r = 0;
			switch (Step)
			{
			case 0: r = LX; break;                       // 로컬 +X가 월드 +X
			case 1: r = LY; break;                       // 월드 +Y
			case 2: r = (EffSize.X - 1) - LX; break;     // 월드 −X
			case 3: r = (EffSize.Y - 1) - LY; break;     // 월드 −Y
			}
			// 양 끝 정합(턱 0): r=0 → BaseSurfaceZ 정확, r=R−1 → +RiseSteps×100 정확(float 오차 0 —
			// (R−1)/(R−1)=1). RiseSteps=1이면 F3-2 산식과 비트 동일.
			OutCellZs[LX * EffSize.Y + LY] =
				BaseSurfaceZ + ((float)r / (float)(R - 1)) * RiseSteps * AOJJ_Grid::OJJ_FoundationSnapStep;
		}
	}
	return true;
}

float AOJJ_RampFoundation::OJJ_ComputeSnapLift(const AOJJ_Grid& Grid, FIntPoint Origin, FIntPoint EffSize,
	int32 RotationSteps, FString* OutHeightSource) const
{
	// 낮은 끝(r=0) 기준 좌표: 행(풋프린트 안 — 씨앗 폴백용)과 바깥 인접 라인(엣지 스냅용)을 함께 산출.
	const int32 Step = ((RotationSteps % 4) + 4) % 4;
	FIntPoint RowOrigin = Origin;   // 낮은 끝 행(풋프린트 안)
	FIntPoint RowSize = EffSize;
	FIntPoint LineOrigin = Origin;  // 낮은 끝 바깥 인접 라인(이웃 판)
	FIntPoint LineSize = EffSize;
	switch (Step)
	{
	case 0: // 낮은 끝 = 서쪽 변
		RowSize = FIntPoint(1, EffSize.Y);
		LineOrigin.X = Origin.X - 1;             LineSize = FIntPoint(1, EffSize.Y);
		break;
	case 1: // 남쪽 변
		RowSize = FIntPoint(EffSize.X, 1);
		LineOrigin.Y = Origin.Y - 1;             LineSize = FIntPoint(EffSize.X, 1);
		break;
	case 2: // 동쪽 변
		RowOrigin.X = Origin.X + EffSize.X - 1;  RowSize = FIntPoint(1, EffSize.Y);
		LineOrigin.X = Origin.X + EffSize.X;     LineSize = FIntPoint(1, EffSize.Y);
		break;
	case 3: // 북쪽 변
		RowOrigin.Y = Origin.Y + EffSize.Y - 1;  RowSize = FIntPoint(EffSize.X, 1);
		LineOrigin.Y = Origin.Y + EffSize.Y;     LineSize = FIntPoint(EffSize.X, 1);
		break;
	}

	// F3.5 ③: 이웃 엣지 스냅 — 낮은 끝이 이웃 판과 같은 단(턱 0), 높은 끝은 +100이라 높은 쪽
	// 이웃과도 자동 일치(단 간격 고정). stale 이웃은 그리드 조회의 weak 검증이 거름.
	const float SnapGridOriginZ = Grid.GetActorLocation().Z + Thickness;
	float NeighborZ = 0.0f;
	int32 ContactCells = 0;
	if (Grid.OJJ_GetDominantFoundationSurfaceZInRect(LineOrigin, LineSize, SnapGridOriginZ, NeighborZ, ContactCells))
	{
		if (OutHeightSource)
		{
			*OutHeightSource = FString::Printf(TEXT("상속-엣지(이웃 접촉 %d셀)"), ContactCells);
		}
		return NeighborZ - Thickness - Grid.GetActorLocation().Z;
	}

	// 씨앗 폴백(고립 램프 = 지형 오르기): 낮은 끝 행 지형 스냅(F3-2 Codex ② — 풋프린트 전체 max를
	// 쓰면 단차 지형에서 낮은 끝이 떠 턱 0이 깨짐). 중간 행 지형 묻힘 가능성은 PIE 관찰 항목.
	if (OutHeightSource)
	{
		*OutHeightSource = TEXT("씨앗(지형, 낮은 끝 행)");
	}
	return Grid.OJJ_ComputeFoundationSnapLift(RowOrigin, RowSize, Thickness);
}

void AOJJ_RampFoundation::UpdateSlabVisual()
{
	// 베이스 슬래브(단일 박스)는 램프 형상과 안 맞음 — 숨기고 충돌도 끔(계단 ISM이 전담).
	if (SlabMesh)
	{
		SlabMesh->SetVisibility(false);
		SlabMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (!StepMeshISM)
	{
		return;
	}

	const float CellSize = OJJ_ResolveCellSize();
	const int32 R = FMath::Max(1, FoundationSize.X);
	const int32 Cols = FMath::Max(1, FoundationSize.Y);
	const float SlabThickness = FMath::Max(1.0f, Thickness);

	// 로컬 공간 계단: 액터 원점 = 풋프린트 중심, 액터 Z + Thickness = Z_low(낮은 단 상면).
	// 행 r 박스 상면(로컬) = Thickness + (r/(R−1))×100 — 등록 SurfaceZ와 동일 산식(시각=데이터 단일원).
	// 두께는 Thickness 고정(행 사이 측면 갭은 F2 결정 ⑥ 프로토 수용과 동일 정책).
	StepMeshISM->ClearInstances();
	for (int32 r = 0; r < R; ++r)
	{
		const float TopLocalZ = SlabThickness
			+ (R > 1 ? ((float)r / (float)(R - 1)) * AOJJ_Grid::OJJ_FoundationSnapStep : 0.0f);
		const FVector StepLocation(
			(r + 0.5f) * CellSize - R * CellSize * 0.5f, 0.0f, TopLocalZ - SlabThickness * 0.5f);
		const FVector StepScale(CellSize / 100.0f, Cols * CellSize / 100.0f, SlabThickness / 100.0f);
		StepMeshISM->AddInstance(FTransform(FRotator::ZeroRotator, StepLocation, StepScale));
	}
}
