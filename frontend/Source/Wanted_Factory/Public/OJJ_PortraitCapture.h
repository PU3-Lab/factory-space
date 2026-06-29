// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OJJ_PortraitCapture.generated.h"

class USceneComponent;
class USkeletalMeshComponent;
class USpringArmComponent;
class USceneCaptureComponent2D;
class UPointLightComponent;
class UTextureRenderTarget2D;
class UAnimationAsset;

/**
 * 대화 패널 포트레이트용 로봇 실시간 캡처 액터 (OJJ 소유).
 *
 * 목적: idle 애니메이션이 도는 로봇을 SceneCaptureComponent2D로 RenderTarget(RT_RobotPortrait, 512x512)에
 *       실시간으로 찍어, 추후 UMG 대화 패널의 포트레이트 Image에 표시한다.
 *
 * 구성(전부 생성자에서 코드로 구성 — 에디터 세팅 누락 방지):
 *   SceneRoot
 *    └ RobotMesh   (USkeletalMeshComponent) : 로봇 메시 + idle 애니 단일노드 재생
 *    └ KeyLight    (UPointLightComponent)   : 로봇을 비추는 키 라이트
 *    └ SpringArm   (USpringArmComponent)    : 카메라 거리/각도 (충돌테스트 off)
 *       └ Capture  (USceneCaptureComponent2D): ShowOnlyActors=self 로 로봇만 캡처 → PortraitRenderTarget
 *
 * 사용법(MVP):
 *   1) 에디터에서 RT_RobotPortrait(512x512) 생성 후 PortraitRenderTarget 슬롯에 할당.
 *   2) 레벨에 본 액터 배치(메인 카메라에 안 잡히도록 멀리/지하에 두는 것을 권장 — 배경/메인뷰 격리는 다음 단계).
 *   3) PIE 실행 → RT_RobotPortrait 더블클릭하면 idle 도는 로봇이 보여야 함.
 *
 * 배경 투명: 캡처는 SCS_SceneColorHDR(알파=역불투명도)로 찍고, 알파 반전(로봇=불투명)은 UI 머티리얼
 * M_Portrait_UI(1-Alpha)에서 한 번만 한다 — 본 액터에서 ShowFlags.AlphaInvert는 켜지 않는다(이중 반전 방지).
 *
 * ⚠️ 전제/후속:
 *  - 단일 인스턴스 전제: 하나의 PortraitRenderTarget을 공유하므로 본 액터는 레벨에 1개만 둔다.
 *    여러 개가 같은 RT에 매 프레임 캡처하면 서로 덮어써 포트레이트가 깜빡인다(복수 필요 시 인스턴스별 RT 주입으로 분리).
 *  - 상시 캡처(bCaptureEveryFrame=true)한다. 단 에디터 첫 PIE는 M_Robot 베이스패스 셰이더가 DDC 미스로
 *    컴파일 중일 수 있고(수 초), 그동안 로봇이 디폴트 머티리얼로 깨진 채 렌더된다. 따라서 BeginPlay 후
 *    GShaderCompilingManager 컴파일 큐가 빌 때까지 폴링(TryBeginCapture)한 뒤 캡처를 켠다 — 고정 지연이 아니라
 *    실제 컴파일 완료를 기다려 머신/DDC 상태에 강건하다. WITH_EDITOR 전용이라 패키징은 셰이더가 쿡되어
 *    이 대기가 통째로 빠지고 즉시 캡처(글리치 자체가 패키징엔 없음).
 *    대화창이 거의 상시 떠있어 open/close 토글은 두지 않는다(상시 캡처 유지). 비용이 문제가 되면 간헐 캡처
 *    (타이머 CaptureScene)나 RT 해상도 축소를 검토한다.
 */
UCLASS()
class WANTED_FACTORY_API AOJJ_PortraitCapture : public AActor
{
	GENERATED_BODY()

public:
	AOJJ_PortraitCapture();

protected:
	virtual void BeginPlay() override;
	/** 워밍업 폴링 도중 액터 파괴(Subsystem Deinitialize 등) 시 타이머를 정리한다. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// --- 컴포넌트 ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portrait")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portrait")
	USkeletalMeshComponent* RobotMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portrait")
	UPointLightComponent* KeyLight;

	/** 키 라이트 반대편에서 그림자(어두운 면)를 살리는 보조광. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portrait")
	UPointLightComponent* FillLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portrait")
	USpringArmComponent* SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portrait")
	USceneCaptureComponent2D* Capture;

	// --- 에디터 설정 ---

	/** 캡처 결과를 출력할 RenderTarget. 에디터에서 RT_RobotPortrait(512x512)를 할당. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portrait")
	UTextureRenderTarget2D* PortraitRenderTarget;

	/** 로봇이 재생할 idle 애니메이션. 생성자에서 로봇 Idle을 기본 로드(에디터에서 교체 가능). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portrait")
	UAnimationAsset* IdleAnimation;

	// --- 자동 프레이밍 튜닝 (메시 크기와 무관하게 상반신을 칸 중앙에 채움) ---

	/** 메시 전체 높이 중 위에서부터 담을 비율(0.42=얼굴~가슴). 작을수록 더 클로즈업(얼굴 위주). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portrait|Framing", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float UpperBodyRatio = 0.42f;

	/** 프레임 여백 배수(1.0=꽉참, 1.35=35% 여백). 크면 줌아웃 — 머리 양옆(귀/안테나) 잘림 방지에 사용. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portrait|Framing", meta = (ClampMin = "1.0", ClampMax = "2.0"))
	float FramePadding = 1.35f;

	// --- 캡처 워밍업 (첫 프레임 글리치 회피: 셰이더 컴파일 완료까지 대기) ---

	/** 셰이더 컴파일 폴링 시작 전 초기 지연(초). 레벨 로드 직후 로봇 머티리얼 셰이더 컴파일 잡이 큐에 등록될
	 *  시간을 준 뒤 폴링을 시작한다(0이면 즉시 폴링). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portrait|Capture", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float CaptureWarmupDelay = 0.3f;

	/** 셰이더 컴파일 완료 폴링 간격(초). 컴파일 중인 동안 이 간격으로 재확인. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portrait|Capture", meta = (ClampMin = "0.02", ClampMax = "1.0"))
	float CaptureWarmupPollInterval = 0.15f;

	/** 셰이더 컴파일 대기 최대 시간(초). 초과하면 미완 상태로라도 캡처 시작(안전망 — 무한 대기 방지). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portrait|Capture", meta = (ClampMin = "1.0", ClampMax = "60.0"))
	float CaptureMaxWarmupWait = 15.f;

private:
	/** 캡처 워밍업 폴링 타이머. */
	FTimerHandle CaptureWarmupTimer;

	/** 누적 폴링 대기 시간(초). CaptureMaxWarmupWait 안전망 비교용. */
	float CaptureWarmupElapsed = 0.f;

	/** 셰이더 컴파일이 끝났는지 폴링 → 끝났으면(또는 최대대기 초과) BeginContinuousCapture 호출. */
	void TryBeginCapture();

	/** 연속 캡처(bCaptureEveryFrame)를 켜고 즉시 한 장 갱신. */
	void BeginContinuousCapture();
};
