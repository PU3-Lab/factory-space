// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PlayerWarehouseSubsystem.h"
#include "OJJ_Player.generated.h"

class USpringArmComponent;
class UCameraComponent;
class USpotLightComponent;
class UInputMappingContext;
class UInputAction;
class AOJJ_BuildController;
// 빌드 뷰 모드(None/TopDown/TPS) — 정의는 OJJ_BuildController.h. B/V 입력 라우팅에서 값으로 사용.
enum class EBuildViewMode : uint8;
class AOJJ_BuildCamera;
class ACameraActor;
class AOJJ_Ladder;
class AMachineBase;
class UUI_MachineInteract;
class UUI_SynthesizerInteract;
class AResourceBase;
class UAnimMontage;
class UAnimSequenceBase;
class UOJJ_CharacterAppearanceData;
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
	FORCEINLINE class UUI_Inventory* GetInventoryWidgetInstance() const { return InventoryWidgetInstance; }
	void ShowMainHUDSaveIndicator(float DisplaySeconds);

	// --- 사다리 등반 (#184, AOJJ_Ladder가 트리거에서 호출) ---
	// 등반 시작: MOVE_Flying+중력0로 전환, 현재 사다리 캐시. 이미 등반 중이면 no-op.
	void BeginClimb(AOJJ_Ladder* Ladder);
	// 등반 종료: 상단이면 step-off 보간 시작, 아니면 MOVE_Walking 복귀+중력 복원.
	void EndClimb(bool bStepOffTop);
	// 등반/step-off 상태를 즉시 청산하고 걷기로 수렴(빌드모드 진입·EndPlay·사다리 소멸 등 비정상 종료용).
	void AbortClimb();

	// [#184] 사다리 트리거 겹침 알림(감지/시작 분리) — 트리거 Begin/EndOverlap가 호출. 여기선 '근접 사다리'
	// 포인터만 갱신하고, 실제 등반 시작은 W(위) 입력에서 BeginClimb로 한다(이미 트리거 안이어도 W로 시작 가능).
	void NotifyLadderOverlap(AOJJ_Ladder* Ladder);
	void NotifyLadderEndOverlap(AOJJ_Ladder* Ladder);

	// 등반 활성 여부 — ABP_Man 스테이트머신 climbing 진입/탈출 조건용(읽기 전용 노출). 방향(위/아래)·속도는
	// 별도 변수 없이 ABP가 Velocity.Z로 판별(위>0 Loop, 아래<0 Down) — 노출 최소화(#184 사다리 애니 장착).
	UFUNCTION(BlueprintPure, Category = "Climb")
	bool IsClimbing() const { return bClimbing; }

	// [끊김① PlayRate 연동] ABP Climb Loop 노드의 PlayRate에 배선 — 실제 수직 등반 속도를 ClimbSpeed로 정규화한
	// 0~1 값. 속도 0(호버)=PlayRate 0(정지 포즈), 최대=1(정상 루프). 애니가 실제 이동에 동기화돼 발/손이 사다리단과
	// 안 미끄러지고 되감기 튐 제거. 등반 아니면 0. (매그니튜드만 — 방향은 GetClimbDirection.)
	UFUNCTION(BlueprintPure, Category = "Climb")
	float GetClimbSpeedNormalized() const;

	// [끊김① 방향 래치] ABP Loop↔Down 상태 전이용 — Velocity.Z 부호(0 근처 토글 떨림) 대신 마지막 비영 입력
	// 방향을 유지. +1=위(Loop), -1=아래(Down), 0=등반 아님. 호버(속도 0)에도 방향이 안 뒤집혀 상태 깜빡임 차단.
	UFUNCTION(BlueprintPure, Category = "Climb")
	float GetClimbDirection() const;

	// [수영] ABP_Man/Woman 스테이트머신 swim 진입/탈출 조건용(읽기 전용). 이동/idle 구분은 ABP가 Velocity로 판별
	// (수평속도>임계 → Swim_Forward, 아니면 Swim_Idle). ⚠️ public 필수 — ABP는 AOJJ_Player 서브클래스 아님(IsClimbing 전례).
	UFUNCTION(BlueprintPure, Category = "OJJ|Swim")
	bool IsSwimming() const { return bSwimming; }

	// [수영] 이동 수영 여부(속도 히스테리시스 적용). ABP가 Swim_Idle↔Swim_Forward 전환에 이 값을 쓰면
	// 코드의 오프셋(잠김 깊이) 판정과 항상 일치(자체 속도임계 사용 시 어긋남 방지).
	UFUNCTION(BlueprintPure, Category = "OJJ|Swim")
	bool IsSwimMoving() const { return bSwimMoving; }

	// [#368] ABP_Man 점프/falling 상태 진입 게이트 — ABP 전이를 raw IsFalling 대신 이 getter로 교체한다.
	// true = 실제 낙하(하강 중 + 하강속도 FallAnimVelocityThreshold 초과). 낮은 턱 짧은 낙하는 false.
	// ⚠️ public 필수(IsClimbing과 동일) — ABP_Man은 AOJJ_Player 서브클래스가 아니라 protected면 BP 호출 불가(codex P1).
	UFUNCTION(BlueprintPure, Category = "OJJ|Animation")
	bool ShouldPlayFallAnim() const;

	// [게임진입 테스트] 이름 기반 캐릭터 스왑 콘솔 명령 — PIE 콘솔에 `SetCharacter Woman` / `SetCharacter Man`.
	// EOJJ_CharacterType 리플렉션을 돌며 짧은 이름·DisplayName(대소문자 무시)으로 매칭하므로 enum에 캐릭터가 추가돼도
	// 이 명령은 자동 대응(수정 불필요). 매칭값 → SetSelectedCharacter → ApplySelectedCharacterAppearance(외형+per-character
	// 데이터 일괄 적용). 알 수 없는 이름이면 사용 가능한 값을 로그로 안내 후 무시(크래시 없음).
	UFUNCTION(Exec)
	void SetCharacter(const FString& CharacterName);

protected:
	virtual void BeginPlay() override;

	// [게임진입] 선택 서브시스템(EOJJ_CharacterType)값으로 GetMesh()의 SkeletalMesh+AnimClass를 스왑(외형만 —
	// 사다리/빌드/입력 로직 무영향, 단일 pawn 유지). AppearanceData/서브시스템/항목 미존재면 안전하게 스킵.
	void ApplySelectedCharacterAppearance();

	// step-off 부드러운 안착 보간(#184) 처리. 평상시엔 별 비용 없음(bSteppingOff 가드).
	virtual void Tick(float DeltaSeconds) override;

	// 폰 파괴/언포제스 시 열려 있던 머신 상호작용 위젯·입력모드를 정리(컨트롤러 무효 시 위젯 제거만).
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// [#357] 착지 콜백 — 점프 슬롯 애니(ActiveJumpMontage)가 아직 재생 중이면 즉시 끊고 locomotion 복귀.
	// 단일 시퀀스 Man_Jump가 착지까지 통짜로 나가 착지 후 서서 미끄러지는 잔상을 제거(LadderFinish의
	// StopAnimMontage 선례 미러). Super 호출로 기본 착지 처리/OnLanded BP 이벤트 보존.
	virtual void Landed(const FHitResult& Hit) override;

	// [#357] 점프 슬롯 애니(ActiveJumpMontage) 정지 헬퍼 — Landed(착지)와 BeginClimb(사다리 진입, Landed 미경유)
	// 양쪽에서 호출해 점프 포즈가 다음 동작을 덮지 않게 한다(codex P2). 미재생이면 no-op.
	void StopJumpMontage();

	// --- UI ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UUserWidget> BuildModeWidgetClass;
	UPROPERTY()
	UUserWidget* BuildModeWidgetInstance;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UUserWidget> MainHUDWidgetClass;
	UPROPERTY()
	UUserWidget* MainHUDWidgetInstance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Resource Nameplate", meta = (ClampMin = "0.0"))
	float ResourceNameplateTraceDistance = 2000.0f;

	TWeakObjectPtr<AResourceBase> FocusedNameplateResource;

	void UpdateResourceNameplate();
	AResourceBase* TraceFocusedOre(APlayerController* PlayerController) const;
	void HideResourceNameplate();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Warehouse")
	TArray<FWarehouseItemStack> InitialWarehouseItems;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UUI_Inventory> InventoryWidgetClass;
	UPROPERTY()
	class UUI_Inventory* InventoryWidgetInstance;
	FTimerHandle InventoryRefreshTimerHandle;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UUI_WarehouseInteract> WarehouseInteractWidgetClass;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UUserWidget> SynthesizerInteractWidgetClass;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UUserWidget> MoldingMachineInteractWidgetClass;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UUI_BaseCampInteract> BaseCampInteractWidgetClass;
	
	UPROPERTY()
	class UUI_BaseCampInteract* BaseCampInteractWidgetInstance;
	
	UPROPERTY()
	UUserWidget* MoldingMachineInteractWidgetInstance;
	
	UPROPERTY()
	class UUI_WarehouseInteract* WarehouseInteractWidgetInstance;
	
	UPROPERTY()
	class UUI_SynthesizerInteract* SynthesizerInteractWidgetInstance;
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

	// [#405 일부] TPS 빌드 1인칭 토글 상태. ArmLength 0 + GetMesh() OwnerNoSee 방식(머리 본 흔들림이
	// 빌드 정밀 배치를 방해해 HeadSocket 부착에서 롤백). 진입 직전 ArmLength를 저장(줌 반영)해 복귀 시 복원.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	bool bFirstPersonBuild = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	float SavedBuildArmLength = 270.0f;

	// 야간(18시~06시) 전방 시야 확보용 스포트라이트. 플레이어 앞에 붙어 밤에만 켠다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Light")
	TObjectPtr<USpotLightComponent> NightSpotLight;

	// --- Character appearance (게임진입) ---
	// 캐릭터 종류 → 외형(메시+ABP) 매핑 DataAsset. BeginPlay에서 선택 서브시스템값으로 GetMesh()를 스왑한다.
	// 미할당이면 스왑 스킵(BP 기본 메시 유지) — 게임진입 흐름 미완성 단계에서도 안전. JJ가 BP_OJJ_Player에 DA 할당.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	TObjectPtr<UOJJ_CharacterAppearanceData> AppearanceData;

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

	// 탑다운 빌드모드 전용 IMC. 탑다운 빌드 진입 시 IMC_Player와 교체된다.
	// ⚠️ 이 IMC에는 IA_Look을 매핑하지 않아야 빌드모드에서 마우스 카메라 회전이 차단된다.
	// B(IA_Build)/좌클릭(IA_BuildPlace)은 포함해야 토글·배치가 동작.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputMappingContext> IMC_Build;

	// [TPS 빌드] 3인칭 빌드모드 전용 IMC. TPS 빌드 진입 시 IMC_Player와 교체된다.
	// ⚠️ IMC_Build와 달리 IA_Look을 포함(3인칭 카메라 회전 필요) + 일반 이동(IA_Move 등)도 포함(걸으며 빌드).
	//    빌드 입력(IA_BuildPlace/IA_MachineRotate/카테고리 숫자키/IA_SetDemolishMode/IA_Build/IA_BuildTPS)도 포함.
	// ⚠️ 미할당 시 ApplyBuildInputContext가 IMC_Player 제거를 막아(입력 먹통 방지) TPS 진입은 되지만 입력은 IMC_Player 유지.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputMappingContext> IMC_BuildTPS;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_Move;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_Look;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_Jump;

	// [#357] 점프 도약 애니(예: Man_Jump 시퀀스). 점프 입력 순간(StartJumpAction) DefaultSlot로 즉시 재생해
	// "떠오른 뒤 늦게 재생"(ABP 스테이트머신이 IsFalling로 잡아 한 박자 늦음)을 해소 — LadderFinishMontage가
	// "도착 순간 재생은 늦음"을 미리 트리거로 푼 것과 같은 슬롯 패턴. 스테이트머신 무수정(슬롯이 출력을 덮음).
	// 시퀀스를 직접 받아 PlaySlotAnimationAsDynamicMontage로 재생 → 새 몽타주 에셋 불요(BP_OJJ_Player에서 Man_Jump 할당).
	// 미할당(nullptr)이면 슬롯 재생 스킵(기존 동작 — 회귀 0). ⚠️ ABP AnimGraph에 'DefaultSlot' 노드 필요(LadderFinish 공용).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OJJ|Animation")
	TObjectPtr<UAnimSequenceBase> JumpAnim = nullptr;

	// [#357] 점프 슬롯 블렌드 인/아웃(초). 도약은 스냅하게(짧은 인), 공중 전환은 부드럽게(아웃). PIE 다이얼.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OJJ|Animation", meta = (ClampMin = "0.0"))
	float JumpAnimBlendInTime = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OJJ|Animation", meta = (ClampMin = "0.0"))
	float JumpAnimBlendOutTime = 0.2f;

	// [#357] Man_Jump 재생 시작 위치(초) — 무릎 구부림 준비동작 구간을 건너뛰고 "도약 시작" 시점부터 재생.
	// 단일 시퀀스라 처음부터 틀면 캐릭터는 이미 뜨는 중인데 애니는 웅크림부터 → 2단 점프 느낌. 기본값 0.3 =
	// PIE 다이얼 확정값(2026-06-24). BP override 대신 C++ 단일 출처(멀티플레이 silent fail 방지) — 재튜닝 시 여기서.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OJJ|Animation", meta = (ClampMin = "0.0"))
	float JumpAnimStartPosition = 0.3f;

	// [#368] falling 애니 진입 하강속도 임계(uu/s, 양수). raw IsFalling만으론 낮은 턱 내려갈 때도 잠깐 true라
	// ABP가 점프/falling 포즈로 진입한다 → 이보다 빠르게 하강(Velocity.Z < -이값)할 때만 진짜 낙하로 본다.
	// 낮은 턱(짧은 낙하)은 착지 전 속도가 작아 미만 → 진입 안 함. 기본 400 = PIE 확정값(2026-06-24).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OJJ|Animation", meta = (ClampMin = "0.0"))
	float FallAnimVelocityThreshold = 400.f;

	// [#357] 입력 시 PlaySlotAnimationAsDynamicMontage가 만든 점프 슬롯 몽타주 핸들(런타임 전용) — Landed에서
	// 이 몽타주만 StopAnimMontage로 끊어 착지 잔상 제거(다른 몽타주 영향 0). 매 점프 갱신, 종료 후 stale은 no-op.
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveJumpMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_Zoom;

	// 머신 상호작용 토글(F키). IMC_Player에 매핑 → 일반 이동 중에만 동작(빌드모드에선 무시).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_Interact;

	// 스프린트(Shift). IMC_Player에 매핑 → 일반 이동에서만 동작. 누름=달리기/뗌=걷기.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_Sprint;

	// 빌드모드 토글(B키) = 탑다운 빌드. 레벨의 BuildController로 위임.
	// 규칙: None→TopDown, TopDown→None, TPS→TopDown(전환). HandleBuildModeKey(TopDown).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_Build;

	// [TPS 빌드] 빌드모드 토글(V키) = 3인칭 빌드. IA_Build와 동일 패턴.
	// 규칙: None→TPS, TPS→None, TopDown→TPS(전환). HandleBuildModeKey(TPS).
	// ⚠️ IA_BuildTPS .uasset(V 매핑)은 에디터에서 생성 후 BP_OJJ_Player에 할당해야 동작(미할당 시 V 무반응 + 경고 로그).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_BuildTPS;

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

	// [옛 빌드 입력 경로 전수 정리] 배치 모드별 직행 IA(Machine/Conveyor/Pipe/Tank/PowerNode/Shield/
	// PowerLine/PowerPlant/Grinder/Miner/Pump/Smelter/Warehouse)는 카테고리 숫자키 슬롯(UI_BuildModeMain)이
	// 완전 대체하여 C++에서 폐기. 퀘스트 이벤트는 슬롯 경로(ExecutePlacementMode)가 동일/정합 문자열로 발사
	// (QuestManagerSubsystem 리스너 기준 — #306/#308 검증). IA .uasset·IMC 매핑은 미변경(에디터 잔여는 UI 담당 후속).
	// 콘솔 SetBuildMode(pipe/tank/tower)는 SetPlacementMode 직접 호출이라 계속 동작.

	// 빌드모드 배치 모드 전환 — 철거(X키). IMC_Build에서 X에 매핑(에디터). 좌클릭으로 호버 대상(머신/컨베이어) 제거.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_SetDemolishMode;

	// [공중 Foundation] 빌드 높이 층 증감(1D Axis: +1=올림/-1=내림). Foundation(Flat)에서 Z 층 올림/내림.
	// ⚠️ 양쪽 IMC 매핑(에디터): IMC_BuildTPS=Q(+1)/E... 실제는 E=+1/Q=Negate, IMC_Build(탑다운)=↑(+1)/↓(Negate).
	//    탑다운은 Q/E가 IA_BuildRotate(카메라)라 화살표 ↑↓ 사용. TPS·탑다운 동일 IA·핸들러·BuildHeightOffset 공유(전환 시 높이 유지).
	// 신규 .uasset(Value Type = Axis1D). 미할당 시 무반응 + 경고 로그.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> IA_BuildHeight;

	// --- 이동 속도 ---
	// 평상시 걷기 속도. BeginPlay에서 MaxWalkSpeed의 권위 있는 초기값으로 적용(BP CharacterMovement 기본값 덮음).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0.0"))
	float WalkSpeed = 175.f;

	// 스프린트(Shift) 중 속도.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0.0"))
	float SprintSpeed = 420.f;

	// 야간 전방 조명 on/off 시간대. 18시 이상 또는 6시 미만일 때 조명을 켠다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light", meta = (ClampMin = "0", ClampMax = "23"))
	int32 NightLightStartHour24 = 18;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light", meta = (ClampMin = "0", ClampMax = "23"))
	int32 NightLightEndHour24 = 6;

	// [빌드 작업등] TPS 빌드모드에서 작업등(L) ON 시 NightSpotLight를 이 값으로 상향(작업등답게, 과하지 않게).
	// 밤 일반 점등 시에는 생성자 기본값(BeginPlay에서 캡처)으로 되돌린다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light|WorkLight", meta = (ClampMin = "0.0"))
	float TPSWorkLightIntensity = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light|WorkLight", meta = (ClampMin = "0.0"))
	float TPSWorkLightRadius = 2500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light", meta = (ClampMin = "0.0"))
	float NightLightFadeSpeed = 2.0f;

	// --- 사다리 등반 (#184) ---
	// 등반 중 수직 이동 속도(MaxFlySpeed). W/S로 위/아래.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb", meta = (ClampMin = "0.0"))
	float ClimbSpeed = 175.f;

	// [끊김① 준등속] 등반 진입 시 CMC MaxAcceleration을 이 값으로 덮어 Velocity.Z가 ClimbSpeed로 빠르게 수렴
	// (가변 가속 램프 최소화 → PlayRate가 실제 속도를 바로 반영). 종료 시 진입 전 값으로 복원. 크게=거의 즉시 도달.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb", meta = (ClampMin = "0.0"))
	float ClimbMaxAcceleration = 8192.f;

	// [#184] 등반 시작 시 캐릭터를 사다리 안쪽(GetStepOffDirection, +X) 바라보게 회전 + 이 오프셋을 더한다.
	// 메시 기본 yaw 오프셋(보통 -90°)·애니 제작 방향 때문에 그대로면 옆을 볼 수 있어 PIE에서 ±90 조정용
	// (재컴파일 없이 디테일 패널). 기본 0 — 캐릭터가 옆 보면 이 값으로 맞춤.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb")
	float LadderFacingYawOffset = 0.f;

	// [#184] 등반 중 캐릭터를 사다리 등반 면에서 띄우는 여유(uu). 실제 거리 = 캡슐반경 + 이 값. 너무 작으면
	// 메시 관통, 크면 떨어져 보임 — 캡슐 반경 부근이 적정이라 이 값은 그 위 소량 gap.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb", meta = (ClampMin = "0.0"))
	float ClimbFaceGap = 5.f;

	// [#184] 등반 면 X/Y 당김 보간 속도(VInterpTo). 즉시 SetActorLocation은 멀리서 시작 시 순간이동이라 부드럽게
	// 당긴다. 클수록 빠르게 붙음(가까이 시작이면 거의 즉시). 0이면 사실상 안 붙음 주의.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb", meta = (ClampMin = "0.1"))
	float ClimbAttachInterpSpeed = 12.f;

	// [#184] 상단 step-off 시 캡슐 반경에 **더하는** 전방(상면 안쪽) 여유 거리(uu). 실제 전진 = 캡슐반경 + 이 값.
	// 캡슐 반경만큼이면 가장자리에 발 얹힘(60처럼 안 튐) — 이 값은 그 위 ±미세조정용(기본 0).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb", meta = (ClampMin = "0.0"))
	float StepOffForward = 0.f;

	// step-off 안착 보간 시간(초). 0이면 즉시(순간이동). 0.15~0.25가 부드러움. 보간 중 이동 입력 잠금.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb", meta = (ClampMin = "0.0"))
	float StepOffDuration = 0.2f;

	// step-off 안착 시 상면 위로 띄우는 여유(uu). 캡슐 반높이에 더해 상면에 살짝 떠서 시작(겹침 방지).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb", meta = (ClampMin = "0.0"))
	float StepOffZMargin = 20.f;

	// 등반 종료 후 트리거 재진입 무시 시간(초). step-off 직후 같은 트리거에 다시 잡혀 MOVE_Flying로
	// 복귀하는 무한 토글(진동)을 차단하는 핵심 가드.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb", meta = (ClampMin = "0.0"))
	float ClimbReentryCooldown = 0.5f;

	// 도달 판정 히스테리시스(uu). 상/하단 경계에서 도달↔미도달 매프레임 토글 방지.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb", meta = (ClampMin = "0.0"))
	float ClimbReachMargin = 5.f;

	// 등반 진입 허용 Z 여유(uu). 캐릭터 발이 사다리 하단 + 이 값 이내일 때만 등반 시작.
	// 상면에서 걸어다니는 캐릭터(발 Z ≈ 상단)가 전체높이 트리거에 닿아 등반으로 오인되어 step-off가
	// 반복되는 것을 차단(밑동 전용 진입). ⚠️ 상면→하강 등반은 미지원(올라가기 전용, MVP). 하강은 후속.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb", meta = (ClampMin = "0.0"))
	float ClimbEntryZTolerance = 80.f;

	// [#184/#343] 사다리 마무리(올라서기) 몽타주(예: AM_Man_Ladder_Finish, DefaultSlot). top 도착 '이전'에
	// Move()의 FinishTriggerDistance 거리트리거로 1회 재생 — 올라서기가 실제 top 도착과 맞물리게(도착 순간
	// 재생은 늦음). 미할당(nullptr)이면 몽타주 없이 기존 동작. ⚠️ ABP AnimGraph에 Slot 'DefaultSlot' 노드 필요.
	// ⚠️ 몽타주 Root Motion OFF(캡슐 이동은 비행이 전담, 켜면 이중이동). #343 옵션A: Finish hips를 Loop 높이(128.4)로
	// 평탄화(상승 0)해 캡슐 비행과 가산되지 않게 함 → '올라서기'는 평탄 hips 위 팔/다리 오버레이로 표현.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb")
	TObjectPtr<UAnimMontage> LadderFinishMontage = nullptr;

	// [#184/#343] 긴 사다리에서 top까지 남은 Z가 이 값 이하면 Finish를 1회 재생. 또한 짧은 사다리 분기 기준:
	// ClimbHeight < FinishTriggerDistance면 Finish 아예 스킵(Loop+step-off만). 옵션A에서 150 권장(BP에서 설정).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb", meta = (ClampMin = "0.0"))
	float FinishTriggerDistance = 150.f;

	// [루트모션 올라서기] 루트모션 Finish 몽타주 핸드오프 시작 거리(uu). 발이 top까지 이만큼 남으면 비행 수직
	// 이동을 끊고 루트모션 몽타주가 캐릭터를 슬래브 위로 안착시킨다(애니=위치 일치). ⚠️ 몽타주의 실제 상승 루트모션
	// 변위와 같게 튜닝해야 자연스럽다(작으면 몽타주가 덜 올라가 종료 스냅 크게, 크면 붕 뜬 뒤 몽타주 재생). 몽타주
	// 미할당/재생 실패(Woman 등)면 무시 — 기존 step-off 폴백. FinishTriggerDistance(짧은 사다리 스킵 기준)와 별개.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb", meta = (ClampMin = "0.0"))
	float ClimbFinishHandoffDistance = 100.f;

	// [루트모션 올라서기] 몽타주 종료 후 발 Z를 슬래브 표면(top)+캡슐 반높이에 맞추는 스냅 보정 허용 오차(uu).
	// 루트모션 종료 위치가 이 오차를 넘게 어긋나면 Z만 스냅(XY는 루트모션 결과 유지). 0이면 항상 스냅.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb", meta = (ClampMin = "0.0"))
	float ClimbFinishZSnapTolerance = 3.f;

	// 현재 오르는 사다리(없으면 null). 등반 상태의 단일 진실원.
	UPROPERTY(Transient)
	TObjectPtr<AOJJ_Ladder> CurrentLadder;

	// [#184] 현재 트리거 겹침 중인 사다리(없으면 null) — W 입력 등반 시작 후보. 등반 상태(CurrentLadder)와 별개
	// (근접 감지 전용). 사다리 파괴 대비 weak.
	TWeakObjectPtr<AOJJ_Ladder> OverlappingLadder;

	// 재진입 쿨다운 만료 월드시각(초). BeginClimb이 이 시각 전이면 무시.
	float ClimbCooldownUntil = 0.f;

	// 등반 활성 플래그(#184). CurrentLadder와 함께 set/clear. 사다리가 GC/파괴로 사라지면 포인터는
	// null이 되지만 이 플래그로 '비정상 소멸'을 감지해 비행/중력0 고착을 Tick에서 복구.
	bool bClimbing = false;

	// [끊김① 방향 래치] 마지막 비영 등반 입력 방향(+1 위/-1 아래). BeginClimb서 +1, Move서 Axis.Y 비영 시 갱신.
	// GetClimbDirection이 노출 — Velocity.Z 부호 대신 이 값으로 ABP 상태 전이(0 근처 토글 방지).
	float ClimbVerticalDir = 1.f;

	// [끊김① 준등속] 등반 진입 전 CMC MaxAcceleration 캐시 — 종료(ResumeWalkingWithCooldown)서 원복. BeginClimb서
	// ClimbMaxAcceleration로 덮기 직전 저장. 걷기 가속에 영향 주지 않게 정확 복원(단일 출처).
	float PreClimbMaxAcceleration = 2048.f;

	// 기본 yaw 회전 속도(°/s) 단일 출처 — 생성자 RotationRate와 수영 이탈 원복이 같은 값을 보게(매직넘버 방지).
	static constexpr float DefaultRotationRateYaw = 540.0f;
	// 기본 MaxFlySpeed(uu/s) — 수영 이탈 시 원복 단일 출처(엔진 기본). 등반은 자체 ClimbSpeed로 덮음(상호배타).
	static constexpr float DefaultMaxFlySpeed = 600.0f;

	// [수영] 현재 수영 중 플래그. Tick의 OJJ_UpdateSwimming이 set/clear. ABP가 IsSwimming()으로 읽음.
	bool bSwimming = false;

	// [수영] idle/이동 상태(속도 히스테리시스 적용된 이력값). TargetZ 오프셋 선택 + ABP Swim_Idle↔Swim_Forward 기준.
	// ABP가 자체 속도임계 대신 IsSwimMoving()을 쓰면 코드/애님 판정이 항상 일치한다(IsSwimming 패턴).
	bool bSwimMoving = false;

	// [수영] 수면 기준 캡슐 중심 Z 오프셋(부유 높이). idle = 머리 물 밖(수면 위로 더 떠야 하니 큰 음수),
	// 이동 = 더 잠김(0에 가깝게). 캐릭터 캡슐 크기에 맞춰 디자이너 튜닝.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OJJ|Swim")
	float SwimIdleOffsetZ = -40.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OJJ|Swim")
	float SwimMoveOffsetZ = 0.0f;

	// [수영] 진입/이탈 수심(공간 히스테리시스) — 수심 = SurfaceZ − 강바닥Z(라인트레이스). ⭐ 클램프된 캐릭터 위치가
	// 아니라 실제 지형 수심 기준이라 순환 없음(이전 CenterDepth 버그 해소). 진입은 깊어야, 이탈은 진입보다 얕을 때.
	// SwimExitWaterDepth < SwimEnterWaterDepth 라 진입/이탈 지점이 달라 진동 방지.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OJJ|Swim")
	float SwimEnterWaterDepth = 90.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OJJ|Swim")
	float SwimExitWaterDepth = 70.0f;

	// [수영] idle↔이동 속도 히스테리시스(uu/s) — 단일 임계 깜빡임(Z 튕김) 방지. SwimMoveExitSpeed < SwimMoveEnterSpeed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OJJ|Swim")
	float SwimMoveEnterSpeed = 60.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OJJ|Swim")
	float SwimMoveExitSpeed = 30.0f;

	// [수영] 수영 중 yaw 회전 속도(°/s) — 생성자 기본(540)이 수영 고속서 홱 돌게 보여 진입 시 낮춤, 이탈 시 원복.
	// bOrientRotationToMovement는 true 유지(이동 방향 회전은 그대로), RotationRate만 조정.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OJJ|Swim")
	float SwimRotationRateYaw = 140.0f;

	// [수영] 수영 중 최대 속도(uu/s) — MOVE_Flying 기본(600)보다 낮춰 헤엄 체감 조정. 진입 시 적용, 이탈 시 600 원복.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OJJ|Swim")
	float SwimMaxFlySpeed = 400.0f;

	// [수영] 수면 클램프 보간 속도(uu/s 환산 lerp). 급격한 Z 점프 방지.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OJJ|Swim")
	float SwimSurfaceLerpSpeed = 6.0f;
	// [수영] 물 영역 완전 이탈(bInWater=false) 시 디바운스(초) — 가장자리 진동 방지. 깊이 이탈과 별개 경로.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OJJ|Swim")
	float SwimExitDebounce = 0.2f;
	// [수영] 연속 물 밖 경과 시간 누적기(SwimExitDebounce 비교용).
	float SwimOutOfWaterTime = 0.0f;

	// [수영] WaterBody 질의용 그리드 캐시(OJJ_QueryWaterBodyAt). 최초 1회 GetActorOfClass.
	TWeakObjectPtr<class AOJJ_Grid> OJJ_CachedGridForSwim;

	// [수영] 매 틱 물 진입/이탈 감지 → MOVE_Swimming 토글 + 수면 클램프. 등반/빌드(MOVE_Flying) 중엔 건너뜀.
	void OJJ_UpdateSwimming(float DeltaSeconds);

	// step-off 보간 상태(#184). 보간 중엔 이동 입력 잠금 + 비행(중력0) 유지, 완료 시 Walking+쿨다운.
	bool bSteppingOff = false;

	// [#184] Finish 마무리 몽타주 한 등반당 1회 재생 가드. FinishTriggerDistance 도달 시 set,
	// BeginClimb/AbortClimb에서 clear. 미설정 시 매 프레임 재트리거되어 몽타주가 처음부터 반복("계속 나옴").
	bool bFinishPlaying = false;

	// [루트모션 올라서기] 루트모션 Finish 몽타주로 슬래브에 안착하는 중(비행 종료→몽타주가 위치 전담). 이 동안
	// Move()의 등반/걷기 입력과 Tick의 X/Y 당김을 모두 차단해 '이중이동'을 막는다. 몽타주 종료 델리게이트에서 clear.
	bool bClimbFinishing = false;

	FVector StepOffStart = FVector::ZeroVector;
	FVector StepOffTarget = FVector::ZeroVector;
	float StepOffElapsed = 0.f;

	// [#184] 등반 면 위치: 사다리에서 바깥(Foundation 반대)으로 (캡슐반경 + ClimbFaceGap) 떨어진 X/Y + 주어진 Z.
	// 등반 시작 스냅·등반 중 X/Y 고정 공용(진입 위치가 멀어도 사다리에 붙여 오르게).
	FVector OJJ_GetClimbFaceLocation(const AOJJ_Ladder* Ladder, float WorldZ) const;

	// [루트모션 올라서기] top 근처(ClimbFinishHandoffDistance) 도달 시 호출 — 잔여 비행 속도를 죽이고 루트모션
	// Finish 몽타주를 재생, 종료 델리게이트를 건다. 성공 시 true(이후 위치는 루트모션 전담). 몽타주/AnimInstance
	// 없거나 재생 실패(스켈레톤 불일치 등, Woman)면 false → 호출측이 기존 step-off EndClimb로 폴백.
	bool BeginClimbFinish();

	// [루트모션 올라서기] Finish 몽타주 종료 콜백(Montage_SetEndDelegate, 비다이나믹). 슬래브 표면에 발 Z 스냅
	// 보정(ClimbFinishZSnapTolerance) 후 걷기 복귀. 중단(bInterrupted)이면 스냅 생략.
	void OnLadderFinishMontageEnded(class UAnimMontage* Montage, bool bInterrupted);

	// 등반/step-off 종료 후 걷기 복귀 + 재진입 쿨다운 개시(공통 단일원).
	void ResumeWalkingWithCooldown();

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

	// --- Intro 연출 (L_Planet 시네마틱 경유 진입) ---
	// 누워있다 일어나는 getup 몽타주. BP_OJJ_Player에서 할당(경로 하드코딩 금지). 미할당이면 인트로 안전 스킵.
	UPROPERTY(EditAnywhere, Category = "OJJ|Intro")
	TObjectPtr<UAnimMontage> GetUpMontage;

	// 몽타주 종료 후 블렌드해 도달할 3인칭 SpringArm 목표 길이(uu).
	UPROPERTY(EditAnywhere, Category = "OJJ|Intro")
	float IntroArmLength = 270.f;

	// 1인칭(ArmLength 0)→3인칭(IntroArmLength) 카메라 블렌드 속도(FInterpTo). 클수록 빠르게 수렴.
	UPROPERTY(EditAnywhere, Category = "OJJ|Intro")
	float IntroBlendSpeed = 2.f;

	// 몽타주 종료 후 카메라 블렌드 진행 중 플래그. Tick에서 SpringArm 길이를 IntroArmLength로 보간.
	bool bBlendingCamera = false;

	// [인트로 입력 잠금] PlayIntroSequence에서 DisableInput을 실제로 적용했는지. 복원(EnableInput)을 정확히 1:1로
	// 짝지어 호출하기 위한 플래그 — PC가 null인 프레임에 DisableInput(null) 후 EnableInput(validPC)로 어긋나
	// 입력이 영구 잠기던 soft-lock을 막는다(불균형 방지). TryRestoreIntroInput에서만 해제.
	bool bIntroInputDisabled = false;

	// [인트로 안전 여유] 안전 타임아웃 = (getup 몽타주 길이) + 이 여유(초). 몽타주의 자연 재생 + 카메라 블렌드를
	// 절대 방해하지 않도록 절대시간(옛 6초)이 아니라 '몽타주 길이 기준'으로 잡는다. 이 여유는 몽타주 종료 후
	// 카메라 블렌드(보통 수초) + 버퍼를 덮을 만큼. 진짜 비정상(델리게이트 미발화 등)일 때만 ForceFinishIntro 발동.
	UPROPERTY(EditAnywhere, Category = "OJJ|Intro", meta = (ClampMin = "0.0"))
	float IntroSafetyExtraSeconds = 8.f;

	// 안전 타임아웃 타이머 핸들. 블렌드 정상 완료 시 ClearTimer로 취소(invalidate).
	FTimerHandle IntroSafetyTimerHandle;

	// [인트로 1인칭] getup 몽타주 동안 카메라를 부착할 메시 소켓 이름. 진짜 머리 시점(1인칭)을 위해
	// GetMesh()의 이 소켓에 카메라를 SnapToTarget으로 붙인다. 메시에 소켓이 없으면(DoesSocketExist) 부착 스킵 →
	// 기존 방식(ArmLength 0)으로 폴백. 메시 본/소켓 명명이 다르면 BP/디테일 패널에서 조정.
	UPROPERTY(EditAnywhere, Category = "OJJ|Intro")
	FName HeadSocketName = TEXT("HeadSocket");

	// [인트로 1인칭] 카메라를 HeadSocket에 부착한 상태 여부. 블렌드 시작 시 원위치 복원 가드(중복 복원/미부착 복원 방지).
	bool bCameraAttachedToHead = false;

	// [인트로 1인칭] HeadSocket 부착 직전의 원래 부착 상태(복원용). 부모 컴포넌트(보통 SpringArm)·소켓·상대 트랜스폼을
	// 저장해 블렌드 시작 시 그대로 되돌린다. 부모는 파괴 대비 weak.
	TWeakObjectPtr<USceneComponent> IntroCameraOriginalParent;
	FName IntroCameraOriginalSocket = NAME_None;
	FTransform IntroCameraOriginalRelativeTransform = FTransform::Identity;

	// 인트로 연출 시작 — 입력 잠금 + 1인칭(ArmLength 0) + getup 몽타주 재생 + 종료 델리게이트 바인드.
	// GetUpMontage/SpringArm/AnimInstance 중 하나라도 null이면 안전 스킵(입력 복구 + 평소 플레이, 크래시 방지).
	void PlayIntroSequence();

	// getup 몽타주 종료 콜백(FOnMontageEnded). 카메라를 HeadSocket에서 떼 원위치 복원 후 3인칭 블렌드를 시작(Tick이 보간 담당).
	void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	// [인트로 1인칭] 카메라를 GetMesh()의 HeadSocket에 부착(머리 시점). 원래 부착 상태를 저장한다.
	// 소켓 미존재/Camera·메시 null이면 부착 스킵(폴백: ArmLength 0 기존 방식 유지).
	void AttachCameraToHeadSocket();

	// [인트로 1인칭] 카메라를 HeadSocket에서 떼고 저장해둔 원래 부모/소켓/상대 트랜스폼으로 복원. 미부착이면 no-op.
	void RestoreCameraFromHeadSocket();

	// [인트로] 입력 복구(EnableInput) + 1회성 플래그(ShouldPlayIntro) 소거를 한곳에서. bIntroInputDisabled로
	// DisableInput과 1:1 짝을 보장한다. PC가 아직 없으면 입력 복구를 보류하고 false 반환 → 호출부가 다음 틱 재시도
	// (블렌드 종료/플래그 소거를 입력 복구 성공에 묶어 soft-lock을 원천 차단). 복구 완료/불필요 시 true.
	bool TryRestoreIntroInput();

	// [인트로 안전망] (몽타주 길이 + IntroSafetyExtraSeconds) 타이머 콜백 — 아직 인트로 잔여 상태(블렌드/입력잠금/
	// 머리부착)면 재생 중 몽타주를 멈추고 카메라(원위치+IntroArmLength)·입력을 강제 복구한다. 정상 완료를 방해하지
	// 않도록 몽타주 길이 기준으로 잡으며, 어떤 경로로도 입력이 영구 잠기지 않게 하는 최종 방어선(진짜 비정상 전용).
	void ForceFinishIntro();

	// --- Ending 연출 (통신탑 설치 시 엔딩 시네마틱) ---
	// 흐름: PlayEndingSequence(입력잠금 + 탑 로우앵글 컷1 + 미세 푸시인) → 페이드아웃 → ShowEndingVideo(BP 위젯)
	//       → 위젯이 FinishEndingSequence 호출(클리어 저장 + 페이드인 + 카메라/입력 복귀 → 샌드박스 계속).
	// 로컬(설치한) 플레이어 화면 전용 — 리플리케이션 범위 밖(멀티 동기화 미고려).

	// 컷1 카메라: 탑 중심에서의 수평 거리(uu).
	UPROPERTY(EditAnywhere, Category = "Ending", meta = (ClampMin = "1.0"))
	float EndingCamDistance = 900.f;

	// 컷1 카메라: 탑 바닥(Bounds 최저 Z)에서 올릴 높이(uu) — 로우 앵글 정도.
	UPROPERTY(EditAnywhere, Category = "Ending")
	float EndingCamHeightOffset = 150.f;

	// 컷1 동안 카메라가 탑 방향으로 전진할 총 거리(uu). 정적에 가까운 미세 푸시인.
	UPROPERTY(EditAnywhere, Category = "Ending", meta = (ClampMin = "0.0"))
	float EndingPushInAmount = 120.f;

	// 컷1(로우앵글 + 푸시인) 길이(초). 종료 시 페이드아웃 시작.
	UPROPERTY(EditAnywhere, Category = "Ending", meta = (ClampMin = "0.1"))
	float EndingCut1Duration = 4.f;

	// 페이드 아웃/인 각각의 길이(초).
	UPROPERTY(EditAnywhere, Category = "Ending", meta = (ClampMin = "0.0"))
	float EndingFadeDuration = 1.f;

	// 엔딩 영상 위젯 자체의 페이드인/아웃 길이(초) — UMG 애니 대신 C++이 RenderOpacity를 직접 보간
	// (WBP_EndingCinematic의 UMG 애니 트랙이 계속 오작동해 위젯 생명주기처럼 페이드도 C++ 소관으로 통일).
	UPROPERTY(EditAnywhere, Category = "Ending", meta = (ClampMin = "0.01"))
	float EndingWidgetFadeDuration = 0.75f;

	// VideoReady(OnMediaOpened) 수신 후 페이드인 시작까지 추가 지연(초). OnMediaOpened는 '파일 열림' 시점이라
	// 첫 프레임이 Media Texture에 기록되기 전 — 그 틈에 올리면 이전 재생 잔상이 짧게 비친다(로그 계측으로 확인).
	UPROPERTY(EditAnywhere, Category = "Ending", meta = (ClampMin = "0.0"))
	float EndingFadeInDelay = 0.2f;

	// 영상 단계 안전 타임아웃 상한(초) — 위젯이 FinishEndingSequence를 끝내 못 부르는 비정상 대비.
	// 실제 영상 길이보다 넉넉히(영상 재생/종료 통지 자체는 BP 위젯 담당).
	UPROPERTY(EditAnywhere, Category = "Ending", meta = (ClampMin = "1.0"))
	float EndingVideoMaxSeconds = 120.f;

	// 엔딩 영상 위젯(BP, 2단계 제작·할당). 위젯은 영상 종료/스킵 시 FinishEndingSequence를 호출해야 한다.
	// 미지정이면 영상 단계를 스킵하고 바로 복귀(안전 — 로그만 남김).
	UPROPERTY(EditDefaultsOnly, Category = "Ending")
	TSubclassOf<UUserWidget> EndingVideoWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> EndingVideoWidgetInstance;

	// PlayEndingSequence가 스폰하는 컷1 카메라. 복귀 블렌드가 원본을 참조하므로 즉시 파괴 대신 수명 만료로 자멸.
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> EndingCamera;

	// 엔딩 시퀀스 재진입/중복 종료 가드.
	bool bEndingSequenceActive = false;

	// [엔딩 입력 잠금] DisableInput 실제 적용 여부 — 인트로(bIntroInputDisabled)와 동일한 1:1 복구 짝 보장.
	bool bEndingInputDisabled = false;

	// 컷1 푸시인 진행 중 플래그(Tick이 보간). 진행 상태는 elapsed/시작위치/방향으로 추적.
	bool bEndingPushInActive = false;
	float EndingPushInElapsed = 0.f;
	FVector EndingCamStartLocation = FVector::ZeroVector;
	FVector EndingCamPushDir = FVector::ForwardVector;

	// [위젯 페이드] 검은 오버레이 알파 보간 상태(푸시인과 동일한 Tick 구동 패턴). 0=없음,
	// +1=페이드인(오버레이 알파→0, 검정이 걷히며 영상 드러남), -1=페이드아웃(→1, 검정으로 덮임).
	// 페이드아웃 완료 시 Tick이 CompleteEndingRestore를 호출한다. 시작 알파는 중도 스킵(페이드인 중 종료) 시
	// 밝기 점프 없이 현재 값에서 이어가기 위한 캡처.
	int32 EndingWidgetFadeDirection = 0;
	float EndingWidgetFadeElapsed = 0.f;
	float EndingWidgetFadeStartOpacity = 0.f;

	// [엔딩 페이드 오버레이] RenderOpacity 방식은 위젯이 뒤 배경과 반투명 합성돼 '검정→영상'이 아니라 갑자기
	// 나타나는 체감 → 화면 전체 검정 레이어(SImage)를 영상 위젯 위에 얹고 그 알파를 보간한다(1=검정, 0=영상).
	// StartCameraFade는 씬 렌더링만 덮고 UMG 위엔 못 그리므로 오버레이 방식으로 확정. Slate 직접 부착이라
	// UPROPERTY 불가 — CompleteEndingRestore/EndPlay에서 명시 제거.
	TSharedPtr<class SImage> EndingFadeOverlayImage;
	float EndingFadeOverlayAlpha = 1.f;

	// [오버레이] 생성(이미 있으면 알파만 적용) — 뷰포트 Slate 레이어 300(UMG AddToViewport(200)은 내부 +10이라 210).
	void EnsureEndingFadeOverlay(float InitialAlpha);
	// [오버레이] 검정 알파 적용(0~1 클램프) + 캐시 갱신.
	void SetEndingFadeOverlayAlpha(float Alpha);
	// [오버레이] 뷰포트에서 제거 + 핸들 해제(미부착이면 no-op).
	void RemoveEndingFadeOverlay();

	// [위젯 페이드인 게이트] Open Source가 비동기라 미디어가 실제 열리기 전에 페이드인하면 Media Texture의
	// 이전 재생 잔상 프레임이 떠오른 뒤 0초로 점프한다 → 페이드인은 NotifyEndingVideoReady(OnMediaOpened)가
	// 올 때까지 Opacity 0으로 대기. 중복 호출/타이밍 꼬임 방지 1회성 플래그 + 미도착 대비 폴백 타이머.
	bool bEndingWidgetFadeInStarted = false;
	// 페이드인 게이팅 단계 공용 타이머 — ①폴백(2초, Ready 미도착) → ②Ready 수신 후 EndingFadeInDelay 지연.
	// 두 용도가 시간상 겹치지 않아 핸들 하나를 재사용(CompleteEndingRestore가 일괄 취소).
	FTimerHandle EndingWidgetReadyTimerHandle;

	// [위젯 페이드인 실행] EndingFadeInDelay 지연 타이머 콜백 — 지연 중 엔딩이 끝났으면 no-op(가드).
	void StartEndingWidgetFadeIn();

	FTimerHandle EndingStageTimerHandle;  // 단계 전환 공용(컷1 종료→페이드, 페이드 종료→영상)
	FTimerHandle EndingSafetyTimerHandle; // soft-lock 최종 방어선(ForceFinishEnding)

	// 컷1 종료 콜백 — 푸시인 정지 + 페이드아웃 시작 + 페이드 완료 시 ShowEndingVideo 예약.
	void StartEndingFadeOut();

	// 페이드아웃 완료 후 엔딩 영상 위젯 표시(최상위 ZOrder + UI 입력). 클래스 미지정이면 즉시 종료 경로.
	void ShowEndingVideo();

	// [위젯 페이드인 시작 신호] 영상 위젯(BP)이 OnMediaOpened 시점에 호출하는 계약 — 미디어가 실제 열린 뒤에만
	// 페이드인(0→1)을 시작해 잔상 프레임 노출을 막는다. 미호출 대비 ShowEndingVideo의 2초 폴백 타이머가 대신 부른다.
	// 중복 호출/엔딩 종료 후 지연 호출은 무시(가드).
	UFUNCTION(BlueprintCallable, Category = "Ending")
	void NotifyEndingVideoReady();

	// 엔딩 종료 진입점 — 클리어 저장 후 위젯을 RenderOpacity 1→0으로 페이드아웃하고, 완료 시
	// CompleteEndingRestore가 실제 복귀를 수행한다(위젯 없는 비정상 경로는 페이드 스킵·즉시 복귀).
	// 영상 위젯(BP)이 영상 종료/스킵 시 호출한다. 중복 호출 가드 포함.
	UFUNCTION(BlueprintCallable, Category = "Ending")
	void FinishEndingSequence();

	// 엔딩 실제 복귀 — 카메라 페이드인 + 뷰타겟/입력 복원 + 위젯 제거/타이머 정리(구 FinishEndingSequence 후반부).
	// 위젯 페이드아웃 완료(Tick) 또는 페이드 스킵 경로에서만 호출.
	void CompleteEndingRestore();

	// [엔딩 안전망] 타임아웃 강제 종료 — 어떤 경로로도 입력이 영구 잠기지 않게 하는 최종 방어선.
	void ForceFinishEnding();

	// [엔딩] 입력 복구 + UI 입력모드/커서 원복의 단일 출처(인트로 TryRestoreIntroInput 규약).
	// PC가 아직 없으면 복구 보류 + false(호출부가 재시도 예약).
	bool TryRestoreEndingInput();

	// [빌드 작업등] L키 토글 상태(빌드모드 한정). UpdateNightSpotLightVisibility가 매 틱 이 값을 읽어
	// TPS=NightSpotLight, TopDown=BuildCamera 하향광으로 분배한다. 빌드 해제 시 false로 리셋.
	bool bWorkLightOn = false;

	// NightSpotLight의 생성자 기본 Intensity/AttenuationRadius. BeginPlay에서 캡처해 작업등 상향 후 원복에 사용.
	float BaseNightLightIntensity = 50.f;
	float BaseNightLightRadius = 1500.f;

	// --- Input handlers ---
	void UpdateNightSpotLightVisibility(float DeltaSeconds);
	// [빌드 작업등] L키 핸들러 — 빌드모드일 때만 bWorkLightOn 토글(빌드 밖 무동작).
	void ToggleWorkLight();
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Zoom(const FInputActionValue& Value);
	void StartJumpAction(const FInputActionValue& Value);
	// B키 핸들러 — 탑다운 빌드 토글. HandleBuildModeKey(TopDown)로 위임.
	void ToggleBuild(const FInputActionValue& Value);
	// V키 핸들러 — TPS 빌드 토글. HandleBuildModeKey(TPS)로 위임.
	void ToggleBuildTPS(const FInputActionValue& Value);
	void BuildPlace(const FInputActionValue& Value);
	void BuildPan(const FInputActionValue& Value);
	void BuildRotate(const FInputActionValue& Value);
	void BuildRotateMachine(const FInputActionValue& Value);
	// [공중 Foundation] Q/E 높이 층 증감 — 축 부호(E=+1/Q=-1)로 BuildController->AdjustBuildHeight(±1) 위임.
	void BuildAdjustHeight(const FInputActionValue& Value);
	void ConnectFactoryAgentClient();
	void SendOperatorGuideRequest();
	void TriggerHUDQuestRequest();
	void TriggerHUDQuestWindowToggle();
	void TriggerHUDAIGuideToggle();
	void TriggerTutorialDialogueReveal();
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

public:
	// [임시 진입로 — F4-1'] PIE 콘솔용 배치 모드 전환(예: `SetBuildMode pipe`). IA 에셋/UI 슬롯이
	// 없는 신규 모드(pipe/tank)의 검증 진입로 — 정식 키/UI 와이어링(BP·UI 소유자 안건) 후 제거 후보.
	UFUNCTION(Exec)
	void SetBuildMode(const FString& ModeName);

	UFUNCTION(Exec)
	void TutorialAdvance();

	UFUNCTION(Exec)
	void TutorialLog();

	UFUNCTION(Exec)
	void SetMachineLevel(const FString& MachineTypeName, int32 NewLevel);

	UFUNCTION(Exec)
	void UpgradeMachineLevel(const FString& MachineTypeName, int32 UpgradeCount = 1);

	UFUNCTION(Exec)
	void ResetGame();

	UFUNCTION(Exec)
	void ResetTutorial();

	UFUNCTION(Exec)
	void BackupAndResetGame();

	UFUNCTION(Exec)
	void RestoreBackupGame();

	UFUNCTION(Exec)
	void ClearWarehouse();

	UFUNCTION(Exec)
	void Give(const FString& ItemID, int32 Count);

	UFUNCTION(Exec)
	void TriggerPlanetEvent(const FString& EventName, float Severity = 1.0f, float DurationSeconds = -1.0f);

	UFUNCTION(Exec)
	void TimeSet(int32 TotalMinutes);

	UFUNCTION(Exec)
	void GenerateFactoryState();

	UFUNCTION(Exec)
	void GenerateFactoryStateLog();

	UFUNCTION(Exec)
	void GenerateFactoryAnalyzeRequest();

	UFUNCTION(Exec, Category = "Cheats")
    void Cheat_ResetMachines();

	// [엔딩] 통신탑 설치 확정 시 BuildController가 호출하는 엔딩 시퀀스 진입점(로컬 화면 전용).
	// 재진입 가드 포함 — 연출 중 중복 호출 무시. 클리어 여부(1회성) 체크는 호출부(BuildController) 책임.
	void PlayEndingSequence(AActor* TowerActor);

	// [엔딩 PIE 검증용] 월드의 첫 통신탑을 찾아 엔딩 연출을 강제 재생(클리어 플래그 무시). 콘솔: `PlayEnding`
	UFUNCTION(Exec)
	void PlayEnding();
protected:
	void SetDemolishModeShortcut();
	// [#184] C키 — 사다리 빌드 서브모드 진입(빌드모드 중에만, SetDemolishModeShortcut 패턴). 공용키 개편서 H로 이동.
	void SetLadderModeShortcut();
	// [공용키] F=평면 Foundation, G=경사 RampFoundation, Z=마우스 초기화(취소). 전부 레거시 BindKey + IsInBuildMode 가드.
	void SetFoundationModeShortcut();
	void SetRampFoundationModeShortcut();
	void CancelPlacementShortcut();
	void TriggerPlanetEventNoneShortcut();
	void TriggerPlanetEventMagneticShortcut();
	void TriggerPlanetEventSandShortcut();
	void TimeSetMorningShortcut();
	// [#405 일부] C키 — TPS 빌드모드에서만 1인칭↔3인칭 토글(레거시 BindKey + IsInBuildMode·TPS 가드).
	// 1인칭=SpringArm ArmLength 0 + GetMesh() OwnerNoSee(자기 뷰에서만 몸 숨김, 멀티 안전).
	void ToggleBuildFPVShortcut();
	// 1인칭 상태 강제 해제(3인칭 복귀 + 메시 복원). 모드 전환(ApplyBuildModeView) 시 호출 — 잔류 방지.
	void ResetFirstPersonBuild();
	// [카테고리 숫자키] 1~9,0 → 현재 카테고리(LDJ UI_BuildModeMain)의 N번 슬롯 실행. 0키=10번 슬롯(1-base).
	// ExecuteHotbarSlot: IsInBuildMode 가드 + BuildModeWidgetInstance Cast(null이면 무동작+로그). 10개 thin BindKey 래퍼.
	void ExecuteHotbarSlot(int32 SlotIndex);
	void SetHotbarSlot1();
	void SetHotbarSlot2();
	void SetHotbarSlot3();
	void SetHotbarSlot4();
	void SetHotbarSlot5();
	void SetHotbarSlot6();
	void SetHotbarSlot7();
	void SetHotbarSlot8();
	void SetHotbarSlot9();
	void SetHotbarSlot10();
	// [카테고리 순환] 빌드모드(TPS+TopDown) ←/→ 방향키로 설치 카테고리(기계/전력/건물) 순환.
	// CycleBuildCategory: GetBuildViewMode()==None 차단 가드 + 위젯(UI_BuildModeMain) CycleSubMode 위임.
	// None(빌드 밖)에서는 무동작(←/→ 오발동 차단). ExecuteHotbarSlot과 동일 위임 구조.
	void CycleBuildCategory(int32 Dir);
	void CycleCategoryNext(); // → : Dir=+1 (기계→전력→건물)
	void CycleCategoryPrev(); // ← : Dir=-1 (역순)
	void SetDemolishMode(const FInputActionValue& Value);

	// 스프린트 — Started=달리기 속도, Completed=걷기 속도로 복귀.
	void StartSprint(const FInputActionValue& Value);
	void StopSprint(const FInputActionValue& Value);

	// B/V 키 공통 라우팅. 토글 규칙(현재==대상이면 해제, 아니면 대상 모드로 진입/전환)을 적용하고,
	// 빌드 활성 상태가 실제로 토글된 경우에만 ApplyBuildModeView 호출(TopDown↔TPS 전환 시 뷰 재적용 방지).
	// (1단계: ApplyBuildModeView는 TopDown 동작만 — TPS 카메라/좌표 분기는 2·3단계)
	void HandleBuildModeKey(EBuildViewMode TargetMode);

	// 빌드 뷰 모드(None/TopDown/TPS)에 맞춰 카메라 뷰타겟·플레이어 가시성·입력 컨텍스트를 전환.
	// BuildController가 단일 진실원 — HandleBuildModeKey가 실제 전환된 모드를 넘겨 호출한다.
	//  - None    : 뷰타겟=플레이어, 캐릭터 보임, IMC_Player 복귀(해제)
	//  - TopDown : 빌드캠 뷰타겟, 캐릭터 숨김, IMC_Build (마우스 커서 + Look 차단)
	//  - TPS     : 뷰타겟=플레이어(3인칭 유지), 캐릭터 보임, IMC_BuildTPS (Look 허용 + 이동 허용)
	void ApplyBuildModeView(EBuildViewMode NewMode);

	// 빌드 뷰 모드별 입력 컨텍스트 적용 헬퍼. 후보 IMC(Player/Build/BuildTPS)를 전부 제거 후 목표만 추가
	// → 모드 전환 시 누락/중복 원천 차단. ⚠️ 목표 IMC가 미할당이면 기존 컨텍스트를 제거하지 않는다(입력 먹통=못 빠져나옴 방지).
	void ApplyBuildInputContext(EBuildViewMode Mode);
};
