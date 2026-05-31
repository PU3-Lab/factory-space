#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/DataTable.h" // 데이터 테이블 헤더
#include "UI_BuildModeMain.generated.h"

UCLASS()
class UUI_BuildModeMain : public UUserWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(meta = (BindWidget))
	class UHorizontalBox* HBox_QuickslotBar; 

	// DT_TestFactoryData를 넣을 변수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FactoryData")
	class UDataTable* FactoryDataTable;

public:
	virtual void NativeConstruct() override;
};