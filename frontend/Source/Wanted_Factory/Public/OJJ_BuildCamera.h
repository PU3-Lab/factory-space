// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OJJ_BuildCamera.generated.h"

class USpringArmComponent;
class UCameraComponent;

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

protected:
	virtual void BeginPlay() override;

	// --- Components ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	// --- Tuning ---
	// 초당 패닝 이동 속도(언리얼 단위/초).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildCamera", meta = (ClampMin = "0.0"))
	float PanSpeed = 1500.f;

	// 초당 회전 속도(도/초).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildCamera", meta = (ClampMin = "0.0"))
	float RotateSpeed = 90.f;

	// 카메라 하향 각도(도). 음수가 아래를 봄. -90이 정탑다운, 약간 기울이면 입체감.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildCamera", meta = (ClampMin = "-90.0", ClampMax = "0.0"))
	float CameraPitch = -70.f;

	// SpringArm 길이(카메라 높이/거리).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildCamera", meta = (ClampMin = "0.0"))
	float ArmLength = 1500.f;
};
