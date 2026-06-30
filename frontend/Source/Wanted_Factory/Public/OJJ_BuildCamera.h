// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OJJ_BuildCamera.generated.h"

class USpringArmComponent;
class UCameraComponent;
class USpotLightComponent;

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
	// 줌아웃 범위(600~15000)가 넓어 스텝도 비례 상향 — 안 그러면 끝까지 줌아웃이 너무 잘게 쪼개져 느림.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildCamera|Zoom", meta = (ClampMin = "0.0"))
	float ZoomStep = 600.f;

	// 줌 최소/최대 팔 길이. 너무 가까우면 그리드 일부만, 너무 멀면 전경 상실 — PIE 튜닝.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildCamera|Zoom", meta = (ClampMin = "0.0"))
	float MinArmLength = 600.f;

	// 맵(지형) 전체가 들어오도록 줌아웃 상한을 크게(정탑다운 FOV90 기준 ~300m 폭). 멀면 EditAnywhere로 줄여 튜닝.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildCamera|Zoom", meta = (ClampMin = "0.0"))
	float MaxArmLength = 15000.f;

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
};
