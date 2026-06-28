#include "UI/ResourceNameplateWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"

void UResourceNameplateWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureWidgetTree();
}

void UResourceNameplateWidget::SetResourceName(const FText& ResourceName)
{
	EnsureWidgetTree();
	if (NameText)
	{
		NameText->SetText(ResourceName);
	}
}

void UResourceNameplateWidget::EnsureWidgetTree()
{
	if (!WidgetTree || Background)
	{
		return;
	}

	Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ResourceNameplateBackground"));
	Background->SetBrushColor(FLinearColor::Transparent);
	Background->SetPadding(FMargin(8.0f, 4.0f));

	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ResourceNameText"));
	NameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.96f, 1.0f, 1.0f)));
	NameText->SetJustification(ETextJustify::Center);
	FSlateFontInfo NameFont = NameText->GetFont();
	NameFont.Size = 32;
	NameText->SetFont(NameFont);
	NameText->SetShadowOffset(FVector2D(1.0f, 1.0f));
	NameText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.75f));

	Background->SetContent(NameText);
	WidgetTree->RootWidget = Background;
}
