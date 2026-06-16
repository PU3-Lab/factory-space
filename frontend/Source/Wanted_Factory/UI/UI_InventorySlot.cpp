#include "UI/UI_InventorySlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/DataTable.h"
#include "Resource/ResourceData.h"
#include "ItemDragDropOperation.h"
#include "OJJ_Player.h"
#include "UI/UI_Inventory.h" 
#include "PlayerWarehouseSubsystem.h"

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
    
   if (CurrentSlotItemID.IsNone() || CurrentSlotItemCount <= 0) return;

   // 1. 드래그 오퍼레이션 생성
   UItemDragDropOperation* DragOp = NewObject<UItemDragDropOperation>(this, UItemDragDropOperation::StaticClass());
   DragOp->DraggedItemID = CurrentSlotItemID; 
   DragOp->Pivot = EDragPivot::MouseDown;

   if (IMG_ItemIcon)
   {
      // 생성 소유자(Outer)를 안전한 PlayerController(GetOwningPlayer)로 지정합니다
      UImage* DragVisualImage = NewObject<UImage>(GetOwningPlayer(), UImage::StaticClass());
      if (DragVisualImage)
      {
         DragVisualImage->SetBrush(IMG_ItemIcon->GetBrush());
         DragVisualImage->SetDesiredSizeOverride(FVector2D(64.f, 64.f));
         DragOp->DefaultDragVisual = DragVisualImage;
      }
   }

   OutOperation = DragOp; 
}

// 슬롯 마우스 다운 강제 감지 규칙
FReply UUI_InventorySlot::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
   FReply Reply = Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);

   // 슬롯에 진짜 아이템이 들어있을 때만 드래그 감지 락을 캡처합니다.
   if (!CurrentSlotItemID.IsNone() && CurrentSlotItemCount > 0)
   {
      // 유저가 마우스 왼쪽 버튼을 누르는 '그 즉시' 엔진에게 드래그를 예약시킵니다.
      if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
      {
         return Reply.DetectDrag(this->TakeWidget(), EKeys::LeftMouseButton);
      }
   }

   return Reply;
}

bool UUI_InventorySlot::NativeOnDrop(const FGeometry& MyGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
   UItemDragDropOperation* ItemDragOp = Cast<UItemDragDropOperation>(InOperation);
   if (!ItemDragOp) return false;

   FName DroppedItemID = ItemDragOp->DraggedItemID;
   if (DroppedItemID.IsNone()) return false;

   UGameInstance* GI = GetGameInstance();
   if (GI)
   {
      UPlayerWarehouseSubsystem* WarehouseSubsystem = GI->GetSubsystem<UPlayerWarehouseSubsystem>();
      if (WarehouseSubsystem)
      {
         // 1. 내 가방 장부에 아이템 +1개 스펙 추가
         WarehouseSubsystem->AddItem(DroppedItemID, 1);
            
         // 2. 플레이어 본체가 들고 있는 인벤토리 공식 인스턴스를 소환합니다.
         APlayerController* PC = GetOwningPlayer();
         if (PC)
         {
            AOJJ_Player* OJJPlayer = Cast<AOJJ_Player>(PC->GetPawn());
            if (OJJPlayer)
            {
               // 플레이어 헤더에 있는 GetInventoryWidgetInstance()를 통해 내부의 공식 새로고침 함수를 완벽하게 트리거
               UUI_Inventory* TargetInventory = OJJPlayer->GetInventoryWidgetInstance();
               if (TargetInventory)
               {
                  TargetInventory->RefreshInventoryWindow();
               }
            }
         }

         UE_LOG(LogTemp, Log, TEXT("[가방 드롭 마감 완료] '%s' 아이템을 성공적으로 내 인벤토리에 안전 바인딩했습니다!"), *DroppedItemID.ToString());
         return true;
      }
   }
   return false;
}