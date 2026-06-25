#pragma once

#include "Blueprint/UserWidget.h"
#include "StateIndicatorIconWidget.generated.h"

class UImage;
class UTexture2D;

UCLASS()
class WANTED_FACTORY_API UStateIndicatorIconWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	void SetIndicatorIcon(
		UTexture2D* IconTexture,
		const FLinearColor& IconTint,
		bool bEnableGlow,
		const FLinearColor& GlowTint,
		float IconDrawSize);

private:
	void EnsureWidgetTree();

	UPROPERTY(Transient)
	TObjectPtr<UImage> IconImage;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CurrentIconTexture;

	FLinearColor CurrentIconTint = FLinearColor::Transparent;
	FLinearColor CurrentGlowTint = FLinearColor::Transparent;
	float CurrentIconDrawSize = 0.0f;
};
