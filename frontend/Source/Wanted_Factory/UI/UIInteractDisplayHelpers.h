#pragma once

// [JJ #296 빌드수정] UI_MachineInteract.cpp / UI_WarehouseInteract.cpp 두 곳에
// 바이트 동일한 익명 namespace 헬퍼(GetResourceDisplayText / GetMachineDisplayText)가
// 중복 정의돼 있어 unity(jumbo) 빌드에서 같은 TU로 합쳐질 때 C2084(재정의) 충돌 발생.
// 단일 정의로 추출하여 충돌을 해소한다. 로직은 기존 본문 그대로(무변경).

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Resource/ResourceData.h"
#include "Machines/MachineSubsystem.h"

namespace UIInteractHelpers
{
inline FText GetResourceDisplayText(const UDataTable* ResourceDataTable, FName ItemName)
{
    if (ItemName.IsNone())
    {
        return FText::GetEmpty();
    }

    if (ResourceDataTable)
    {
        if (const FResourceData* RowData = ResourceDataTable->FindRow<FResourceData>(ItemName, TEXT("GetResourceDisplayText")))
        {
            if (!RowData->DisplayName.IsEmpty())
            {
                return FText::FromString(RowData->DisplayName);
            }
        }
    }

    return FText::FromName(ItemName);
}

inline FText GetMachineDisplayText(UMachineSubsystem* MachineSubsystem, FName MachineTypeName)
{
    if (MachineSubsystem)
    {
        return MachineSubsystem->GetMachineDisplayName(MachineTypeName);
    }

    return MachineTypeName.IsNone() ? FText::GetEmpty() : FText::FromName(MachineTypeName);
}
}
