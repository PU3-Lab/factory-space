#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "UI_Minimap.generated.h"

class UImage;
class UCanvasPanel;
class UMaterialInstanceDynamic;
class AMachineBase;
class AOJJ_MinimapCapture;
class AOJJ_Grid;

// 문제 머신 마커 1개 — 0.5초 주기 판별 결과를 캐시하고, 위치는 매 틱 갱신(플레이어 이동 추종).
struct FMinimapProblemMarker
{
	TWeakObjectPtr<AMachineBase> Machine;
	FLinearColor Color = FLinearColor::Yellow;
};

/**
 * [미니맵] 원형 플레이어 중심 미니맵 (북쪽 고정 = 월드 +X가 화면 위) — WBP_Minimap의 C++ 부모.
 *
 * 데이터 흐름:
 *  - 배경: AOJJ_MinimapCapture가 구운 RT를 IMG_Map의 M_Minimap 머티리얼로 표시.
 *    C++는 MID 파라미터 WindowCenter(V2: 플레이어 UV)/WindowSize(스칼라: ZoomWorldSize/캡처 extent)만 공급 —
 *    확대창 이동/줌은 전부 머티리얼 UV 연산(TexSample UV = WindowCenter + (UV-0.5)*WindowSize).
 *  - 마커: UFactoryManagerSubsystem::GetMachineNodes()를 0.5초 주기로 순회해 문제 머신
 *    (NoPower/Blocked/Disabled 또는 isBroken)을 수집. 색은 에이전트 디버그 하이라이트 계승
 *    (전력=Cyan, 그 외=Yellow). 위치는 매 틱 (머신XY-플레이어XY)/ZoomWorldSize → 원 밖이면
 *    방향 유지한 채 가장자리(반경-EdgePadding) 클램프 — 시야 밖 문제도 방향은 항상 보인다.
 *  - UV 규약은 OJJ_MinimapCapture.h 참조(캡처 pitch-90/yaw0 고정과 한 몸).
 *
 * WBP_Minimap 디자이너 요구(전부 BindWidgetOptional — 없으면 해당 기능만 조용히 꺼짐):
 *  - IMG_Map: M_Minimap 머티리얼 브러시(원형 마스크 포함)
 *  - IMG_PlayerArrow: 위(북쪽)를 향하는 화살표 텍스처, 중앙 배치
 *  - CP_Markers: 마커가 붙을 CanvasPanel(IMG_Map 위, IMG_PlayerArrow 아래 권장)
 */
UCLASS()
class WANTED_FACTORY_API UUI_Minimap : public UUserWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(meta = (BindWidgetOptional)) UImage* IMG_Map;
	UPROPERTY(meta = (BindWidgetOptional)) UImage* IMG_PlayerArrow;
	UPROPERTY(meta = (BindWidgetOptional)) UCanvasPanel* CP_Markers;

	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 미니맵 원이 덮는 월드 지름(uu). 작을수록 확대. PIE 디테일 패널 다이얼.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap", meta = (ClampMin = "100.0"))
	float ZoomWorldSize = 10000.0f;

	// 문제 머신 마커 브러시. 텍스처 미지정이면 단색 사각(브러시 기본 white) — 틴트는 코드가 문제 유형별로 덮는다.
	// ImageSize가 마커 픽셀 크기.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
	FSlateBrush MarkerBrush;

	// 가장자리 클램프 여유(px) — 원 반경에서 이만큼 안쪽에 마커를 세운다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap", meta = (ClampMin = "0.0"))
	float EdgePadding = 8.0f;

	// 문제 머신 재판별 주기(초). 위치 갱신은 매 틱이라 이 값은 판별 비용만 결정.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap", meta = (ClampMin = "0.05"))
	float MarkerRefreshInterval = 0.5f;

private:
	// IMG_Map 브러시 머티리얼로 만든 MID(WindowCenter/WindowSize 공급 대상). 브러시가 머티리얼이 아니면 null.
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MapMID;

	// 마커 위젯 풀 — 매 갱신 생성/파괴 대신 재사용, 남는 것은 Collapsed.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> MarkerPool;

	TArray<FMinimapProblemMarker> ProblemMarkers;
	float MarkerRefreshAccum = 0.0f;

	// 레벨 참조 lazy 캐시(BeginPlay 순서 무관). 캡처 액터 없으면 그리드 bounds로 폴백.
	TWeakObjectPtr<AOJJ_MinimapCapture> CachedCapture;
	TWeakObjectPtr<AOJJ_Grid> CachedGrid;

	// 캡처 기준값(중심 XY/한 변 월드 크기) 해석. 캡처 액터 → 그리드 폴백 순. 둘 다 없으면 false.
	bool ResolveCaptureFrame(FVector2D& OutCenter, float& OutWorldSize);

	// 0.5초 주기 — GetMachineNodes 순회로 문제 머신 목록 재수집.
	void RefreshProblemMarkers();

	// 매 틱 — 풀에서 마커를 꺼내 위치/색 적용(+가장자리 클램프).
	void UpdateMarkerPositions(const FVector2D& PlayerXY);

	UImage* GetOrCreatePoolMarker(int32 Index);
};
