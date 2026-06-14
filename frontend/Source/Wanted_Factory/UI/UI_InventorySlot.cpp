#include "UI/UI_InventorySlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/DataTable.h"
#include "Resource/ResourceData.h"
#include "ItemDragDropOperation.h"

void UUI_InventorySlot::UpdateSlot(FName ItemID, int32 ItemCount)
{
	// 1. 아이템이 없거나 개수가 0개 이하인 경우 빈 슬롯 처리
	if (ItemID.IsNone() || ItemCount <= 0)
	{
		if (IMG_ItemIcon) IMG_ItemIcon->SetVisibility(ESlateVisibility::Hidden);
		if (TXT_ItemCount) TXT_ItemCount->SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	// 2. 수량 텍스트 표현
	if (TXT_ItemCount)
	{
		TXT_ItemCount->SetVisibility(ESlateVisibility::Visible);
		TXT_ItemCount->SetText(FText::AsNumber(ItemCount));
	}

	// 3. 데이터 테이블에서 아이템 ID(철, 구리, 주괴 등) 정보 찾기
	if (!ResourceDataTable) return;
	
	FResourceData* RowData = ResourceDataTable->FindRow<FResourceData>(ItemID, TEXT("FindResourceIconContext"));

	if (RowData && IMG_ItemIcon)
	{
		IMG_ItemIcon->SetVisibility(ESlateVisibility::Visible);

		// TSoftObjectPtr<UTexture2D> ImgAsset 경로에서 에셋 동적 로드 및 적용
		if (RowData->ImgAsset.IsValid())
		{
			// 이미 메모리에 로드되어 있다면 바로 가볍게 세팅
			IMG_ItemIcon->SetBrushFromTexture(RowData->ImgAsset.Get());
		}
		else
		{
			// 아직 로드가 안 되어 있다면 즉시 비동기/동기식 로드 수행 (LoadSynchronous)
			UTexture2D* LoadedTexture = RowData->ImgAsset.LoadSynchronous();
			if (LoadedTexture)
			{
				IMG_ItemIcon->SetBrushFromTexture(LoadedTexture);
			}
		}
	}
}

void UUI_InventorySlot::NativeOnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& InPointerEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(MyGeometry, InPointerEvent, OutOperation);
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, TEXT("!!! DRAG DETECTED IN C++ !!!"));
	//if (CurrentSlotItemID.IsNone() || CurrentSlotItemCount <= 0) return;

	// 1. 드래그 오퍼레이션 오브젝트 인스턴스 생성
	UItemDragDropOperation* DragOp = NewObject<UItemDragDropOperation>(this, UItemDragDropOperation::StaticClass());
    
	// 2. 주머니에 저장해둔 진짜 아이템 ID를 배달부에 패킹
	DragOp->DraggedItemID = CurrentSlotItemID; 
	DragOp->Pivot = EDragPivot::MouseDown;

	// 3. 엔진에 리턴하여 공중 부양 시작
	OutOperation = DragOp; 
}