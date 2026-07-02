#include "UI/UI_InventorySlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/DataTable.h"
#include "Resource/ResourceData.h"
#include "ItemDragDropOperation.h"
#include "OJJ_Player.h"
#include "UI/UI_Inventory.h" 
#include "UI/UI_BaseCampInteract.h"
#include "PlayerWarehouseSubsystem.h"
#include "UObject/ConstructorHelpers.h"

UUI_InventorySlot::UUI_InventorySlot(const FObjectInitializer& ObjectInitializer)
   : Super(ObjectInitializer)
{
   static ConstructorHelpers::FObjectFinder<UDataTable> ResourceTableFinder(
      TEXT("/Game/DataTable/DT_ResourceData.DT_ResourceData"));
   if (ResourceTableFinder.Succeeded())
   {
      ResourceDataTable = ResourceTableFinder.Object;
   }
}

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
    if (!ResourceDataTable)
    {
       if (IMG_ItemIcon) IMG_ItemIcon->SetVisibility(ESlateVisibility::Hidden);
       return;
    }
    
    FResourceData* RowData = ResourceDataTable->FindRow<FResourceData>(ItemID, TEXT("FindResourceIconContext"));

    if (RowData && IMG_ItemIcon)
    {
       UTexture2D* IconTexture = nullptr;

       if (RowData->ImgAsset.IsValid())
       {
          IconTexture = RowData->ImgAsset.Get();
       }
       else
       {
          IconTexture = RowData->ImgAsset.LoadSynchronous();
       }

       if (IconTexture)
       {
          IMG_ItemIcon->SetVisibility(ESlateVisibility::Visible);
          IMG_ItemIcon->SetBrushFromTexture(IconTexture);
       }
       else
       {
          IMG_ItemIcon->SetVisibility(ESlateVisibility::Hidden);
       }
    }
    else if (IMG_ItemIcon)
    {
       IMG_ItemIcon->SetVisibility(ESlateVisibility::Hidden);
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
   DragOp->Payload = this;

   if (IMG_ItemIcon)
   {
      // 생성 소유자(Outer)를 PlayerController(GetOwningPlayer)로 지정합니다
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

   // 내장 필드 Payload를 검사/저장해둔 출발지(Payload)가 나 혹은 또 다른 인벤토리 슬롯 클래스(UUI_InventorySlot)인지 검사
   if (Cast<UUI_InventorySlot>(ItemDragOp->Payload))
   {
      UE_LOG(LogTemp, Log, TEXT("[가방 드롭 가드] 내 인벤토리 슬롯 간의 내부 이동/제자리 드롭이 감지되어 추가 연산을 취소합니다."));
      
      // true를 리턴해 연산을 안전하게 종료(Rollback)시킴으로써 개수가 늘어나는 버그를 완벽 차단합니다.
      return true; 
   }

   // 📦 이하 구역은 캐스팅 가드를 통과한 '외부 기계 창고 UI' 등에서 아이템을 드래그해왔을 때만 실행됩니다.
   UGameInstance* GI = GetGameInstance();
   if (GI)
   {
      UPlayerWarehouseSubsystem* WarehouseSubsystem = GI->GetSubsystem<UPlayerWarehouseSubsystem>();
      if (WarehouseSubsystem)
      {
         // 1. 내 가방 장부에 아이템 +1개 스펙 추가
         UUI_BaseCampInteract* BaseCampSource = Cast<UUI_BaseCampInteract>(ItemDragOp->Payload);
         if (BaseCampSource && !BaseCampSource->TakeInputItemForInventoryDrop(DroppedItemID))
         {
            return false;
         }

         if (!WarehouseSubsystem->AddItem(DroppedItemID, 1))
         {
            if (BaseCampSource)
            {
               BaseCampSource->ReturnInputItemFromFailedDrop(DroppedItemID);
            }
            return false;
         }
            
         // 2. 플레이어 본체가 들고 있는 인벤토리 공식 인스턴스를 소환합니다.
         if (BaseCampSource)
         {
            BaseCampSource->RefreshCampInventoryAfterInventoryDrop();
         }

         APlayerController* PC = GetOwningPlayer();
         if (PC)
         {
            AOJJ_Player* OJJPlayer = Cast<AOJJ_Player>(PC->GetPawn());
            if (OJJPlayer)
            {
               UUI_Inventory* TargetInventory = OJJPlayer->GetInventoryWidgetInstance();
               if (TargetInventory)
               {
                  TargetInventory->RefreshInventoryWindow();
               }
            }
         }

         UE_LOG(LogTemp, Log, TEXT("[가방 드롭 마감 완료] 외부에서 들어온 '%s' 아이템을 인벤토리에 장부 등록했습니다!"), *DroppedItemID.ToString());
         return true;
      }
   }
   return false;
}
