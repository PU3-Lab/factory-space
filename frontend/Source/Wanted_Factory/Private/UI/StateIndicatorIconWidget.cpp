#include "UI/StateIndicatorIconWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"

void UStateIndicatorIconWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureWidgetTree();
}

void UStateIndicatorIconWidget::SetIndicatorIcon(
	UTexture2D* IconTexture,
	const FLinearColor& IconTint,
	bool bEnableGlow,
	const FLinearColor& GlowTint,
	float IconDrawSize)
{
	EnsureWidgetTree();
	if (!IconImage)
	{
		return;
	}

	if (CurrentIconTexture != IconTexture)
	{
		IconImage->SetBrushFromTexture(IconTexture, true);
		CurrentIconTexture = IconTexture;
	}

	FLinearColor EffectiveIconTint = IconTint;
	if (bEnableGlow)
	{
		EffectiveIconTint += GlowTint * GlowTint.A;
		EffectiveIconTint.A = IconTint.A;
	}

	if (CurrentIconTint != EffectiveIconTint)
	{
		IconImage->SetColorAndOpacity(EffectiveIconTint);
		CurrentIconTint = EffectiveIconTint;
	}
	CurrentGlowTint = GlowTint;

	const float ClampedIconDrawSize = FMath::Max(1.0f, IconDrawSize);
	if (!FMath::IsNearlyEqual(CurrentIconDrawSize, ClampedIconDrawSize))
	{
		const FVector2D DesiredSize(ClampedIconDrawSize, ClampedIconDrawSize);
		IconImage->SetDesiredSizeOverride(DesiredSize);
		CurrentIconDrawSize = ClampedIconDrawSize;
	}
}

void UStateIndicatorIconWidget::EnsureWidgetTree()
{
	if (!WidgetTree || IconImage)
	{
		return;
	}

	IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("StateIndicatorIconImage"));
	WidgetTree->RootWidget = IconImage;
}
