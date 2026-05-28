// Fill out your copyright notice in the Description page of Project Settings.


#include "OJJ_BuildController.h"

#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "MachineBase.h"
#include "OJJ_Grid.h"

AOJJ_BuildController::AOJJ_BuildController()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AOJJ_BuildController::EnterBuildMode()
{
	if (bIsBuildMode)
	{
		return;
	}

	if (!TargetGrid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] TargetGrid 미설정 — EnterBuildMode 중단"));
		return;
	}

	if (!MachineClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] MachineClass 미설정 — EnterBuildMode 중단"));
		return;
	}

	TargetGrid->SetVisualizationVisible(true);

	bIsBuildMode = true;

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->bShowMouseCursor = true;
		PC->bEnableClickEvents = true;
		PC->bEnableMouseOverEvents = true;
	}

	// 첫 UpdateMouseHover 호출이 무조건 갱신을 트리거하도록 sentinel로 초기화
	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
}

void AOJJ_BuildController::ExitBuildMode()
{
	if (!bIsBuildMode)
	{
		return;
	}

	if (TargetGrid)
	{
		TargetGrid->SetVisualizationVisible(false);
		TargetGrid->ClearHoverPreview();
	}

	bIsBuildMode = false;

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->bShowMouseCursor = false;
	}

	// 재진입 시 같은 셀에 정지해 있어도 첫 갱신이 동작하도록 sentinel로 리셋
	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
}

void AOJJ_BuildController::ToggleBuildMode()
{
	if (bIsBuildMode)
	{
		ExitBuildMode();
	}
	else
	{
		EnterBuildMode();
	}
}

FIntPoint AOJJ_BuildController::ComputeOriginFromCursorCell(FIntPoint CursorCell, AMachineBase* Machine) const
{
	if (!Machine)
	{
		return CursorCell;
	}

	// AOJJ_Grid::CalculateFootprint / GetMachinePlacementLocation과 동일한 정수화 규칙.
	// 입력(cursor → origin)과 시각 보정(origin → footprint center)이 반대 방향이지만
	// 같은 size 가정에서 동작해야 호버/배치와 occupancy/메시 위치가 어긋나지 않음.
	const FVector2D Size = Machine->GetMachineSize();
	const int32 SizeX = FMath::Max(1, FMath::CeilToInt(Size.X));
	const int32 SizeY = FMath::Max(1, FMath::CeilToInt(Size.Y));

	// (Size-1)/2 정수 나눗셈 → lower-left bias. 1x1 offset 0 (회귀 없음).
	return FIntPoint(CursorCell.X - (SizeX - 1) / 2, CursorCell.Y - (SizeY - 1) / 2);
}

void AOJJ_BuildController::UpdateMouseHover()
{
	if (!bIsBuildMode)
	{
		return;
	}

	if (!TargetGrid || !MachineClass)
	{
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return;
	}

	FHitResult Hit;
	const bool bHit = PC->GetHitResultUnderCursorByChannel(
		UEngineTypes::ConvertToTraceType(ECC_Visibility),
		/*bTraceComplex=*/ false,
		Hit);

	if (!bHit)
	{
		// 트레이스 실패 → stale 미리보기/캐시가 다음 클릭에 잘못 적용되지 않도록 명시적 리셋
		TargetGrid->ClearHoverPreview();
		CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
		return;
	}

	// floor 또는 이미 배치된 머신 위에서 hover를 유지.
	// 머신 Cube mesh가 Visibility 채널을 Block해서 trace를 가로채도, 머신 위 XY는
	// 점유된 셀에 정확히 매핑되므로 CanPlaceMachine 검증을 거치게 그대로 통과시킨다
	// → 점유 셀과 겹친 풋프린트가 빨강으로 표시됨. 그 외 표면(캐릭터/벽 등)은
	// off-grid이므로 기존처럼 ClearHoverPreview로 차단.
	UPrimitiveComponent* HitComp = Hit.GetComponent();
	AActor* HitActor = Hit.GetActor();
	const bool bHitFloor = (HitComp == TargetGrid->GetGridFloorMesh());
	const bool bHitMachine = HitActor && HitActor->IsA<AMachineBase>();
	if (!bHitFloor && !bHitMachine)
	{
		TargetGrid->ClearHoverPreview();
		CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
		return;
	}

	const FIntPoint CursorCell = TargetGrid->WorldToGrid(Hit.Location);

	// Tick마다 호출되는 경로라 동일 셀이면 ISM 리빌드 스킵
	if (CursorCell == CurrentHoverCell)
	{
		return;
	}

	AMachineBase* DefaultMachine = MachineClass.GetDefaultObject();
	if (!DefaultMachine)
	{
		return;
	}

	// cursor cell → lower-left origin (마우스 = 풋프린트 중심 정책).
	// 예전엔 IsValidGridCell(cursor)로 anchor 음수/초과를 사전 차단했으나, 이 차단이
	// 왼쪽/위 경계 비대칭을 만들었음 (오른쪽/아래는 anchor가 valid한 상태에서 풋프린트가
	// +X,+Y로 새서 빨강 표시되는데, 왼쪽/위는 anchor 자체가 음수가 되어 hover 사라짐).
	// 이제 origin이 그리드 음수/초과여도 그대로 넘김 → CanPlaceMachine이 풋프린트 셀별
	// IsValidGridCell 검사로 false → UpdateHoverPreview가 풋프린트 전체 빨강 (대칭).
	const FIntPoint Origin = ComputeOriginFromCursorCell(CursorCell, DefaultMachine);

	TargetGrid->UpdateHoverPreview(DefaultMachine, Origin);
	CurrentHoverCell = CursorCell;
}

void AOJJ_BuildController::OnLeftClickPressed()
{
	// SP-only contract 강제 (헤더의 MULTIPLAYER LIMITATION 명시와 일치).
	// 클라이언트에서 호출되면 TryPlaceMachine의 HasAuthority ensure가 트리거되고
	// spawn된 머신은 orphan으로 남음 → 진입부에서 차단.
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BuildController] OnLeftClickPressed called on non-authority — SP-only contract"));
		return;
	}

	if (!bIsBuildMode)
	{
		return;
	}

	if (!TargetGrid || !MachineClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] TargetGrid 또는 MachineClass 미설정"));
		return;
	}

	// 마우스가 floor 밖이라 호버 갱신이 한 번도 안 됐으면 클릭 무시
	if (CurrentHoverCell.X == INT_MIN || CurrentHoverCell.Y == INT_MIN)
	{
		return;
	}

	AMachineBase* DefaultMachine = MachineClass.GetDefaultObject();
	if (!DefaultMachine)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] MachineClass CDO 없음"));
		return;
	}

	// cursor cell → lower-left origin — UpdateMouseHover와 같은 변환을 사용해야 호버
	// 미리보기와 실제 배치 위치가 어긋나지 않음. CanPlaceMachine이 IsValidGridCell +
	// OccupiedCells 통합 판정하므로 anchor 음수/초과도 자연 거부됨 → 사전 bounds
	// 차단(IsValidGridCell)은 더 이상 필요 없음.
	const FIntPoint Origin = ComputeOriginFromCursorCell(CurrentHoverCell, DefaultMachine);

	if (!TargetGrid->CanPlaceMachine(DefaultMachine, Origin))
	{
		UE_LOG(LogTemp, Log, TEXT("[BuildController] origin %s 배치 불가 (bounds/점유)"),
			*Origin.ToString());
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = this;

	AMachineBase* NewMachine = World->SpawnActor<AMachineBase>(
		MachineClass,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams);

	if (!NewMachine)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] SpawnActor 실패"));
		return;
	}

	FString OutReason;
	if (!TargetGrid->TryPlaceMachine(NewMachine, Origin, OutReason))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildController] TryPlaceMachine 실패: %s"), *OutReason);
		NewMachine->Destroy();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[BuildController] origin %s 머신 배치 성공"),
		*Origin.ToString());

	// 직전 origin이 이제 점유됨 → 다음 UpdateMouseHover에서 빨강으로 강제 재표시
	CurrentHoverCell = FIntPoint(INT_MIN, INT_MIN);
}
