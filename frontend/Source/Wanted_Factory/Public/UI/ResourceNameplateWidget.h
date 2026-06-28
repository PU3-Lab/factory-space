#pragma once

#include "Blueprint/UserWidget.h"
#include "ResourceNameplateWidget.generated.h"

class UBorder;
class UTextBlock;

UCLASS()
class WANTED_FACTORY_API UResourceNameplateWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	void SetResourceName(const FText& ResourceName);

private:
	void EnsureWidgetTree();

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Background;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameText;
};
