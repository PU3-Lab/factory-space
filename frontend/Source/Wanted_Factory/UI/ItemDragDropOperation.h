#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "ItemDragDropOperation.generated.h"

/**
 * 인벤토리에서 머신 UI로 아이템을 드래그할 때 데이터를 나르는 배달부 클래스
 */
UCLASS()
class WANTED_FACTORY_API UItemDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	// 가방 슬롯에서 쥐고 출발한 아이템의 고유 ID (예: iron_ore, copper_ingot)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DragDrop")
	FName DraggedItemID;
};