#include "UI/UI_InventorySlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/DataTable.h"
#include "Resource/ResourceData.h"
#include "ItemDragDropOperation.h"

void UUI_InventorySlot::UpdateSlot(FName ItemID, int32 ItemCount)
{
    // 슬롯이 실행될 때 자기가 무슨 아이템인지 무조건 주머니에 먼저 꽉 저장합니다
    CurrentSlotItemID = ItemID;
    CurrentSlotItemCount = ItemCount;

    // 1. 아이템이 없거나 개수가 0개 이하인 경우 빈 슬롯 시각화 처리
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

    // 3. 데이터 테이블에서 아이템 ID 정보 찾기
    if (!ResourceDataTable) return;
    
    FResourceData* RowData = ResourceDataTable->FindRow<FResourceData>(ItemID, TEXT("FindResourceIconContext"));

    if (RowData && IMG_ItemIcon)
    {
       IMG_ItemIcon->SetVisibility(ESlateVisibility::Visible);

       if (RowData->ImgAsset.IsValid())
       {
          IMG_ItemIcon->SetBrushFromTexture(RowData->ImgAsset.Get());
       }
       else
       {
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
    
   // (테스트가 완전히 끝났다면 가드 조건을 다시 활성화하셔도 좋습니다!)
   if (CurrentSlotItemID.IsNone() || CurrentSlotItemCount <= 0) return;

   // 1. 드래그 오퍼레이션 오브젝트 인스턴스 생성
   UItemDragDropOperation* DragOp = NewObject<UItemDragDropOperation>(this, UItemDragDropOperation::StaticClass());
   DragOp->DraggedItemID = CurrentSlotItemID; 
   DragOp->Pivot = EDragPivot::MouseDown; // 마우스 누른 지점을 기점으로 아이콘이 매달립니다.

   // 런타임에 마우스 따라다닐 투명한 껍데기 전용 가짜 Image 컴포넌트 생성
   if (IMG_ItemIcon)
   {
      UImage* DragVisualImage = NewObject<UImage>(this, UImage::StaticClass());
      if (DragVisualImage)
      {
         // 현재 내 슬롯이 띄우고 있는 텍스처(철광석 등) 이미지 리소스를 가짜 이미지에 그대로 복사
         DragVisualImage->SetBrush(IMG_ItemIcon->GetBrush());
            
         // 드래그해서 움직일 때 아이콘 크기가 너무 거대해지거나 찌그러지지 않도록 크기 고정
         DragVisualImage->SetDesiredSizeOverride(FVector2D(64.f, 64.f));

         // 복사 처리가 완료된 산뜻한 가짜 이미지를 배달부 비주얼에 탁 안겨줍니다.
         DragOp->DefaultDragVisual = DragVisualImage;
      }
   }

   // 3. 엔진에 리턴하여 공중 부양 시작
   OutOperation = DragOp; 
}