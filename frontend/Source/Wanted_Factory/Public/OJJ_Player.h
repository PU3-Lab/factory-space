// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "OJJ_Player.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class AOJJ_BuildController;
class AOJJ_BuildCamera;
struct FInputActionValue;

/**
 * TPS 직접조작 플레이어 캐릭터 (1단계: 이동/카메라 골격).
 *
 * 구성:
 *  - SpringArm + Camera: 마우스로 카메라 회전(bUsePawnControlRotation), 스크롤로 줌
 *  - WASD 이동: 컨트롤러 yaw 기준 전/후/좌/우, 이동 방향으로 캐릭터 회전
 *  - SPACE 점프: ACharacter 기본 Jump 사용
 *
 * 입력 에셋(IMC/IA)은 블루프린트 파생 클래스에서 할당한다.
 * 빌드모드(B키)/BuildController 연동은 2단계, 탑다운 전환은 3단계에서 추가.
 */
UCLASS()
class WANTED_FACTORY_API AOJJ_Player : public ACharacter
{
	GENERATED_BODY()

public:
	AOJJ_Player();

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	virtual void BeginPlay() override;

	// --- Components ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	// --- Camera tuning ---
	// 스크롤 줌 한 틱당 SpringArm 길이 변화량
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float ZoomStep = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float MinArmLength = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float MaxArmLength = 800.f;

	// --- Look(마우스 카메라 회전) tuning ---
	// 마우스 입력 델타에 곱해지는 회전 감도. 값이 작을수록 부드럽고 느림.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Look", meta = (ClampMin = "0.0"))
	float LookYawSensitivity = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Look", meta = (ClampMin = "0.0"))
	float LookPitchSensitivity = 0.5f;

	// 카메라 상하 회전 제한(deg). 뒤집힘 방지. BeginPlay에서 PlayerCameraManager에 적용.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Look", meta = (ClampMin = "-89.9", ClampMax = "0.0"))
	float CameraPitchMin = -80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Look", meta = (ClampMin = "0.0", ClampMax = "89.9"))
	float CameraPitchMax = 80.f;

	// --- Input assets (블루프린트에서 할당) ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputMappingContext> IMC_Player;

	// 빌드모드 전용 IMC. 빌드모드 진입 시 IMC_Player와 교체된다.
	// ⚠️ 이 IMC에는 IA_Look을 매핑하지 않아야 빌드모드에서 마우스 카메라 회전이 차단된다.
	// B(IA_Build)/좌클릭(IA_BuildPlace)은 포함해야 토글·배치가 동작.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputMappingContext> IMC_Build;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_Move;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_Look;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_Jump;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_Zoom;

	// 빌드모드 토글(B키). 레벨의 BuildController로 위임.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_Build;

	// 빌드모드 머신 배치(좌클릭). 빌드모드 밖에서는 BuildController 내부 가드로 no-op.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_BuildPlace;

	// --- Build mode 연동 ---
	// 레벨에 배치된 BuildController 인스턴스. BeginPlay에서 GetActorOfClass로 캐시(소유 X, spawn X).
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Build")
	TObjectPtr<AOJJ_BuildController> BuildController;

	// 빌드 탑다운 카메라 클래스. BeginPlay에서 이 클래스로 spawn한다(수동 레벨 배치 불필요).
	// 미설정 시 C++ 기본 AOJJ_BuildCamera 사용. BP 파생을 지정하면 PanSpeed/Pitch 등 에디터 튜닝 가능.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	TSubclassOf<AOJJ_BuildCamera> BuildCameraClass;

	// BeginPlay에서 spawn된 빌드 카메라 인스턴스. 진입 시 그리드 중심으로 자동 재배치.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Build")
	TObjectPtr<AOJJ_BuildCamera> BuildCamera;

	// 빌드모드 진입/복귀 시 카메라 뷰타겟 블렌드 시간(초).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build", meta = (ClampMin = "0.0"))
	float CameraBlendTime = 0.4f;

	// --- Input handlers ---
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Zoom(const FInputActionValue& Value);
	void ToggleBuild(const FInputActionValue& Value);
	void BuildPlace(const FInputActionValue& Value);

	// 빌드모드 상태에 맞춰 카메라 뷰타겟/플레이어 가시성을 전환. BuildController가 단일 진실원이므로
	// ToggleBuild에서 IsInBuildMode() 결과(bEntering)를 받아 호출한다. (3b에서 IMC 교체 추가 예정)
	void ApplyBuildModeView(bool bEntering);
};
