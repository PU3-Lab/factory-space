#include "UI_Minimap.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "EngineUtils.h"
#include "FactoryManagerSubsystem.h"
#include "MachineBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "OJJ_Grid.h"
#include "OJJ_MinimapCapture.h"

namespace
{
	// MID 파라미터 이름 — M_Minimap 머티리얼과의 계약(에디터 제작 시 동일 이름 필수).
	const FName ParamWindowCenter(TEXT("WindowCenter"));
	const FName ParamWindowSize(TEXT("WindowSize"));

	// 전력=Yellow, 고장/막힘=Red — 디버그 하이라이트와 다른 미니맵 고유 체계.
	const FLinearColor PowerIssueColor = FLinearColor(1.0f, 1.0f, 0.0f, 1.0f);
	const FLinearColor FaultIssueColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
}

void UUI_Minimap::NativeConstruct()
{
	Super::NativeConstruct();

	// IMG_Map 브러시 머티리얼 → MID 승격(WindowCenter/WindowSize 파라미터 공급용).
	if (IMG_Map)
	{
		if (UMaterialInterface* MapMaterial = Cast<UMaterialInterface>(IMG_Map->GetBrush().GetResourceObject()))
		{
			MapMID = UMaterialInstanceDynamic::Create(MapMaterial, this);
			IMG_Map->SetBrushFromMaterial(MapMID);
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[UI_Minimap] IMG_Map 브러시가 머티리얼이 아님 — 확대창 이동 비활성(정적 이미지로 표시). WBP에서 M_Minimap 할당 필요."));
		}
	}
}

void UUI_Minimap::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	APawn* Pawn = GetOwningPlayerPawn();
	if (!Pawn)
	{
		return;
	}
	const FVector PawnLocation = Pawn->GetActorLocation();
	const FVector2D PlayerXY(PawnLocation.X, PawnLocation.Y);

	// 배경 확대창: 플레이어 위치 → 캡처 UV. 규약(OJJ_MinimapCapture.h): U=+Y, V=-X.
	FVector2D CaptureCenter;
	float CaptureWorldSize = 0.0f;
	if (MapMID && ResolveCaptureFrame(CaptureCenter, CaptureWorldSize) && CaptureWorldSize > KINDA_SMALL_NUMBER)
	{
		const float U = 0.5f + (PlayerXY.Y - CaptureCenter.Y) / CaptureWorldSize;
		const float V = 0.5f - (PlayerXY.X - CaptureCenter.X) / CaptureWorldSize;
		MapMID->SetVectorParameterValue(ParamWindowCenter, FLinearColor(U, V, 0.0f, 0.0f));
		MapMID->SetScalarParameterValue(ParamWindowSize, ZoomWorldSize / CaptureWorldSize);
	}

	// 북쪽 고정 — 맵은 안 돌고 화살표만 회전. yaw 0(+X=북)=위, yaw 90(+Y)=오른쪽 → 각도=yaw 그대로.
	if (IMG_PlayerArrow)
	{
		IMG_PlayerArrow->SetRenderTransformAngle(Pawn->GetActorRotation().Yaw);
	}

	MarkerRefreshAccum += InDeltaTime;
	if (MarkerRefreshAccum >= MarkerRefreshInterval)
	{
		MarkerRefreshAccum = 0.0f;
		RefreshProblemMarkers();
	}

	UpdateMarkerPositions(PlayerXY);
}

bool UUI_Minimap::ResolveCaptureFrame(FVector2D& OutCenter, float& OutWorldSize)
{
	if (const AOJJ_MinimapCapture* Capture = CachedCapture.Get())
	{
		OutCenter = Capture->GetCaptureWorldCenter();
		OutWorldSize = Capture->GetCaptureWorldSize();
		return true;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	for (TActorIterator<AOJJ_MinimapCapture> It(World); It; ++It)
	{
		CachedCapture = *It;
		OutCenter = It->GetCaptureWorldCenter();
		OutWorldSize = It->GetCaptureWorldSize();
		return true;
	}

	// 캡처 액터 미배치 폴백: 그리드 bounds로 같은 프레임 구성(배경은 못 그려도 마커·UV 계약은 유지).
	if (!CachedGrid.IsValid())
	{
		for (TActorIterator<AOJJ_Grid> It(World); It; ++It)
		{
			CachedGrid = *It;
			break;
		}
	}
	if (const AOJJ_Grid* Grid = CachedGrid.Get())
	{
		FVector2D BoundsMin, BoundsMax;
		Grid->GetGridWorldBounds(BoundsMin, BoundsMax);
		const FVector2D Extent = BoundsMax - BoundsMin;
		OutCenter = (BoundsMin + BoundsMax) * 0.5f;
		OutWorldSize = FMath::Max(Extent.X, Extent.Y);
		return OutWorldSize > KINDA_SMALL_NUMBER;
	}

	return false;
}

void UUI_Minimap::RefreshProblemMarkers()
{
	ProblemMarkers.Reset();

	UGameInstance* GI = GetGameInstance();
	UFactoryManagerSubsystem* FactoryManager = GI ? GI->GetSubsystem<UFactoryManagerSubsystem>() : nullptr;
	if (!FactoryManager)
	{
		return;
	}

	for (const FMachineNode& Node : FactoryManager->GetMachineNodes())
	{
		AMachineBase* Machine = Node.MachineActor.Get();
		if (!Machine)
		{
			continue;
		}

		const EMachineState State = Machine->GetMachineState();
		const bool bPowerIssue = (State == EMachineState::NoPower);
		const bool bFaultIssue =
			Machine->isBroken() ||
			State == EMachineState::Blocked ||
			State == EMachineState::Disabled;
		if (!bPowerIssue && !bFaultIssue)
		{
			continue;
		}

		FMinimapProblemMarker Marker;
		Marker.Machine = Machine;
		// 고장은 수리 전엔 전력 넣어도 안 돌아가므로 선행 과제 — 고장 색 우선.
		Marker.Color = bFaultIssue ? FaultIssueColor : PowerIssueColor;
		ProblemMarkers.Add(Marker);
	}
}

void UUI_Minimap::UpdateMarkerPositions(const FVector2D& PlayerXY)
{
	if (!CP_Markers)
	{
		return;
	}

	// 원 반경은 마커 패널 지오메트리에서 산출(레이아웃 미확정 프레임이면 이번 틱 스킵).
	const FVector2D PanelSize = CP_Markers->GetCachedGeometry().GetLocalSize();
	const float Diameter = FMath::Min(PanelSize.X, PanelSize.Y);
	if (Diameter <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	const float ClampRadius = Diameter * 0.5f - EdgePadding;
	const float PixelsPerUU = Diameter / ZoomWorldSize;

	int32 UsedCount = 0;
	for (const FMinimapProblemMarker& Marker : ProblemMarkers)
	{
		const AMachineBase* Machine = Marker.Machine.Get();
		if (!Machine)
		{
			continue;
		}

		UImage* MarkerImage = GetOrCreatePoolMarker(UsedCount);
		if (!MarkerImage)
		{
			break;
		}
		++UsedCount;

		// 월드 델타 → 위젯 로컬(중앙 원점). 배경 UV 규약과 동일 축: 화면 +x=월드+Y, 화면 -y=월드+X.
		const FVector MachineLocation = Machine->GetActorLocation();
		FVector2D Offset(
			(MachineLocation.Y - PlayerXY.Y) * PixelsPerUU,
			-(MachineLocation.X - PlayerXY.X) * PixelsPerUU);

		// 시야 원 밖이면 방향 유지한 채 가장자리에 세운다 — 내비게이션 핵심.
		if (Offset.SizeSquared() > FMath::Square(ClampRadius))
		{
			Offset = Offset.GetSafeNormal() * ClampRadius;
		}

		MarkerImage->SetBrushTintColor(Marker.Color);
		MarkerImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (UCanvasPanelSlot* MarkerSlot = Cast<UCanvasPanelSlot>(MarkerImage->Slot))
		{
			MarkerSlot->SetPosition(Offset);
		}
	}

	// 남는 풀 위젯은 숨김(파괴 대신 재사용 대기).
	for (int32 Index = UsedCount; Index < MarkerPool.Num(); ++Index)
	{
		if (MarkerPool[Index])
		{
			MarkerPool[Index]->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

UImage* UUI_Minimap::GetOrCreatePoolMarker(int32 Index)
{
	if (MarkerPool.IsValidIndex(Index) && MarkerPool[Index])
	{
		return MarkerPool[Index];
	}
	if (!WidgetTree || !CP_Markers)
	{
		return nullptr;
	}

	UImage* MarkerImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	if (!MarkerImage)
	{
		return nullptr;
	}
	MarkerImage->SetBrush(MarkerBrush);

	UCanvasPanelSlot* MarkerSlot = CP_Markers->AddChildToCanvas(MarkerImage);
	if (MarkerSlot)
	{
		// 패널 중앙 앵커 + 중앙 정렬 → SetPosition이 곧 '중앙 기준 오프셋'.
		MarkerSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		MarkerSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		MarkerSlot->SetAutoSize(false);
		const FVector2D BrushSize = MarkerBrush.ImageSize.IsNearlyZero()
			? FVector2D(12.0f, 12.0f)
			: FVector2D(MarkerBrush.ImageSize);
		MarkerSlot->SetSize(BrushSize);
	}

	MarkerPool.SetNum(FMath::Max(MarkerPool.Num(), Index + 1));
	MarkerPool[Index] = MarkerImage;
	return MarkerImage;
}
