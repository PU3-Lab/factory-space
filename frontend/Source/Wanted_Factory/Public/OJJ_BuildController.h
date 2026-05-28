// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OJJ_BuildController.generated.h"

class AOJJ_Grid;
class AMachineBase;

/**
 * 건설 모드 컨트롤러. 호버 갱신, 미리보기, 클릭 배치 라우팅을 담당.
 * PlayerController가 Tick/Input에서 UpdateMouseHover/OnLeftClickPressed로 위임 호출.
 *
 * 풋프린트 조회는 MachineClass의 CDO(GetDefaultObject)에서 GetMachineSize만 읽음.
 * 미리보기 전용으로 머신 액터를 spawn하지 않으므로 부작용(BeginPlay/tick/collision/
 * 서브클래스 게임플레이 로직)에서 자유로움.
 *
 * ⚠️ MULTIPLAYER LIMITATION: Single-player only
 *
 * This controller currently spawns and registers machines locally on the calling client.
 * AOJJ_Grid::TryPlaceMachine requires HasAuthority(), so in multiplayer:
 * - Clients calling OnLeftClickPressed will spawn a local throwaway machine
 * - The grid registration will silently fail (ensure warning + false return)
 * - Machine becomes orphaned and not visible to other players
 *
 * For multiplayer support, refactor to:
 * 1. OnLeftClickPressed → Server RPC with requested cell
 * 2. Server validates MachineClass/grid/cell
 * 3. Server spawns and registers the real AMachineBase
 * 4. Replication handles client visibility
 *
 * Tracked for future Phase (MP support).
 */
UCLASS()
class WANTED_FACTORY_API AOJJ_BuildController : public AActor
{
	GENERATED_BODY()

public:
	AOJJ_BuildController();

protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "BuildController")
	TObjectPtr<AOJJ_Grid> TargetGrid;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildController")
	TSubclassOf<AMachineBase> MachineClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BuildController")
	bool bIsBuildMode = false;

	// 마우스 아래 cursor cell (WorldToGrid 결과). 머신의 lower-left origin은
	// ComputeOriginFromCursorCell로 변환 — 짝수 머신은 lower-left bias 정책.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BuildController")
	FIntPoint CurrentHoverCell = FIntPoint::ZeroValue;

public:
	UFUNCTION(BlueprintCallable, Category = "BuildController")
	void EnterBuildMode();

	UFUNCTION(BlueprintCallable, Category = "BuildController")
	void ExitBuildMode();

	UFUNCTION(BlueprintCallable, Category = "BuildController")
	bool IsInBuildMode() const { return bIsBuildMode; }

	// PlayerController가 Tick에서 호출. 마우스 위치 → 그리드 셀 → 호버 미리보기 갱신.
	UFUNCTION(BlueprintCallable, Category = "BuildController")
	void UpdateMouseHover();

	// PlayerController가 입력에서 호출. 현재 호버 셀에 머신 배치 시도.
	UFUNCTION(BlueprintCallable, Category = "BuildController")
	void OnLeftClickPressed();

	// 빌드 모드 상태 토글. Enter/Exit의 자체 가드(이미 같은 상태면 no-op) 덕분에 안전.
	UFUNCTION(BlueprintCallable, Category = "BuildController")
	void ToggleBuildMode();

private:
	// cursor cell → lower-left origin 변환. 마우스 = 풋프린트 중심 정책.
	// (Size-1)/2 정수 나눗셈 → lower-left bias:
	//  - 1x1: offset 0 (회귀 없음)
	//  - 2x2: offset 0 → 마우스가 머신 좌하단 셀
	//  - 3x3: offset 1 → 마우스가 정중앙 셀
	//  - 4x4: offset 1 → 마우스가 중앙 좌하단 셀
	// ⚠️ 입력 방향(cursor → origin). 시각 보정인 AOJJ_Grid::GetMachinePlacementLocation
	// (origin → footprint center 액터 위치)과 반대 방향이지만 같은 size 정수화 규칙
	// (CeilToInt + Max(1))을 따라야 호버/배치와 occupancy/시각이 어긋나지 않는다.
	FIntPoint ComputeOriginFromCursorCell(FIntPoint CursorCell, AMachineBase* Machine) const;
};
