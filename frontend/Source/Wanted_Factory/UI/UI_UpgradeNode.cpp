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
    if (!MachineSubsystem) return;

    const int32 CurrentGlobalLevel = MachineSubsystem->GetMachineLevel(MachineType);

    FMachineTableRow LevelData;
    const bool bHasLevelData = MachineSubsystem->FindMachineDataForLevel(MachineType, NodeLevel, LevelData);
    
    // 데이터 테이블 기반 이름 및 아이콘 드로잉 (유저님 기존 무결한 로직 유지)
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

    if (!B_HighlightBorder || !BTN_Card) return;

    // 기본적으로 하이라이트 판넬은 항상 자리를 유지하도록 설정
    B_HighlightBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);


    // 컴포넌트 비주얼 & 가시성 상태 머신

    // 1. 다음 업그레이드 대상 카드 ➡️ 클릭 가능, 하이라이트 없음, 선명함
    if (bHasLevelData && NodeLevel == CurrentGlobalLevel + 1)
    {
        BTN_Card->SetIsEnabled(true);
        BTN_Card->SetVisibility(ESlateVisibility::Visible); // 마우스 클릭 수신 가능 상태
        
        B_HighlightBorder->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f)); // 배경 투명
        
        if (IMG_MachineIcon)
        {
            IMG_MachineIcon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
        }
    }
    // 2. 현재 활성화된 최고 레벨 카드 ➡️ 클릭 불가, 파란 하이라이트 활성화, 아이콘 선명
    else if (CurrentGlobalLevel == NodeLevel)
    {
        // Visibility만 HitTestInvisible로 꺾어서 클릭 상호작용만 완벽히 차단합니다.
        BTN_Card->SetIsEnabled(true);
        BTN_Card->SetVisibility(ESlateVisibility::HitTestInvisible); 
        
        B_HighlightBorder->SetBrushColor(FLinearColor(0.0f, 0.45f, 1.0f, 1.0f)); // 정갈한 파란 불빛 가동
        
        if (IMG_MachineIcon)
        {
            IMG_MachineIcon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)); // 오염 없는 100% 원본 화질
        }
    }
    // 3. 이미 지나간 하위 레벨 카드들 ➡️ 클릭 불가, 하이라이트 없음, 어둡게 처리
    else if (CurrentGlobalLevel > NodeLevel)
    {
        BTN_Card->SetIsEnabled(false); // 하위 단계는 내장 disabled 필터로 자연스럽게 어둡게 유도
        BTN_Card->SetVisibility(ESlateVisibility::HitTestInvisible);
        
        B_HighlightBorder->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
        
        if (IMG_MachineIcon)
        {
            IMG_MachineIcon->SetColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.4f, 0.7f));
        }
    }
    // 4. 아직 도달하지 못한 잠긴 카드들 ➡️ 클릭 불가, 하이라이트 없음, 매우 어둡게 처리
    else
    {
        BTN_Card->SetIsEnabled(false);
        BTN_Card->SetVisibility(ESlateVisibility::HitTestInvisible);
        
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
