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
	TArray<float>& OutCellZs) const
{
	// R = 오르는 방향(로컬 +X) 길이. EffSize는 회전 스왑된 월드 크기라 step 0/2에선 EffSize.X,
	// 1/3에선 EffSize.Y가 R과 같다(F3-0 스왑 규칙) — r 산출은 월드 축 기준이므로 아래 switch로 흡수.
	const int32 R = FMath::Max(1, FoundationSize.X);
	if (R < 2 || EffSize.X < 1 || EffSize.Y < 1)
	{
		return false; // 1행 램프는 경사 불능 — 평판 단일값 경로 폴백.
	}

	OutCellZs.SetNumUninitialized(EffSize.X * EffSize.Y);
	const int32 Step = ((RotationSteps % 4) + 4) % 4;
	for (int32 LX = 0; LX < EffSize.X; ++LX)
	{
		for (int32 LY = 0; LY < EffSize.Y; ++LY)
		{
			// 풋프린트 로컬(월드 축) → 오르는 방향 행 r. 양 끝 정합(턱 0): r=0 → Z_low 정확,
			// r=R−1 → (R−1)/(R−1)=1이라 Z_low+100 정확(float 오차 0) — f3 계획 §보강 산식.
			int32 r = 0;
			switch (Step)
			{
			case 0: r = LX; break;                       // 로컬 +X가 월드 +X
			case 1: r = LY; break;                       // 월드 +Y
			case 2: r = (EffSize.X - 1) - LX; break;     // 월드 −X
			case 3: r = (EffSize.Y - 1) - LY; break;     // 월드 −Y
			}
			OutCellZs[LX * EffSize.Y + LY] =
				BaseSurfaceZ + ((float)r / (float)(R - 1)) * AOJJ_Grid::OJJ_FoundationSnapStep;
		}
	}
	return true;
}

float AOJJ_RampFoundation::OJJ_ComputeSnapLift(const AOJJ_Grid& Grid, FIntPoint Origin, FIntPoint EffSize,
	int32 RotationSteps) const
{
	// 낮은 끝(r=0) 행만 스냅 기준(Codex F3-2 ②): 단차 지형 위 배치(주 사용처)에서 풋프린트 전체 max를
	// 쓰면 낮은 끝이 높은 단으로 떠 양 끝 턱 0이 깨짐. 기존 그리드 헬퍼를 1행 서브렉트로 재사용 —
	// off-grid/무효 폴백(0) 규약 동일. 중간 행 지형이 보간 계단 위로 솟는 묻힘 가능성은 F3-3 PIE
	// 관찰 항목(후속 후보: 행별 묻힘 검증 거부).
	const int32 Step = ((RotationSteps % 4) + 4) % 4;
	FIntPoint RowOrigin = Origin;
	FIntPoint RowSize = EffSize;
	switch (Step)
	{
	case 0: RowSize = FIntPoint(1, EffSize.Y); break;                                            // r=0 행 = 서쪽 변
	case 1: RowSize = FIntPoint(EffSize.X, 1); break;                                            // 남쪽 변
	case 2: RowOrigin.X = Origin.X + EffSize.X - 1; RowSize = FIntPoint(1, EffSize.Y); break;    // 동쪽 변
	case 3: RowOrigin.Y = Origin.Y + EffSize.Y - 1; RowSize = FIntPoint(EffSize.X, 1); break;    // 북쪽 변
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
