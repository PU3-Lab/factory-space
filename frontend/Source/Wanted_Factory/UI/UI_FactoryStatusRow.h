#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI_FactoryStatusRow.generated.h"

class UDataTable;
class UImage;
class UTextBlock;
class UTexture2D;

UCLASS()
class WANTED_FACTORY_API UUI_FactoryStatusRow : public UUserWidget
{
	GENERATED_BODY()

public:
	UUI_FactoryStatusRow(const FObjectInitializer& ObjectInitializer);

	// 1. 아이템 아이콘 이미지 변수
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "Factory Status")
	class UImage* IMG_ItemIcon;

	// 2. 아이템 이름 텍스트 변수 (예: 철광석, 철판)
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "Factory Status")
	class UTextBlock* TXT_ItemName;

	// 3. 실제 생산량 수치 텍스트 변수 (예: 12.5 / s)
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "Factory Status")
	class UTextBlock* TXT_ActualProduction;

	// 4. 이론상 생산량 수치 텍스트 변수 (예: 15.0 / s)
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "Factory Status")
	class UTextBlock* TXT_TheoreticalProduction;

	UFUNCTION(BlueprintCallable, Category = "Factory Status")
	void SetRowData(class UTexture2D* InItemIcon, const FText& InItemName, float InActualProductionPerSecond, float InTheoreticalProductionPerSecond);

	UFUNCTION(BlueprintCallable, Category = "Factory Status")
	void SetItemIcon(UTexture2D* InItemIcon);

	UFUNCTION(BlueprintCallable, Category = "Factory Status")
	void SetItemName(const FText& InItemName);

	UFUNCTION(BlueprintCallable, Category = "Factory Status")
	void SetActualProduction(float InActualProductionPerSecond);

	UFUNCTION(BlueprintCallable, Category = "Factory Status")
	void SetTheoreticalProduction(float InTheoreticalProductionPerSecond);

private:
	FText FormatProductionText(float InProductionPerSecond) const;
};
