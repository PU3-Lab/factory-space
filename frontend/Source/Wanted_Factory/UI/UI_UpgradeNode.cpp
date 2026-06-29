#include "UI_UpgradeNode.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Machines/MachineSubsystem.h"
#include "Machines/MachineTable.h"
#include "OJJ_Player.h"
#include "UI/UI_BaseCampInteract.h"

void UUI_UpgradeNode::NativeConstruct()
{
    Super::NativeConstruct();

    if (BTN_Card)
    {
        BTN_Card->OnClicked.RemoveDynamic(this, &UUI_UpgradeNode::OnCardClicked);
        BTN_Card->OnClicked.AddDynamic(this, &UUI_UpgradeNode::OnCardClicked);
    }

    RefreshNodeState();
}

void UUI_UpgradeNode::RefreshNodeState()
{
    UGameInstance* GI = GetGameInstance();
    UMachineSubsystem* MachineSubsystem = GI ? GI->GetSubsystem<UMachineSubsystem>() : nullptr;
    if (!MachineSubsystem)
    {
        return;
    }

    const int32 CurrentGlobalLevel = MachineSubsystem->GetMachineLevel(MachineType);

    FMachineTableRow LevelData;
    const bool bHasLevelData = MachineSubsystem->FindMachineDataForLevel(MachineType, NodeLevel, LevelData);
    if (bHasLevelData)
    {
        if (TXT_LevelLabel)
        {
            TXT_LevelLabel->SetText(FText::FromString(FString::Printf(TEXT("%d레벨 %s"), NodeLevel, *LevelData.DisplayName)));
        }

        if (IMG_MachineIcon)
        {
            UTexture2D* Tex = LevelData.ImgAsset.IsValid() ? LevelData.ImgAsset.Get() : LevelData.ImgAsset.LoadSynchronous();
            if (Tex)
            {
                IMG_MachineIcon->SetBrushFromTexture(Tex);
            }
        }
    }

    if (BTN_Card)
    {
        BTN_Card->SetIsEnabled(bHasLevelData && NodeLevel == CurrentGlobalLevel + 1);
    }

    if (!B_HighlightBorder)
    {
        return;
    }

    B_HighlightBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    if (CurrentGlobalLevel == NodeLevel)
    {
        B_HighlightBorder->SetBrushColor(FLinearColor(0.0f, 0.45f, 1.0f, 1.0f));
        if (IMG_MachineIcon)
        {
            IMG_MachineIcon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
        }
    }
    else if (CurrentGlobalLevel > NodeLevel)
    {
        B_HighlightBorder->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
        if (IMG_MachineIcon)
        {
            IMG_MachineIcon->SetColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f, 0.8f));
        }
    }
    else
    {
        B_HighlightBorder->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
        if (IMG_MachineIcon)
        {
            IMG_MachineIcon->SetColorAndOpacity(FLinearColor(0.15f, 0.15f, 0.15f, 0.6f));
        }
    }
}

void UUI_UpgradeNode::OnCardClicked()
{
    UGameInstance* GI = GetGameInstance();
    UMachineSubsystem* MachineSubsystem = GI ? GI->GetSubsystem<UMachineSubsystem>() : nullptr;
    if (!MachineSubsystem)
    {
        return;
    }

    const int32 CurrentGlobalLevel = MachineSubsystem->GetMachineLevel(MachineType);
    if (NodeLevel != CurrentGlobalLevel + 1)
    {
        return;
    }

    AOJJ_Player* Player = Cast<AOJJ_Player>(GetOwningPlayerPawn());
    if (!Player)
    {
        return;
    }

    Player->UpgradeMachineLevel(MachineType.ToString(), 1);

    if (MachineSubsystem->GetMachineLevel(MachineType) != NodeLevel)
    {
        return;
    }

    if (UUI_BaseCampInteract* MainBaseCampUI = GetTypedOuter<UUI_BaseCampInteract>())
    {
        MainBaseCampUI->RefreshAllUpgradeNodes();
        return;
    }

    RefreshNodeState();
}
