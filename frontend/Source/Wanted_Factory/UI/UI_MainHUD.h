#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlanetEventManagerSubsystem.h"
#include "UI_MainHUD.generated.h"

class UImage;
class UTexture2D;
class UProgressBar;

UCLASS()
class WANTED_FACTORY_API UUI_MainHUD : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UFUNCTION(BlueprintCallable, Category = "HUD | Quest")
    void ToggleQuestWindow();

    UFUNCTION()
    void OnRequestQuestsClicked();

protected:
    UPROPERTY(meta = (BindWidget))
    class UUI_QuestWindow* WBP_QuestWindow;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* TXT_InGameTime;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* TXT_DisasterDay;

    UPROPERTY(meta = (BindWidgetOptional))
    class UTextBlock* TXT_WindSpeed;

    UPROPERTY(meta = (BindWidgetOptional))
    class UTextBlock* TXT_Rainfall;

    UPROPERTY(meta = (BindWidgetOptional))
    class UBorder* B_PlanetEvent;

    UPROPERTY(meta = (BindWidgetOptional))
    class UTextBlock* TXT_PlanetEvent;

    // [HUD 아이콘] 날씨/이벤트 동적 아이콘 — 기존 TXT_*와 같은 Refresh 분기에서 브러시 교체.
    // ⚠️ BindWidgetOptional(형제 날씨 위젯과 동일): C++ 빌드가 WBP 갱신보다 먼저라, WBP에 이미지가
    //    아직 없어도 null 허용(필수 BindWidget이면 미추가 시 기존 WBP 바인딩 에러). 전부 null 체크.
    UPROPERTY(meta = (BindWidgetOptional))
    class UImage* IMG_Rainfall;

    UPROPERTY(meta = (BindWidgetOptional))
    class UImage* IMG_Wind;

    UPROPERTY(meta = (BindWidgetOptional))
    class UImage* IMG_PlanetEvent;

    // [이벤트 캡슐] None 상태에서 표시되는 모래폭풍 위험도 바. 이벤트 진행 중엔 숨김. BindWidgetOptional.
    UPROPERTY(meta = (BindWidgetOptional))
    class UProgressBar* PB_StormRisk;

    // [HUD 아이콘] 상태별 텍스처 — BP_OJJ(WBP_MainHUD 클래스 디폴트)에서 할당. null이면 해당 아이콘 숨김.
    UPROPERTY(EditDefaultsOnly, Category = "HUD|Icons|Rainfall")
    TObjectPtr<UTexture2D> RainfallIcon_Clear;
    UPROPERTY(EditDefaultsOnly, Category = "HUD|Icons|Rainfall")
    TObjectPtr<UTexture2D> RainfallIcon_Drizzle;
    UPROPERTY(EditDefaultsOnly, Category = "HUD|Icons|Rainfall")
    TObjectPtr<UTexture2D> RainfallIcon_Rain;
    UPROPERTY(EditDefaultsOnly, Category = "HUD|Icons|Rainfall")
    TObjectPtr<UTexture2D> RainfallIcon_Heavy;

    UPROPERTY(EditDefaultsOnly, Category = "HUD|Icons|Wind")
    TObjectPtr<UTexture2D> WindIcon_Calm;
    UPROPERTY(EditDefaultsOnly, Category = "HUD|Icons|Wind")
    TObjectPtr<UTexture2D> WindIcon_Breeze;
    UPROPERTY(EditDefaultsOnly, Category = "HUD|Icons|Wind")
    TObjectPtr<UTexture2D> WindIcon_Strong;
    UPROPERTY(EditDefaultsOnly, Category = "HUD|Icons|Wind")
    TObjectPtr<UTexture2D> WindIcon_Storm;

    UPROPERTY(EditDefaultsOnly, Category = "HUD|Icons|Event")
    TObjectPtr<UTexture2D> PlanetEventIcon_Magnetic;
    UPROPERTY(EditDefaultsOnly, Category = "HUD|Icons|Event")
    TObjectPtr<UTexture2D> PlanetEventIcon_Sand;

    // [HUD 아이콘 색] 상태별 틴트 — 텍스처 스왑과 같은 분기에서 SetColorAndOpacity. 아이콘 표시될 때만 적용(숨김이면 무의미).
    UPROPERTY(EditDefaultsOnly, Category = "HUD|Icons|Rainfall")
    FLinearColor RainColor_Clear = FLinearColor(1.0f, 0.961f, 0.0f, 1.0f);
    UPROPERTY(EditDefaultsOnly, Category = "HUD|Icons|Rainfall")
    FLinearColor RainColor_Drizzle = FLinearColor(0.02f, 0.2f, 1.0f, 1.0f);
    UPROPERTY(EditDefaultsOnly, Category = "HUD|Icons|Rainfall")
    FLinearColor RainColor_Rain = FLinearColor(0.02f, 0.2f, 1.0f, 1.0f);
    UPROPERTY(EditDefaultsOnly, Category = "HUD|Icons|Rainfall")
    FLinearColor RainColor_Heavy = FLinearColor(0.02f, 0.2f, 1.0f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category = "HUD|Icons|Wind")
    FLinearColor WindColor_Calm = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
    UPROPERTY(EditDefaultsOnly, Category = "HUD|Icons|Wind")
    FLinearColor WindColor_Breeze = FLinearColor(0.224f, 0.2f, 0.22f, 1.0f);
    UPROPERTY(EditDefaultsOnly, Category = "HUD|Icons|Wind")
    FLinearColor WindColor_Strong = FLinearColor(0.0f, 0.5f, 1.0f, 1.0f);
    UPROPERTY(EditDefaultsOnly, Category = "HUD|Icons|Wind")
    FLinearColor WindColor_Storm = FLinearColor(0.016f, 0.14f, 1.0f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category = "HUD|Icons|Event")
    FLinearColor PlanetEventColor_Magnetic = FLinearColor(0.538f, 0.063f, 1.0f, 1.0f);
    UPROPERTY(EditDefaultsOnly, Category = "HUD|Icons|Event")
    FLinearColor PlanetEventColor_Sand = FLinearColor(1.0f, 0.224f, 0.048f, 1.0f);

    // [이벤트 캡슐] 모래폭풍 위험도 바 percent (None 상태). 보너스 미충족(5%)→Low, 충족(20%)→High.
    UPROPERTY(EditDefaultsOnly, Category = "HUD|StormRisk", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float StormRiskLowPercent = 0.3f;
    UPROPERTY(EditDefaultsOnly, Category = "HUD|StormRisk", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float StormRiskHighPercent = 0.7f;

    // [이벤트 캡슐] 위험도 바 색 — 낮음 노랑 / 높음 주황.
    UPROPERTY(EditDefaultsOnly, Category = "HUD|StormRisk")
    FLinearColor StormRiskLowColor = FLinearColor(1.0f, 0.85f, 0.1f, 1.0f);
    UPROPERTY(EditDefaultsOnly, Category = "HUD|StormRisk")
    FLinearColor StormRiskHighColor = FLinearColor(1.0f, 0.5f, 0.1f, 1.0f);

    // [이벤트 캡슐] 진행 중 이벤트 캡슐(B_PlanetEvent) 색 — 자기폭풍 보라 / 모래폭풍 주황. (기존 하드코딩 teal/brown 대체)
    UPROPERTY(EditDefaultsOnly, Category = "HUD|StormRisk")
    FLinearColor MagneticStormCapsuleColor = FLinearColor(0.45f, 0.2f, 0.7f, 0.9f);
    UPROPERTY(EditDefaultsOnly, Category = "HUD|StormRisk")
    FLinearColor SandStormCapsuleColor = FLinearColor(0.7f, 0.4f, 0.12f, 0.9f);

    virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
    UFUNCTION()
    void HandleWeatherChanged(const FPlanetWeatherState& WeatherState);

    void RefreshWeatherText(const FPlanetWeatherState& WeatherState);

    UFUNCTION()
    void HandlePlanetEventStarted(EPlanetEventType EventType, float Severity);

    UFUNCTION()
    void HandlePlanetEventEnded(EPlanetEventType EventType);

    void RefreshPlanetEventUI(EPlanetEventType EventType, float Severity);

    // [이벤트 캡슐] None 상태 전용 — 모래폭풍 발생 확률(매니저 상수)로 PB_StormRisk percent/색 + TXT 위험 텍스트.
    // 시간 누적 없음(날씨 전환 시만 점프). 호출 측이 EventType==None일 때만 호출.
    void RefreshStormRiskBar(const FPlanetWeatherState& WeatherState);
};
