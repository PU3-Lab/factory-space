#include "UI_BuildModeMain.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"
#include "Components/ContentWidget.h"
#include "Machines/MachineSubsystem.h"
#include "QuestManagerSubsystem.h"
#include "UI_QuestWindow.h"
#include "UI_SimpleQuestWindow.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/WidgetAnimation.h"

namespace
{
    void CollectImageWidgetsRecursive(UWidget* Widget, TArray<UImage*>& OutImages)
    {
       if (!Widget) return;
       if (UImage* Image = Cast<UImage>(Widget))
       {
          OutImages.Add(Image);
          return;
       }
       if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
       {
          for (int32 ChildIndex = 0; ChildIndex < Panel->GetChildrenCount(); ++ChildIndex)
          {
             CollectImageWidgetsRecursive(Panel->GetChildAt(ChildIndex), OutImages);
          }
          return;
       }
       if (UContentWidget* Content = Cast<UContentWidget>(Widget))
       {
          CollectImageWidgetsRecursive(Content->GetContent(), OutImages);
       }
    }

    void ApplyMachineIconToButton(UButton* Button, const FMachineTableRow& MachineData)
    {
        if (!Button) return;
        UTexture2D* Texture = MachineData.ImgAsset.IsValid()
           ? MachineData.ImgAsset.Get()
           : MachineData.ImgAsset.LoadSynchronous();
        if (!Texture) return;

        TArray<UImage*> Images;
        CollectImageWidgetsRecursive(Button->GetContent(), Images);
        for (UImage* Image : Images)
        {
            if (Image) Image->SetBrushFromTexture(Texture);
        }
        
        if (Images.Num() == 0)
        {
            FButtonStyle ButtonStyle = Button->GetStyle();
            ButtonStyle.Normal.SetResourceObject(Texture);
            ButtonStyle.Hovered.SetResourceObject(Texture);
            ButtonStyle.Pressed.SetResourceObject(Texture);
            ButtonStyle.Disabled.SetResourceObject(Texture);
            Button->SetStyle(ButtonStyle);
        }
    }
}

void UUI_BuildModeMain::NativeConstruct()
{
    Super::NativeConstruct();
    
    if (BTN_SubMode_Machine)   BTN_SubMode_Machine->OnClicked.AddDynamic(this, &UUI_BuildModeMain::OnMachineModeClicked);
    if (BTN_SubMode_Power)     BTN_SubMode_Power->OnClicked.AddDynamic(this, &UUI_BuildModeMain::OnPowerModeClicked);
    if (BTN_SubMode_Structure) BTN_SubMode_Structure->OnClicked.AddDynamic(this, &UUI_BuildModeMain::OnStructureModeClicked);

    // 1~10번 슬롯 컴포넌트 자동 수집구역
    HotbarButtons.Empty();
    HotbarIcons.Empty();
    HotbarTexts.Empty(); // 초기화 추가

    for (int i = 1; i <= 10; ++i)
    {
        FString ButtonName = FString::Printf(TEXT("BTN_Slot_%d"), i);
        FString IconName = FString::Printf(TEXT("IMG_Icon_%d"), i);
        FString TextName = FString::Printf(TEXT("TXT_Name_%d"), i); // 이름 규칙 긁어오기

        UButton* SlotBtn = Cast<UButton>(GetWidgetFromName(*ButtonName));
        UImage* SlotImg = Cast<UImage>(GetWidgetFromName(*IconName));
        UTextBlock* SlotTxt = Cast<UTextBlock>(GetWidgetFromName(*TextName)); // 텍스트 컴포넌트 탐색

        if (SlotBtn && SlotImg)
        {
            HotbarButtons.Add(SlotBtn);
            HotbarIcons.Add(SlotImg);
        }
        
        // 발견된 텍스트 컴포넌트를 무결하게 캐싱합니다
        if (SlotTxt)
        {
            HotbarTexts.Add(SlotTxt);
        }
    }

    if (HotbarButtons.IsValidIndex(0)) HotbarButtons[0]->OnClicked.AddDynamic(this, &UUI_BuildModeMain::OnSlot1Clicked);
    if (HotbarButtons.IsValidIndex(1)) HotbarButtons[1]->OnClicked.AddDynamic(this, &UUI_BuildModeMain::OnSlot2Clicked);
    if (HotbarButtons.IsValidIndex(2)) HotbarButtons[2]->OnClicked.AddDynamic(this, &UUI_BuildModeMain::OnSlot3Clicked);
    if (HotbarButtons.IsValidIndex(3)) HotbarButtons[3]->OnClicked.AddDynamic(this, &UUI_BuildModeMain::OnSlot4Clicked);
    if (HotbarButtons.IsValidIndex(4)) HotbarButtons[4]->OnClicked.AddDynamic(this, &UUI_BuildModeMain::OnSlot5Clicked);
    if (HotbarButtons.IsValidIndex(5)) HotbarButtons[5]->OnClicked.AddDynamic(this, &UUI_BuildModeMain::OnSlot6Clicked);
    if (HotbarButtons.IsValidIndex(6)) HotbarButtons[6]->OnClicked.AddDynamic(this, &UUI_BuildModeMain::OnSlot7Clicked);
    if (HotbarButtons.IsValidIndex(7)) HotbarButtons[7]->OnClicked.AddDynamic(this, &UUI_BuildModeMain::OnSlot8Clicked);
    if (HotbarButtons.IsValidIndex(8)) HotbarButtons[8]->OnClicked.AddDynamic(this, &UUI_BuildModeMain::OnSlot9Clicked);
    if (HotbarButtons.IsValidIndex(9)) HotbarButtons[9]->OnClicked.AddDynamic(this, &UUI_BuildModeMain::OnSlot10Clicked);

    if (Anim_HotbarOut)
    {
        FWidgetAnimationDynamicEvent EndEvent;
        EndEvent.BindDynamic(this, &UUI_BuildModeMain::OnHotbarOutFinished);
        BindToAnimationFinished(Anim_HotbarOut, EndEvent);
    }
    
    if (IMG_SelectedPreview) IMG_SelectedPreview->SetVisibility(ESlateVisibility::Collapsed);
    if (TXT_SelectedName)    TXT_SelectedName->SetVisibility(ESlateVisibility::Collapsed);
    
    InitializeHotbarRegistry();
    RefreshHotbarSlotsVisual();
    RefreshQuestWindowMode();

    if (UGameInstance* GI = GetGameInstance())
    {
        if (UQuestManagerSubsystem* QuestManager = GI->GetSubsystem<UQuestManagerSubsystem>())
        {
            QuestManager->OnTutorialStepChanged.RemoveDynamic(this, &UUI_BuildModeMain::HandleTutorialStepChangedForSimpleQuest);
            QuestManager->OnTutorialStepChanged.AddDynamic(this, &UUI_BuildModeMain::HandleTutorialStepChangedForSimpleQuest);
            QuestManager->OnTutorialDialogueLogged.RemoveDynamic(this, &UUI_BuildModeMain::HandleTutorialDialogueLoggedForQuestWindow);
            QuestManager->OnTutorialDialogueLogged.AddDynamic(this, &UUI_BuildModeMain::HandleTutorialDialogueLoggedForQuestWindow);
            RefreshSimpleQuestWindow();
        }
    }
}

void UUI_BuildModeMain::NativeDestruct()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UQuestManagerSubsystem* QuestManager = GI->GetSubsystem<UQuestManagerSubsystem>())
        {
            QuestManager->OnTutorialStepChanged.RemoveDynamic(this, &UUI_BuildModeMain::HandleTutorialStepChangedForSimpleQuest);
            QuestManager->OnTutorialDialogueLogged.RemoveDynamic(this, &UUI_BuildModeMain::HandleTutorialDialogueLoggedForQuestWindow);
        }
    }

    Super::NativeDestruct();
}

void UUI_BuildModeMain::RefreshQuestWindowMode()
{
    UGameInstance* GI = GetGameInstance();
    UQuestManagerSubsystem* QuestManager = GI ? GI->GetSubsystem<UQuestManagerSubsystem>() : nullptr;

    const bool bShowFullQuestWindow =
        QuestManager && QuestManager->IsFullQuestWindowUnlocked();

    if (WBP_QuestWindow)
    {
        WBP_QuestWindow->SetVisibility(
            bShowFullQuestWindow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    }

    if (WBP_SimpleQuestWindow)
    {
        WBP_SimpleQuestWindow->SetVisibility(
            bShowFullQuestWindow ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
    }
}

void UUI_BuildModeMain::RefreshSimpleQuestWindow()
{
    if (!WBP_SimpleQuestWindow)
    {
        return;
    }

    UGameInstance* GI = GetGameInstance();
    UQuestManagerSubsystem* QuestManager = GI ? GI->GetSubsystem<UQuestManagerSubsystem>() : nullptr;
    if (!QuestManager)
    {
        return;
    }

    FTutorialQuestStep CurrentStep;
    if (QuestManager->HasPendingTutorialStartDialogue())
    {
        FString LoggedQuestId;
        FString LoggedTriggerType;
        TArray<FTutorialQuestDialogueLine> LoggedLines;
        QuestManager->GetLastTutorialDialogueLog(LoggedQuestId, LoggedTriggerType, LoggedLines);

        if (LoggedTriggerType == TEXT("on_complete")
            && QuestManager->GetTutorialQuestStepById(LoggedQuestId, CurrentStep))
        {
            WBP_SimpleQuestWindow->UpdateSimpleQuest(
                FText::FromString(CurrentStep.Title),
                FText::FromString(QuestManager->GetTutorialStepDisplayDescription(CurrentStep)));
        }
        return;
    }

    if (!QuestManager->GetCurrentTutorialQuestStep(CurrentStep))
    {
        return;
    }

    WBP_SimpleQuestWindow->UpdateSimpleQuest(
        FText::FromString(CurrentStep.Title),
        FText::FromString(QuestManager->GetTutorialStepDisplayDescription(CurrentStep)));
}

void UUI_BuildModeMain::HandleTutorialStepChangedForSimpleQuest(const FTutorialQuestStep& Step)
{
    if (!WBP_SimpleQuestWindow)
    {
        return;
    }

    UGameInstance* GI = GetGameInstance();
    UQuestManagerSubsystem* QuestManager = GI ? GI->GetSubsystem<UQuestManagerSubsystem>() : nullptr;

    WBP_SimpleQuestWindow->UpdateSimpleQuest(
        FText::FromString(Step.Title),
        FText::FromString(QuestManager ? QuestManager->GetTutorialStepDisplayDescription(Step) : Step.Description));

    RefreshQuestWindowMode();
}

void UUI_BuildModeMain::HandleTutorialDialogueLoggedForQuestWindow(const FString& QuestId, const FString& TriggerType, const TArray<FTutorialQuestDialogueLine>& Lines)
{
    RefreshQuestWindowMode();
    RefreshSimpleQuestWindow();
}

void UUI_BuildModeMain::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    RefreshHotbarSlotsVisual();
    UpdateSelectedPreview();
}

// 카테고리 버튼 클릭은 슬롯1 자동선택을 하지 않음(기존 UX). 순환(←/→) 애니 진행 중 버튼이 끼어들면
// 직전 CycleSubMode가 세운 플래그를 이 클릭의 OnHotbarOutFinished가 소비해 오발동하므로, 진입부에서 명시적으로 끈다.
void UUI_BuildModeMain::OnMachineModeClicked()   { bAutoSelectFirstSlotOnSwitch = false; SwitchBuildSubMode(EBuildSubMode::Machine); }
void UUI_BuildModeMain::OnPowerModeClicked()     { bAutoSelectFirstSlotOnSwitch = false; SwitchBuildSubMode(EBuildSubMode::Power); }
void UUI_BuildModeMain::OnStructureModeClicked() { bAutoSelectFirstSlotOnSwitch = false; SwitchBuildSubMode(EBuildSubMode::Structure); }

void UUI_BuildModeMain::SwitchBuildSubMode(EBuildSubMode NewMode)
{
    if (CurrentSubMode == NewMode) return;
    PendingSubMode = NewMode;

    if (Anim_HotbarOut) PlayAnimation(Anim_HotbarOut);
    else OnHotbarOutFinished();
}

void UUI_BuildModeMain::OnHotbarOutFinished()
{
    CurrentSubMode = PendingSubMode;
    CachedMachineLevels.Empty();
    RefreshHotbarSlotsVisual();

    // [카테고리 순환] 순환 경로(←/→)로 전환된 경우에만 새 카테고리의 슬롯1 자동선택.
    // CurrentSubMode가 방금 갱신됐으므로 ExecutePlacementMode(1)이 정확히 새 카테고리 기준으로 해석됨.
    // (CycleSubMode 직후가 아니라 여기서 부르는 이유: out-애니 완료 전엔 CurrentSubMode가 이전 값)
    if (bAutoSelectFirstSlotOnSwitch)
    {
        bAutoSelectFirstSlotOnSwitch = false;
        ExecutePlacementMode(1);
    }

    if (Anim_HotbarIn) PlayAnimation(Anim_HotbarIn);
}

void UUI_BuildModeMain::CycleSubMode(int32 Dir)
{
    // Machine(0)↔Power(1)↔Structure(2) 3분류 래핑. +1=다음, +2=(-1 등가) 이전 — uint8 언더플로 없이 역순.
    const uint8 Cur = (uint8)CurrentSubMode;
    const uint8 Nxt = (uint8)((Cur + (Dir > 0 ? 1 : 2)) % 3);
    if ((EBuildSubMode)Nxt == CurrentSubMode) return; // 동일 카테고리면 무동작(이론상 없음)

    bAutoSelectFirstSlotOnSwitch = true; // OnHotbarOutFinished에서 슬롯1 자동선택 게이트
    SwitchBuildSubMode((EBuildSubMode)Nxt);
}

void UUI_BuildModeMain::InitializeHotbarRegistry()
{
    SubModeHotbarRegistry.Empty();

    // 🤖 [1. 기계 설치 모드 한글 이름표 바인딩]
    TArray<FHotbarSlotData>& MachineHotbar = SubModeHotbarRegistry.FindOrAdd(EBuildSubMode::Machine);
    MachineHotbar.SetNum(10);
    MachineHotbar[0] = { EOJJ_BuildPlacementMode::Conveyor,       TEXT("Conveyor"),       FText::FromString(TEXT("컨베이어")) };
    MachineHotbar[1] = { EOJJ_BuildPlacementMode::Warehouse,      TEXT("WarehousePort"),  FText::FromString(TEXT("창고")) };
    MachineHotbar[2] = { EOJJ_BuildPlacementMode::Smelter,        TEXT("Smelter"),        FText::FromString(TEXT("제련기")) };
    MachineHotbar[3] = { EOJJ_BuildPlacementMode::Grinder,        TEXT("Grinder"),        FText::FromString(TEXT("분쇄기")) };
    MachineHotbar[4] = { EOJJ_BuildPlacementMode::MoldingMachine, TEXT("MoldingMachine"), FText::FromString(TEXT("성형기")) };
    MachineHotbar[5] = { EOJJ_BuildPlacementMode::Synthesizer,    TEXT("Synthesizer"),    FText::FromString(TEXT("합성기")) };
    MachineHotbar[6] = { EOJJ_BuildPlacementMode::Miner,          TEXT("MinerMachine"),   FText::FromString(TEXT("채굴기")) };
    MachineHotbar[7] = { EOJJ_BuildPlacementMode::Pump,           TEXT("Pump"),           FText::FromString(TEXT("펌프")) };
    MachineHotbar[8] = { EOJJ_BuildPlacementMode::LiquidTank,     TEXT("LiquidTank"),     FText::FromString(TEXT("액체 탱크")) };
    MachineHotbar[9] = { EOJJ_BuildPlacementMode::Pipe,           TEXT("Pipe"),           FText::FromString(TEXT("파이프")) };

    // ⚡ [2. 전력 설치 모드 한글 이름표 바인딩]
    TArray<FHotbarSlotData>& PowerHotbar = SubModeHotbarRegistry.FindOrAdd(EBuildSubMode::Power);
    PowerHotbar.SetNum(10);
    PowerHotbar[0] = { EOJJ_BuildPlacementMode::PowerLine,  TEXT("Cable"),  FText::FromString(TEXT("전선")) };
    PowerHotbar[1] = { EOJJ_BuildPlacementMode::PowerNode,  TEXT("PowerGridNode"),  FText::FromString(TEXT("송전탑")) };
    PowerHotbar[2] = { EOJJ_BuildPlacementMode::PowerPlant, TEXT("PowerPlant"), FText::FromString(TEXT("기본 발전소")) };
    // 4~10번 자리는 미래에 구현될 수력, 화력 발전소 등을 위해 깔끔하게 빈 칸 유지

    // 🏢 [3. 건물 설치 모드 한글 이름표 바인딩]
    TArray<FHotbarSlotData>& StructHotbar = SubModeHotbarRegistry.FindOrAdd(EBuildSubMode::Structure);
    StructHotbar.SetNum(10);
    StructHotbar[0] = { EOJJ_BuildPlacementMode::BaseCamp,               TEXT("BaseCamp"), FText::FromString(TEXT("중앙 거점")) };
    StructHotbar[1] = { EOJJ_BuildPlacementMode::TeleCommunicationTower, TEXT("TeleCommunicationTower"), FText::FromString(TEXT("통신탑")) };
    StructHotbar[2] = { EOJJ_BuildPlacementMode::SignalAmplifier,        TEXT("Signal_Amplifier"), FText::FromString(TEXT("신호증폭기")) };
    StructHotbar[8] = { EOJJ_BuildPlacementMode::Shield,                 TEXT("MagneticShield"),    FText::FromString(TEXT("차폐막")) };
}

void UUI_BuildModeMain::RefreshHotbarSlotsVisual()
{
    UGameInstance* GameInstance = GetGameInstance();
    UMachineSubsystem* MachineSubsystem = GameInstance ? GameInstance->GetSubsystem<UMachineSubsystem>() : nullptr;
    if (!MachineSubsystem) return;

    const TArray<FHotbarSlotData>* ActiveHotbarData = SubModeHotbarRegistry.Find(CurrentSubMode);
    if (!ActiveHotbarData) return;

    for (int32 i = 0; i < HotbarButtons.Num(); ++i)
    {
        if (!ActiveHotbarData->IsValidIndex(i)) continue;

        const FHotbarSlotData& SlotData = (*ActiveHotbarData)[i];
        UButton* TargetBtn = HotbarButtons[i];
        UImage* TargetImg = HotbarIcons[i];
        
        // 실시간 스왑용 텍스트 컴포넌트 안전 획득
        UTextBlock* TargetTxt = HotbarTexts.IsValidIndex(i) ? HotbarTexts[i] : nullptr;

        // 슬롯 정보가 통째로 비어있는 미래 확장용 예약 칸 구조라면 텍스트와 이미지 숨김
        if (SlotData.SubsystemKey.IsNone() && SlotData.DisplayName.IsEmpty())
        {
            if (TargetImg) TargetImg->SetVisibility(ESlateVisibility::Hidden);
            if (TargetTxt) TargetTxt->SetVisibility(ESlateVisibility::Hidden);
            continue;
        }

        // 장부에 적어둔 이쁜 한글 이름표로 텍스트를 즉시 갈아끼웁니다
        if (TargetTxt)
        {
            TargetTxt->SetVisibility(ESlateVisibility::Visible);
            TargetTxt->SetText(SlotData.DisplayName);
        }

        // 서브시스템 키가 없는 순수 Placeholder 가이드용 건물("중앙거점" 등)은 이미지만 가려줍니다.
        if (SlotData.SubsystemKey.IsNone())
        {
            if (TargetImg) TargetImg->SetVisibility(ESlateVisibility::Hidden);
            continue;
        }

        // 아이콘 이미지 파일 실시간 리트리브 및 캐싱 처리 구역
        const int32 CurrentLevel = MachineSubsystem->GetMachineLevel(SlotData.SubsystemKey);
        const int32 CachedLevel = CachedMachineLevels.FindRef(SlotData.SubsystemKey);

        if (CurrentLevel != CachedLevel || CachedLevel == 0)
        {
            FMachineTableRow MachineData;
            if (MachineSubsystem->FindMachineData(SlotData.SubsystemKey, MachineData))
            {
                if (TargetImg) TargetImg->SetVisibility(ESlateVisibility::Visible);
                ApplyMachineIconToButton(TargetBtn, MachineData);
                CachedMachineLevels.FindOrAdd(SlotData.SubsystemKey) = CurrentLevel;
            }
        }
    }
}

void UUI_BuildModeMain::ExecutePlacementMode(int32 SlotIndex)
{
    AOJJ_BuildController* BuildController = Cast<AOJJ_BuildController>(
       UGameplayStatics::GetActorOfClass(GetWorld(), AOJJ_BuildController::StaticClass()));
    if (!BuildController) return;

    const TArray<FHotbarSlotData>* ActiveHotbarData = SubModeHotbarRegistry.Find(CurrentSubMode);
    if (!ActiveHotbarData || !ActiveHotbarData->IsValidIndex(SlotIndex - 1)) return;

    const FHotbarSlotData& FinalTargetSlot = (*ActiveHotbarData)[SlotIndex - 1];

    if (FinalTargetSlot.SubsystemKey.IsNone() && FinalTargetSlot.DisplayName.IsEmpty()) return;

    BuildController->SetPlacementMode(FinalTargetSlot.PlacementMode);

    UGameInstance* GameInstance = GetGameInstance();
    UQuestManagerSubsystem* QuestManager = GameInstance ? GameInstance->GetSubsystem<UQuestManagerSubsystem>() : nullptr;
    if (!QuestManager) return;

    if (!FinalTargetSlot.SubsystemKey.IsNone())
    {
        FString EventString;
        if (FinalTargetSlot.SubsystemKey == TEXT("WarehousePort"))
        {
            EventString = TEXT("SelectWarehouseMode");
        }
        else
        {
            EventString = FString::Printf(TEXT("Select%sMode"), *FinalTargetSlot.SubsystemKey.ToString());
        }
        QuestManager->NotifyTutorialEvent(*EventString);
    }
}

void UUI_BuildModeMain::UpdateSelectedPreview()
{
    if (!IMG_SelectedPreview || !TXT_SelectedName) return;

    AOJJ_BuildController* BuildController = Cast<AOJJ_BuildController>(
       UGameplayStatics::GetActorOfClass(GetWorld(), AOJJ_BuildController::StaticClass()));
    
    if (!BuildController || !BuildController->IsInBuildMode())
    {
        IMG_SelectedPreview->SetVisibility(ESlateVisibility::Collapsed);
        TXT_SelectedName->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    EOJJ_BuildPlacementMode CurrentMode = BuildController->GetPlacementMode();

    // 가드 조건에서 Foundation(바닥)을 완벽히 제거합니다.
    // 그래야 F, G 키를 눌렀을 때 프리뷰 칸이 강제로 Collapsed 되지 않고 로직이 정상 작동합니다.
    if (CurrentMode == EOJJ_BuildPlacementMode::Demolish)
    {
        IMG_SelectedPreview->SetVisibility(ESlateVisibility::Collapsed);
        TXT_SelectedName->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    FName TargetDTKey = NAME_None;
    switch (CurrentMode)
    {
        case EOJJ_BuildPlacementMode::Conveyor:       TargetDTKey = TEXT("Conveyor"); break;
        case EOJJ_BuildPlacementMode::Warehouse:      TargetDTKey = TEXT("WarehousePort"); break;
        case EOJJ_BuildPlacementMode::Smelter:        TargetDTKey = TEXT("Smelter"); break;
        case EOJJ_BuildPlacementMode::Grinder:        TargetDTKey = TEXT("Grinder"); break;
        case EOJJ_BuildPlacementMode::MoldingMachine: TargetDTKey = TEXT("MoldingMachine"); break;
        case EOJJ_BuildPlacementMode::Synthesizer:    TargetDTKey = TEXT("Synthesizer"); break;
        case EOJJ_BuildPlacementMode::Miner:          TargetDTKey = TEXT("MinerMachine"); break;
        case EOJJ_BuildPlacementMode::Pump:           TargetDTKey = TEXT("Pump"); break;
        case EOJJ_BuildPlacementMode::LiquidTank:     TargetDTKey = TEXT("LiquidTank"); break;
        case EOJJ_BuildPlacementMode::Pipe:           TargetDTKey = TEXT("Pipe"); break;
        case EOJJ_BuildPlacementMode::PowerLine:      TargetDTKey = TEXT("Cable"); break;
        case EOJJ_BuildPlacementMode::PowerNode:      TargetDTKey = TEXT("PowerGridNode"); break;
        case EOJJ_BuildPlacementMode::PowerPlant:     TargetDTKey = TEXT("PowerPlant"); break;
        case EOJJ_BuildPlacementMode::TeleCommunicationTower: TargetDTKey = TEXT("TeleCommunicationTower"); break;
        case EOJJ_BuildPlacementMode::BaseCamp:       TargetDTKey = TEXT("BaseCamp"); break;
        case EOJJ_BuildPlacementMode::SignalAmplifier: TargetDTKey = TEXT("Signal_Amplifier"); break;
        case EOJJ_BuildPlacementMode::Shield:         TargetDTKey = TEXT("MagneticShield"); break;
        
        // [사다리 H 분기 추가]
        case EOJJ_BuildPlacementMode::Ladder:         
            TargetDTKey = TEXT("Ladder"); 
            break; 

        // [바닥 F, G 분기 추가] 1단계에서 개방한 퍼블릭 함수로 정확하게 분기합니다.
        case EOJJ_BuildPlacementMode::Foundation:
            {
                if (BuildController->IsRampFoundationSelected()) 
                {
                    TargetDTKey = TEXT("SlantedFloor"); // G 키를 누른 경사 바닥 상태일 때
                }
                else
                {
                    TargetDTKey = TEXT("FlatFloor");    // F 키를 누른 평평한 바닥 상태일 때
                }
            }
            break;

        default: break;
    }

    if (TargetDTKey.IsNone())
    {
        IMG_SelectedPreview->SetVisibility(ESlateVisibility::Collapsed);
        TXT_SelectedName->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }
    
    UGameInstance* GameInstance = GetGameInstance();
    UMachineSubsystem* MachineSubsystem = GameInstance ? GameInstance->GetSubsystem<UMachineSubsystem>() : nullptr;
    
    FMachineTableRow MachineData;
    if (MachineSubsystem && MachineSubsystem->FindMachineData(TargetDTKey, MachineData))
    {
        FText SelectedItemDisplayName = FText::GetEmpty();
        for (const auto& Pair : SubModeHotbarRegistry)
        {
            for (const FHotbarSlotData& SlotData : Pair.Value)
            {
                if (SlotData.PlacementMode == CurrentMode)
                {
                    SelectedItemDisplayName = SlotData.DisplayName;
                    break;
                }
            }
            if (!SelectedItemDisplayName.IsEmpty()) break;
        }

        // 구조체 실제 멤버 변수명인 DisplayName(FString)을 FText로 안전하게 변환하여 매핑합니다.
        if (SelectedItemDisplayName.IsEmpty())
        {
            SelectedItemDisplayName = FText::FromString(MachineData.DisplayName); 
        }

        if (!SelectedItemDisplayName.IsEmpty())
        {
            TXT_SelectedName->SetVisibility(ESlateVisibility::Visible);
            TXT_SelectedName->SetText(SelectedItemDisplayName);
        }
        else
        {
            TXT_SelectedName->SetVisibility(ESlateVisibility::Collapsed);
        }

        UTexture2D* PreviewTexture = MachineData.ImgAsset.IsValid() ? MachineData.ImgAsset.Get() : MachineData.ImgAsset.LoadSynchronous();
        if (PreviewTexture)
        {
            IMG_SelectedPreview->SetVisibility(ESlateVisibility::Visible);
            IMG_SelectedPreview->SetBrushFromTexture(PreviewTexture);
        }
        else
        {
            IMG_SelectedPreview->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
    else
    {
        IMG_SelectedPreview->SetVisibility(ESlateVisibility::Collapsed);
        TXT_SelectedName->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UUI_BuildModeMain::OnSlot1Clicked()  { ExecutePlacementMode(1); }
void UUI_BuildModeMain::OnSlot2Clicked()  { ExecutePlacementMode(2); }
void UUI_BuildModeMain::OnSlot3Clicked()  { ExecutePlacementMode(3); }
void UUI_BuildModeMain::OnSlot4Clicked()  { ExecutePlacementMode(4); }
void UUI_BuildModeMain::OnSlot5Clicked()  { ExecutePlacementMode(5); }
void UUI_BuildModeMain::OnSlot6Clicked()  { ExecutePlacementMode(6); }
void UUI_BuildModeMain::OnSlot7Clicked()  { ExecutePlacementMode(7); }
void UUI_BuildModeMain::OnSlot8Clicked()  { ExecutePlacementMode(8); }
void UUI_BuildModeMain::OnSlot9Clicked()  { ExecutePlacementMode(9); }
void UUI_BuildModeMain::OnSlot10Clicked() { ExecutePlacementMode(10); }
