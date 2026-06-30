// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OJJ_BuildCamera.generated.h"

class USpringArmComponent;
class UCameraComponent;
class USpotLightComponent;
class APlayerController;

/**
 * 빌드모드 전용 탑다운 카메라 액터 (3단계).
 *
 * 레벨에 배치되어 그리드 중심을 내려다본다. AOJJ_Player가 빌드모드 진입 시
 * SetViewTargetWithBlend로 이 액터를 뷰타겟으로 전환하고(플레이어는 숨김),
 * 빌드모드에서 WASD/QE 입력을 Pan/Rotate로 위임받아 카메라를 움직인다.
 *
 * possess 하지 않으므로 입력은 플레이어가 받아 이 액터의 Pan/Rotate를 호출한다.
 * Pan/Rotate는 호출 시점의 DeltaSeconds × 속도를 적용하므로 자체 Tick 불필요.
 *
 * 패닝은 "카메라 yaw 기준 상대 이동" — Q/E로 회전하면 WASD 방향도 함께 돈다.
 */
UCLASS()
class WANTED_FACTORY_API AOJJ_BuildCamera : public AActor
{
	GENERATED_BODY()

public:
	AOJJ_BuildCamera();

	// WASD 패닝. Axis.X=좌우(D/A), Axis.Y=전후(W/S). 카메라 yaw 기준 상대 평면 이동.
	void Pan(const FVector2D& Axis);

	// Q/E 회전. Axis +1=시계, -1=반시계(에디터 매핑에 따름). yaw만 회전.
	void Rotate(float Axis);

	// 마우스 휠 줌. ScrollDelta +면 줌인(팔 길이 감소). MinArmLength~MaxArmLength로 클램프.
	// 빌드모드에서 AOJJ_Player::Zoom()이 위임 호출(TPS/빌드 동일 휠 UX).
	void Zoom(float ScrollDelta);

	// [빌드 작업등] 탑다운 빌드모드에서 L키 작업등 on/off. 플레이어가 매 틱 현재 상태를 위임한다.
	// 플레이어 NightSpotLight는 탑다운에서 SetActorHiddenInGame으로 렌더가 차단되므로, 탑다운 전용으로
	// 카메라에 부착된 하향 SpotLight를 따로 켠다(밤 분위기 유지 위해 은은하게).
	void SetWorkLightEnabled(bool bEnabled);

protected:
	virtual void BeginPlay() override;

	// [높은 메시 위 자동 상승] 매 틱 Root Z를 TargetRootZ로 보간(올라갈 땐 빠르게/내려올 땐 천천히).
	// 오버랩 재계산은 throttle하지만 보간은 매 틱 돌려 출렁임 없이 부드럽게 한다.
	virtual void Tick(float DeltaSeconds) override;

	// 빌드모드 진입 시 플레이어가 SetActorLocation(그리드 평면 Z)을 세팅한 직후 호출된다.
	// 그 Z를 기준 지면 높이(BaselineGroundZ)로 캡처한다. 진입/이탈 전환 로직은 건드리지 않고 수동적으로 읽기만 한다.
	virtual void BecomeViewTarget(APlayerController* PC) override;

	// 빌드모드 이탈/TPS 전환 시 호출 — Z 자동 조정을 비활성화(불필요한 오버랩/보간 차단).
	virtual void EndViewTarget(APlayerController* PC) override;

	// [높은 메시 위 자동 상승] 카메라가 보는 XY 영역을 박스 오버랩해 영역 내 스태틱메시 최고점 위로 TargetRootZ를 산출.
	void RecomputeTargetRootZ();

	// --- Components ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	// [빌드 작업등] 카메라에 부착돼 아래(빌드 영역)를 비추는 하향 SpotLight. 기본 off, L키로 토글.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BuildCamera|WorkLight")
	TObjectPtr<USpotLightComponent> WorkLight;

	// --- Tuning ---
	// 초당 패닝 이동 속도(언리얼 단위/초).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildCamera", meta = (ClampMin = "0.0"))
	float PanSpeed = 1500.f;

	// 초당 회전 속도(도/초).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildCamera", meta = (ClampMin = "0.0"))
	float RotateSpeed = 90.f;

	// 카메라 하향 각도(도). 음수가 아래를 봄. -90이 정탑다운(바닥과 수평으로 내려다봄), 기울이면 입체감.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildCamera", meta = (ClampMin = "-90.0", ClampMax = "0.0"))
	float CameraPitch = -90.f;

	// SpringArm 길이(카메라 높이/거리). 휠 줌이 이 값을 변경(런타임 현재값 역할도 겸함).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildCamera", meta = (ClampMin = "0.0"))
	float ArmLength = 1500.f;

	// 휠 한 틱당 팔 길이 변화량. 빌드캠은 스케일이 커서 TPS(50)보다 크게.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildCamera|Zoom", meta = (ClampMin = "0.0"))
	float ZoomStep = 150.f;

	// 줌 최소/최대 팔 길이. 너무 가까우면 그리드 일부만, 너무 멀면 전경 상실 — PIE 튜닝.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildCamera|Zoom", meta = (ClampMin = "0.0"))
	float MinArmLength = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildCamera|Zoom", meta = (ClampMin = "0.0"))
	float MaxArmLength = 4000.f;

	// [빌드 작업등] 하향 SpotLight 튜닝. 밤 분위기를 유지하도록 과하지 않게(은은하게) 기본값을 잡는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildCamera|WorkLight", meta = (ClampMin = "0.0"))
	float WorkLightIntensity = 50.f;

	// 카메라 높이(최대 줌아웃 ArmLength)에서도 지면까지 닿도록 넉넉히. 비물리 falloff라 반경이 곧 도달거리.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildCamera|WorkLight", meta = (ClampMin = "0.0"))
	float WorkLightAttenuationRadius = 6000.f;

	// 빌드 영역을 넓게 덮도록 콘 각을 크게. 너무 좁으면 스폿이 점처럼 보임.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildCamera|WorkLight", meta = (ClampMin = "0.0", ClampMax = "80.0"))
	float WorkLightInnerCone = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildCamera|WorkLight", meta = (ClampMin = "0.0", ClampMax = "80.0"))
	float WorkLightOuterCone = 55.f;

	// ───────────────────────────────────────────────────────────────────────────
	// [높은 메시 위 자동 상승] 카메라가 보는 영역 내 키 큰 메시 위로 Root Z를 부드럽게 올려 메시 뚫림(클리핑) 방지.
	// 정탑다운이라 보는 영역 = 카메라 XY 중심 ± HalfExtentXY(= ArmLength * ViewExtentFactor).
	// ───────────────────────────────────────────────────────────────────────────

	// 보는 영역 반경 / ArmLength 비율. 클수록 화면 가장자리 메시까지 커버하지만 거대 배경에 과반응할 수 있음.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OJJ|BuildCamera", meta = (ClampMin = "0.0"))
	float ViewExtentFactor = 0.7f;

	// 최고 메시 위로 띄울 여유 높이(uu). 카메라가 메시 상단에 이 거리만큼 떠야 클리핑이 사라짐.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OJJ|BuildCamera", meta = (ClampMin = "0.0"))
	float MeshClearance = 300.f;

	// Root Z가 올라갈 때 보간 속도(빠르게 — 메시에 닿기 전 신속히 회피).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OJJ|BuildCamera", meta = (ClampMin = "0.0"))
	float RiseInterpSpeed = 8.f;

	// Root Z가 내려올 때 보간 속도(천천히 — 메시 경계서 들락날락할 때 출렁임 감소).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OJJ|BuildCamera", meta = (ClampMin = "0.0"))
	float FallInterpSpeed = 3.f;

	// 박스 오버랩 재계산 간격(프레임). 입력(Pan/Zoom)이 있을 때 이 간격마다만 재계산해 부하를 낮춘다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OJJ|BuildCamera", meta = (ClampMin = "1"))
	int32 OverlapThrottleFrames = 4;

	// 오버랩 박스의 Z 반높이(uu). XY로만 영역을 한정하고 Z는 넉넉히 잡아 어떤 높이의 메시도 포함.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OJJ|BuildCamera", meta = (ClampMin = "0.0"))
	float OverlapBoxHalfHeightZ = 100000.f;

	// [배경 과반응 차단] 액터 바운드 높이(2*Extent.Z)가 이 값을 넘으면 거대 배경(산/벽 등)으로 간주해 최고점 계산에서 제외.
	// 빌드 구조물 최대 높이 + 여유로 잡는다. 배경 메시(48m+)는 잘리고 일반 구조물(수 m)만 카메라 상승 대상이 됨.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OJJ|BuildCamera", meta = (ClampMin = "0.0"))
	float MaxConsiderMeshHeight = 2000.f;

	// --- Runtime (Z 자동 조정 상태) ---
	// 진입 시 캡처한 기준 지면 높이(그리드 평면 Z). 영역에 키 큰 메시가 없으면 이 높이로 복귀.
	UPROPERTY(Transient)
	float BaselineGroundZ = 0.f;

	// 보간 목표 Root Z. 오버랩 재계산이 갱신, Tick 보간이 추종.
	UPROPERTY(Transient)
	float TargetRootZ = 0.f;

	// 현재 이 카메라가 빌드 뷰타겟인지. true일 때만 Z 자동 조정 동작.
	UPROPERTY(Transient)
	bool bIsActiveBuildView = false;

	// Pan/Zoom 등으로 보는 영역이 바뀌었는지 — true일 때만 throttle 간격마다 오버랩 재계산.
	UPROPERTY(Transient)
	bool bViewChanged = false;

	// 마지막 오버랩 재계산 이후 경과 프레임 수(throttle 카운터).
	UPROPERTY(Transient)
	int32 FramesSinceOverlap = 0;
};
