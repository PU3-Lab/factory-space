#include "UI_FactoryStatusRow.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

UUI_FactoryStatusRow::UUI_FactoryStatusRow(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UUI_FactoryStatusRow::SetRowData(
	UTexture2D* InItemIcon,
	const FText& InItemName,
	float InActualProductionPerSecond,
	float InTheoreticalProductionPerSecond)
{
	SetItemIcon(InItemIcon);
	SetItemName(InItemName);
	SetActualProduction(InActualProductionPerSecond);
	SetTheoreticalProduction(InTheoreticalProductionPerSecond);
}

void UUI_FactoryStatusRow::SetItemIcon(UTexture2D* InItemIcon)
{
	if (!IMG_ItemIcon)
	{
		return;
	}

	if (InItemIcon)
	{
		IMG_ItemIcon->SetBrushFromTexture(InItemIcon);
		IMG_ItemIcon->SetVisibility(ESlateVisibility::Visible);
		return;
	}

	IMG_ItemIcon->SetBrush(FSlateBrush());
	IMG_ItemIcon->SetVisibility(ESlateVisibility::Hidden);
}

void UUI_FactoryStatusRow::SetItemName(const FText& InItemName)
{
	if (TXT_ItemName)
	{
		TXT_ItemName->SetText(InItemName);
	}
}

void UUI_FactoryStatusRow::SetActualProduction(float InActualProductionPerSecond)
{
	if (TXT_ActualProduction)
	{
		TXT_ActualProduction->SetText(FormatProductionText(InActualProductionPerSecond));
	}
}

void UUI_FactoryStatusRow::SetTheoreticalProduction(float InTheoreticalProductionPerSecond)
{
	if (TXT_TheoreticalProduction)
	{
		TXT_TheoreticalProduction->SetText(FormatProductionText(InTheoreticalProductionPerSecond));
	}
}

FText UUI_FactoryStatusRow::FormatProductionText(float InProductionPerSecond) const
{
	return FText::FromString(FString::Printf(TEXT("%.1f / s"), InProductionPerSecond));
}
