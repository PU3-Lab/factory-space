#include "UI_UpgradeNode.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Machines/MachineSubsystem.h"
#include "Machines/MachineTable.h"
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

    int32 CurrentGlobalLevel = MachineSubsystem->GetMachineLevel(MachineType);

    FMachineTableRow LevelData;
    if (MachineSubsystem->FindMachineDataForLevel(MachineType, NodeLevel, LevelData))
    {
        if (TXT_LevelLabel) TXT_LevelLabel->SetText(FText::FromString(FString::Printf(TEXT("%d레벨 %s"), NodeLevel, *LevelData.DisplayName)));
        if (IMG_MachineIcon)
        {
            UTexture2D* Tex = LevelData.ImgAsset.IsValid() ? LevelData.ImgAsset.Get() : LevelData.ImgAsset.LoadSynchronous();
            if (Tex) IMG_MachineIcon->SetBrushFromTexture(Tex);
        }
    }
    
    if (B_HighlightBorder)
    {
        // 카드 외곽 구역은 마우스 클릭을 통과시키고 눈에만 보이도록 고정
        B_HighlightBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

        if (CurrentGlobalLevel == NodeLevel)
        {
            // ① 현재 내 공장 가동 레벨인 경우 -> 네온 블루 하이라이트 활성화!
            B_HighlightBorder->SetBrushColor(FLinearColor(0.0f, 0.45f, 1.0f, 1.0f)); // 푸른 광채
            if (IMG_MachineIcon) IMG_MachineIcon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)); // 선명하게
        }
        else if (CurrentGlobalLevel > NodeLevel)
        {
            // ② 이미 마스터하고 지나간 과거 레벨 -> 테두리 불은 끄고 알맹이는 정상 노출
            B_HighlightBorder->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f)); // 테두리 투명화
            if (IMG_MachineIcon) IMG_MachineIcon->SetColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f, 0.8f)); // 지나간 레벨 느낌의 반투명
        }
        else
        {
            // ③ 아직 해금되지 않은 미래 잠김 레벨 -> 테두리 끄고 아이콘을 어둡게 블랙아웃 (SF 감성)
            B_HighlightBorder->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f)); // 테두리 투명화
            if (IMG_MachineIcon) IMG_MachineIcon->SetColorAndOpacity(FLinearColor(0.15f, 0.15f, 0.15f, 0.6f)); // 어둡게 잠금 연출
        }
    }
}

void UUI_UpgradeNode::OnCardClicked()
{
    UGameInstance* GI = GetGameInstance();
    UMachineSubsystem* MachineSubsystem = GI ? GI->GetSubsystem<UMachineSubsystem>() : nullptr;
    if (!MachineSubsystem) return;

    int32 CurrentGlobalLevel = MachineSubsystem->GetMachineLevel(MachineType);
    
    // 현재 글로벌 기계 레벨의 바로 다음 레벨을 클릭했을 때만 업그레이드 기믹 활성화
    if (CurrentGlobalLevel + 1 == NodeLevel)
    {
        if (MachineSubsystem->UpgradeMachineType(MachineType))
        {
            // UMG 위젯 트리 장부를 2단계 역추적(GetOuter->GetOuter)
            if (GetOuter() && GetOuter()->GetOuter())
            {
                UUI_BaseCampInteract* MainBaseCampUI = Cast<UUI_BaseCampInteract>(GetOuter()->GetOuter());
                if (MainBaseCampUI)
                {
                    // 카드 하나가 성공적으로 레벨업 되었으니 메인 화면에 깔린 12개 노드 전체를 실시간 동시 불빛 동기화
                    MainBaseCampUI->RefreshAllUpgradeNodes();
                }
            }

            // 폴백 처리로 내 카드 자체도 즉시 새로고침
            RefreshNodeState();
        }
    }
}