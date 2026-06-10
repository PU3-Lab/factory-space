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
class AMachineBase;
class UUI_MachineInteract;
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

	// 폰 파괴/언포제스 시 열려 있던 머신 상호작용 위젯·입력모드를 정리(컨트롤러 무효 시 위젯 제거만).
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// --- UI ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UUserWidget> BuildModeWidgetClass;
	UPROPERTY()
	UUserWidget* BuildModeWidgetInstance;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UUserWidget> MainHUDWidgetClass;
	UPROPERTY()
	UUserWidget* MainHUDWidgetInstance;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UUI_Inventory> InventoryWidgetClass;
	UPROPERTY()
	class UUI_Inventory* InventoryWidgetInstance;
	FTimerHandle InventoryRefreshTimerHandle;
	bool bIsInventoryOpen = false;
	void TriggerInventoryToggle();
	void UpdateInventoryRealtime();

	// 머신 상호작용(F) 위젯 클래스. BP에서 WBP_MachineInteract(UUI_MachineInteract 자식)만 지정 가능하도록 타입 고정.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UUI_MachineInteract> MachineInteractWidgetClass;

	// 현재 열린 머신 상호작용 위젯. 위젯이 BTN_Close로 스스로 닫힐 수 있어 weak로 추적(소유 X).
	UPROPERTY(Transient)
	TWeakObjectPtr<UUI_MachineInteract> MachineInteractWidgetInstance;

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

	// 마우스 상하(pitch) 반전. true면 마우스 올림→시점 내려감(invert Y). 기본 반전 ON.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Look")
	bool bInvertLookPitch = true;

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

	// 머신 상호작용 토글(F키). IMC_Player에 매핑 → 일반 이동 중에만 동작(빌드모드에선 무시).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_Interact;

	// 스프린트(Shift). IMC_Player에 매핑 → 일반 이동에서만 동작. 누름=달리기/뗌=걷기.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_Sprint;

	// 빌드모드 토글(B키). 레벨의 BuildController로 위임.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_Build;

	// 빌드모드 머신 배치(좌클릭). 빌드모드 밖에서는 BuildController 내부 가드로 no-op.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_BuildPlace;

	// 빌드모드 카메라 패닝(WASD, 2D Axis). IMC_Build에만 매핑 → 빌드모드에서만 동작.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_BuildPan;

	// 빌드모드 카메라 회전(Q/E, 1D Axis: E=+1, Q=-1). IMC_Build에만 매핑.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_BuildRotate;

	// 빌드모드 호버 머신 회전(R, Digital). 카메라 회전(Q/E)과 별개. IMC_Build에만 매핑.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_MachineRotate;

	// 빌드모드 배치 모드 전환 — 머신 모드(예: 1키). IMC_Build에 매핑. 에셋 연결은 에디터 작업.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_SetMachineMode;

	// 빌드모드 배치 모드 전환 — 컨베이어 모드(예: 2키). IMC_Build에 매핑. 에셋 연결은 에디터 작업.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_SetConveyorMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_SetPowerNodeMode;

	// 빌드모드 배치 모드 전환 — 차폐장(예: 8키). IMC_Build에 매핑. 에셋 연결은 에디터 작업.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_SetShieldMode;

	// 빌드모드 배치 모드 전환 — 전선 드래그(예: - 키). IMC_Build에 매핑. 에셋 연결은 에디터 작업.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_SetPowerLineMode;

	// 빌드모드 배치 모드 전환 — 발전소(예: 7키). IMC_Build에 매핑. 에셋 연결은 에디터 작업.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_SetPowerPlantMode;

	// 빌드모드 배치 모드 전환 — 그라인더(예: 2키). IMC_Build에 매핑. 에셋 연결은 에디터 작업.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_SetGrinderMode;

	// 빌드모드 배치 모드 전환 — 채굴기(예: 5키). IMC_Build에 매핑. 에셋 연결은 에디터 작업.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_SetMinerMode;

	// 빌드모드 배치 모드 전환 — 펌프(예: 6키). IMC_Build에 매핑. 에셋 연결은 에디터 작업.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_SetPumpMode;

	// 빌드모드 배치 모드 전환 — 스멜터(예: 3키 — 빈 키 가정, IMC_Build에서 확정). 에셋 연결은 에디터 작업.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_SetSmelterMode;

	// 빌드모드 배치 모드 전환 — 창고(1키, generic Machine 진입 키 대체). IMC_Build에서 1번을 이 IA로 재매핑(에디터).
	// generic IA_SetMachineMode/SetMachineMode는 코드에 그대로 보존(진입 키만 교체) — 스멜터 때와 동일 방식.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_SetWarehouseMode;

	// 빌드모드 배치 모드 전환 — 철거(X키). IMC_Build에서 X에 매핑(에디터). 좌클릭으로 호버 대상(머신/컨베이어) 제거.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_SetDemolishMode;

	// --- 이동 속도 ---
	// 평상시 걷기 속도. BeginPlay에서 MaxWalkSpeed의 권위 있는 초기값으로 적용(BP CharacterMovement 기본값 덮음).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0.0"))
	float WalkSpeed = 250.f;

	// 스프린트(Shift) 중 속도.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0.0"))
	float SprintSpeed = 600.f;

	// --- 상호작용(Interact) ---
	// 카메라 전방 머신 상호작용 트레이스 최대 거리(uu). 빌드모드 호버와 동일 채널(ECC_Visibility).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact", meta = (ClampMin = "0.0"))
	float MaxInteractDistance = 500.f;

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
	void StartJumpAction(const FInputActionValue& Value);
	void ToggleBuild(const FInputActionValue& Value);
	void BuildPlace(const FInputActionValue& Value);
	void BuildPan(const FInputActionValue& Value);
	void BuildRotate(const FInputActionValue& Value);
	void BuildRotateMachine(const FInputActionValue& Value);
	void ConnectFactoryAgentClient();
	void SendOperatorGuideRequest();
	void TriggerHUDQuestRequest();
	void TriggerHUDQuestWindowToggle();
	void TriggerHUDAIGuideToggle();
	// 머신 상호작용(F) — 로컬 전용. 카메라 트레이스로 머신을 찾아 UI_MachineInteract를 토글한다.
	// 빌드모드 중에는 무시(상호배제). 이미 열려 있으면 닫고, 아니면 새로 생성·표시.
	void OnInteract(const FInputActionValue& Value);

	// 머신 상호작용 위젯을 닫고 입력모드/커서를 게임 전용으로 복원. weak 추적 인스턴스 정리.
	void CloseMachineInteractWidget(class APlayerController* PC);

	// 위젯 OnClosed 델리게이트 구독 핸들러 — 위젯의 모든 닫힘 경로(특히 자체 BTN_Close)에서
	// 입력모드/커서를 즉시 복원. 멱등이며, 새 위젯이 이미 열려 있으면(이전 위젯의 지연 Destruct
	// 브로드캐스트일 수 있어) no-op으로 살아있는 위젯 상태를 보호한다. AddDynamic 대상이라 UFUNCTION 필수.
	UFUNCTION()
	void RestoreGameInputMode();

	// 좌클릭 뗌/취소 — 컨베이어 드래그 커밋/취소를 BuildController로 위임.
	void BuildPlaceReleased(const FInputActionValue& Value);
	void BuildPlaceCanceled(const FInputActionValue& Value);

	// 배치 모드 전환 — BuildController->SetPlacementMode로 위임.
	void SetMachineMode(const FInputActionValue& Value);
	void SetConveyorMode(const FInputActionValue& Value);
	void SetPowerNodeMode(const FInputActionValue& Value);
	void SetShieldMode(const FInputActionValue& Value);
	void SetPowerLineMode(const FInputActionValue& Value);
	void SetPowerPlantMode(const FInputActionValue& Value);
	void SetGrinderMode(const FInputActionValue& Value);
	void SetMinerMode(const FInputActionValue& Value);
	void SetPumpMode(const FInputActionValue& Value);
	void SetSmelterMode(const FInputActionValue& Value);
	void SetWarehouseMode(const FInputActionValue& Value);
	void SetDemolishMode(const FInputActionValue& Value);

	// Foundation(기초) 모드 진입(G키 — M/J/I와 같은 BindKey 직접 바인딩이라 무인자. 정식 IA 전환은 백로그).
	void SetFoundationMode();

	// 스프린트 — Started=달리기 속도, Completed=걷기 속도로 복귀.
	void StartSprint(const FInputActionValue& Value);
	void StopSprint(const FInputActionValue& Value);

	// 빌드모드 상태에 맞춰 카메라 뷰타겟/플레이어 가시성을 전환. BuildController가 단일 진실원이므로
	// ToggleBuild에서 IsInBuildMode() 결과(bEntering)를 받아 호출한다. (3b에서 IMC 교체 추가 예정)
	void ApplyBuildModeView(bool bEntering);
};
