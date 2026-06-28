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
 * 다음 단계(이번 범위 밖): 배경 투명(알파) 처리 + WBP_DialogueBalloon 포트레이트 Image 연결.
 */
UCLASS()
class WANTED_FACTORY_API AOJJ_PortraitCapture : public AActor
{
	GENERATED_BODY()

public:
	AOJJ_PortraitCapture();

protected:
	virtual void BeginPlay() override;

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
};
