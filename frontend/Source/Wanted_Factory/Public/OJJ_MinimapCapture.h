#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OJJ_MinimapCapture.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class AOJJ_Grid;

/**
 * [미니맵] 그리드 전체를 1장의 RenderTarget으로 내려찍는 직교 탑다운 캡처 액터 (레벨 배치형).
 *
 * 설계:
 *  - 상시 캡처가 아니라 요청식(RequestCapture) — bCaptureEveryFrame/OnMovement 모두 끔.
 *    지형/공장은 느리게 변하므로 BeginPlay 1초 후 1회 + RecaptureInterval 주기 재캡처로 충분.
 *  - BeginPlay에서 레벨의 AOJJ_Grid를 찾아 GetGridCenter 상공(CaptureHeight)으로 자동 이동,
 *    OrthoWidth = 그리드 extent(max(X,Y)) — 레벨 배치 위치는 무관(자동 정렬).
 *  - 캡처 방향: pitch -90, yaw 0 고정. 이때 이미지 +U = 월드 +Y, 이미지 -V(위) = 월드 +X.
 *    → UV 변환: U = 0.5 + (WorldY - CenterY)/OrthoWidth, V = 0.5 - (WorldX - CenterX)/OrthoWidth.
 *    UI_Minimap과 M_Minimap 머티리얼이 이 규약을 공유한다(변경 시 3곳 동기화).
 *
 * 에디터 후속: RT 에셋(권장 2048², RTF RGBA8) 생성 후 RenderTarget에 할당, 레벨에 1개 배치.
 */
UCLASS()
class WANTED_FACTORY_API AOJJ_MinimapCapture : public AActor
{
	GENERATED_BODY()

public:
	AOJJ_MinimapCapture();

	// 즉시 1회 캡처. 대규모 건설 직후 등 외부에서 강제 갱신하고 싶을 때 호출 가능.
	UFUNCTION(BlueprintCallable, Category = "Minimap")
	void RequestCapture();

	// UI_Minimap이 UV 변환에 쓰는 캡처 기준값 — 캡처된 이미지의 월드 중심 XY.
	UFUNCTION(BlueprintPure, Category = "Minimap")
	FVector2D GetCaptureWorldCenter() const;

	// 캡처된 이미지 한 변이 덮는 월드 크기(uu) = OrthoWidth.
	UFUNCTION(BlueprintPure, Category = "Minimap")
	float GetCaptureWorldSize() const;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Minimap")
	TObjectPtr<USceneCaptureComponent2D> CaptureComponent;

	// 캡처 대상 RenderTarget. 에디터에서 RT 에셋(2048²) 생성 후 할당 — 미할당이면 캡처 스킵 + 경고 로그.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	// 주기 재캡처 간격(초). 0이면 주기 재캡처 끔(BeginPlay 1회만).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap", meta = (ClampMin = "0.0"))
	float RecaptureInterval = 10.0f;

	// 그리드 평면에서 캡처 카메라까지 높이(uu). 직교라 화각 무관 — 지형 최고점보다 높으면 충분.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap", meta = (ClampMin = "100.0"))
	float CaptureHeight = 20000.0f;

	// BeginPlay 후 첫 캡처까지 지연(초) — 레벨 액터/머티리얼 초기화가 끝난 뒤 찍기 위한 여유.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap", meta = (ClampMin = "0.0"))
	float InitialCaptureDelay = 1.0f;

	FTimerHandle InitialCaptureTimerHandle;
	FTimerHandle RecaptureTimerHandle;

	// 그리드 기준 자동 정렬(위치/OrthoWidth). 그리드 미발견 시 배치 위치·기존 OrthoWidth 유지.
	void AlignToGrid();
};
