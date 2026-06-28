// Fill out your copyright notice in the Description page of Project Settings.


#include "OJJ_Player.h"

#include "Camera/CameraComponent.h"
#include "FactoryAgentClientSubsystem.h"
#include "FactorySaveSubsystem.h"
#include "PlanetEventManagerSubsystem.h"
#include "QuestManagerSubsystem.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Components/SpotLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "OJJ_CharacterSelectionSubsystem.h"
#include "OJJ_CharacterAppearanceData.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "OJJ_BuildController.h"
#include "OJJ_BuildCamera.h"
#include "OJJ_Ladder.h"
#include "Components/CapsuleComponent.h"
#include "OJJ_Grid.h"
#include "Blueprint/UserWidget.h"
#include "MachineBase.h"
#include "UI/UI_MachineInteract.h"
#include "UI/UI_BuildModeMain.h"
#include "UI/UI_MainHUD.h"
#include "UI/UI_Inventory.h"
#include "UI/UI_WarehouseInteract.h"
#include "UI/UI_QuestWindow.h"
#include "UI/UI_DialogueBalloon.h"
#include "UI/UI_SynthesizerInteract.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/EditableText.h"
#include "Machines/MachineSubsystem.h"
#include "Machines/LiquidTank.h"
#include "Machines/WarehousePort.h"
#include "PlayerWarehouseSubsystem.h"

AOJJ_Player::AOJJ_Player()
{
	// step-off 안착 보간(#184)을 위해 Tick 사용. 평상시 Tick 본문은 bSteppingOff 가드로 즉시 반환.
	PrimaryActorTick.bCanEverTick = true;

	// 빌드 카메라 기본 클래스 = C++ 기본. BP 파생을 BP_OJJ_Player에서 지정하면 그걸로 spawn.
	BuildCameraClass = AOJJ_BuildCamera::StaticClass();

	// 카메라가 컨트롤러 회전을 따르고, 캐릭터 본체는 이동 방향으로 회전 (TPS 표준)
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 270.f;
	SpringArm->bUsePawnControlRotation = true; // 마우스 입력으로 카메라 회전
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 10.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false; // 회전은 SpringArm이 담당

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;          // 이동 방향으로 본체 회전
	Movement->RotationRate = FRotator(0.f, 540.f, 0.f);
	Movement->MaxWalkSpeed = WalkSpeed;                  // 기존 하드코딩 600 → 걷기 속도(단일 출처). BeginPlay에서 재확정.
	// [#357] 점프 높이 = JumpZVelocity. BP_OJJ_Player override 대신 C++ 단일 출처(메모리 원칙: BP override는
	// 멀티플레이 silent fail 위험 — 서버/클라 생성자값 보장). BP의 JumpZVelocity override는 제거해 이 값 상속.
	Movement->JumpZVelocity = 335.f;
	Movement->AirControl = 0.35f;

	NightSpotLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("NightSpotLight"));
	NightSpotLight->SetupAttachment(RootComponent);
	NightSpotLight->SetRelativeLocation(FVector(30.0f, 0.0f, 30.0f));
	NightSpotLight->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
	NightSpotLight->Intensity = 50.f;
	NightSpotLight->AttenuationRadius = 1500.f;
	NightSpotLight->InnerConeAngle = 30.0f;
	NightSpotLight->OuterConeAngle = 45.0f;
	NightSpotLight->bUseInverseSquaredFalloff = false;
	NightSpotLight->LightFalloffExponent = 2.5f;
	NightSpotLight->SetVisibility(false);
}

void AOJJ_Player::BeginPlay()
{
	Super::BeginPlay();

	// 걷기 속도를 권위 있게 적용(BP CharacterMovement의 MaxWalkSpeed 기본값을 덮음 — 단일 출처).
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = WalkSpeed;
	}

	// 로컬 플레이어 컨트롤러의 EnhancedInput 서브시스템에 매핑 컨텍스트 등록
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (IMC_Player)
			{
				Subsystem->AddMappingContext(IMC_Player, 0);
			}
		}

		// 카메라 상하 회전 제한 — 뒤집힘 방지 (bUsePawnControlRotation 카메라는 CameraManager가 pitch를 clamp)
		if (APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
		{
			CameraManager->ViewPitchMin = CameraPitchMin;
			CameraManager->ViewPitchMax = CameraPitchMax;
		}
	}

	// 레벨에 배치된 BuildController 인스턴스를 찾아 캐시 (spawn 하지 않음 —
	// 레벨 인스턴스에 와이어링된 TargetGrid/MachineClass 설정을 유지해야 하므로).
	BuildController = Cast<AOJJ_BuildController>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AOJJ_BuildController::StaticClass()));
	if (!BuildController)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[OJJ_Player] AOJJ_BuildController 인스턴스를 레벨에서 찾지 못함 — 빌드모드(B키) 비활성. ")
			TEXT("레벨에 AOJJ_BuildController가 배치되어 있고 TargetGrid/MachineClass가 설정됐는지 확인."));
	}

	// 빌드 탑다운 카메라는 spawn으로 생성 (수동 레벨 배치 불필요 — 진입 시 그리드 중심으로 자동 배치).
	if (UWorld* World = GetWorld())
	{
		const TSubclassOf<AOJJ_BuildCamera> SpawnClass =
			BuildCameraClass ? BuildCameraClass : TSubclassOf<AOJJ_BuildCamera>(AOJJ_BuildCamera::StaticClass());
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		BuildCamera = World->SpawnActor<AOJJ_BuildCamera>(SpawnClass, FTransform::Identity, SpawnParams);
	}
	if (!BuildCamera)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[OJJ_Player] AOJJ_BuildCamera spawn 실패 — 빌드모드 탑다운 전환 비활성."));
	}
	
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC && MainHUDWidgetClass)
	{
		MainHUDWidgetInstance = CreateWidget<UUserWidget>(PC, MainHUDWidgetClass);
		if (MainHUDWidgetInstance)
		{
			MainHUDWidgetInstance->AddToViewport();
		}
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
		{
			QuestManager->StartTutorialQuestTest();
		}

		if (UFactorySaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UFactorySaveSubsystem>())
		{
			SaveSubsystem->HandlePlayerReady(this);
		}
	}
	
	// [빌드 작업등] NightSpotLight 기본 강도/반경 캡처 — TPS 작업등 상향 후 밤 일반 점등으로 원복할 때 사용.
	if (NightSpotLight)
	{
		BaseNightLightIntensity = NightSpotLight->Intensity;
		BaseNightLightRadius = NightSpotLight->AttenuationRadius;
	}

	UpdateNightSpotLightVisibility();
	ConnectFactoryAgentClient();

	// [게임진입] 선택 캐릭터 외형 적용 — 세이브 로드(HandlePlayerReady→LoadCurrentGame)가 SetSelectedCharacter로
	// 캐릭터를 복원한 뒤 적용되도록 BeginPlay 말미로 이동(메시/ABP 확정). 외형/입력/카메라 setup은 메시와 무관해 안전.
	ApplySelectedCharacterAppearance();

	// [L_Planet 인트로] 시네마틱(L_Cinematic) 경유 진입 시에만 getup 몽타주 + 카메라 1인칭→3인칭 연출.
	// 외형 확정 직후 재생(올바른 ABP에서 getup 몽타주). 디버그 직접진입(플래그 false)은 스킵하고 평소 플레이.
	// 플래그는 연출 완료 시 PlayIntroSequence/Tick에서 소거(1회성).
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UOJJ_CharacterSelectionSubsystem* Selection = GameInstance->GetSubsystem<UOJJ_CharacterSelectionSubsystem>())
		{
			if (Selection->GetShouldPlayIntro())
			{
				PlayIntroSequence();
			}
		}
	}
}

void AOJJ_Player::ApplySelectedCharacterAppearance()
{
	// [게임진입] 선택 서브시스템값 → DataAsset 매핑 → GetMesh() 스왑. 어느 단계든 미존재면 안전 스킵
	// (AppearanceData 미할당/서브시스템 없음/항목 없음 → BP 기본 메시 유지). 외형만 — 로직 BP는 단일 유지.
	if (!AppearanceData)
	{
		return;
	}
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return;
	}
	UOJJ_CharacterSelectionSubsystem* Selection = GameInstance->GetSubsystem<UOJJ_CharacterSelectionSubsystem>();
	if (!Selection)
	{
		return;
	}
	const FOJJ_CharacterAppearance* Appearance = AppearanceData->Appearances.Find(Selection->GetSelectedCharacter());
	if (!Appearance)
	{
		return;
	}
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}
	// 메시·ABP 각각 비어 있으면 해당 스왑 스킵(부분 지정 허용 — 스켈레톤 동일 시 메시만, 다르면 ABP까지).
	if (Appearance->SkeletalMesh)
	{
		MeshComp->SetSkeletalMeshAsset(Appearance->SkeletalMesh);
		// [게임진입] 메시 스왑 직후 BP_OJJ_Player Mesh의 머티리얼 override(Man 전용 ManMat) 제거 —
		// SetSkeletalMeshAsset은 OverrideMaterials를 유지하므로, 안 비우면 Woman 메시에 ManMat가 덮여
		// 머티리얼이 깨진다(스켈레톤/UV mismatch). 비우면 각 메시 자체 머티리얼(ManMat/WomanMat)을 사용.
		MeshComp->EmptyOverrideMaterials();
	}
	if (Appearance->AnimClass)
	{
		MeshComp->SetAnimInstanceClass(Appearance->AnimClass);
	}
	// [게임진입] 캐릭터별 메시 Z 보정 — 메시 피벗(발바닥 원점)이 캐릭터마다 달라 단일 BP RelativeLocation.Z로는
	// 발이 뜬다(Woman). bOverrideMeshRelativeZ=true인 캐릭터만 X/Y는 BP 기본 유지하고 Z만 DA값으로 덮는다.
	// false면 BP 기본 Z 유지(Man 등 — 미설정 회귀 방지). 재호출 시에도 절대값 대입이라 누적 없음.
	if (Appearance->bOverrideMeshRelativeZ)
	{
		FVector MeshLoc = MeshComp->GetRelativeLocation();
		MeshLoc.Z = Appearance->MeshRelativeZ;
		MeshComp->SetRelativeLocation(MeshLoc);
	}
	// [게임진입] 캐릭터별 인트로 getup 몽타주 / 점프 시퀀스 교체. Man 전용 애님을 Woman_Skeleton에서 재생하면
	// 누우므로(스켈레톤 mismatch) DA 매핑값으로 덮는다. ApplySelectedCharacterAppearance가 BeginPlay에서
	// PlayIntroSequence·점프 입력보다 먼저 호출되므로 인트로/점프 전에 세팅 완료. 비어 있으면(null) BP_OJJ_Player
	// 기본값(Man) 유지 — DA 누락 시 기존 동작 보존(회귀 0).
	if (Appearance->GetUpMontage)
	{
		GetUpMontage = Appearance->GetUpMontage;
	}
	if (Appearance->JumpAnim)
	{
		JumpAnim = Appearance->JumpAnim;
	}
}

void AOJJ_Player::PlayIntroSequence()
{
	// [L_Planet 인트로] 누워있다 일어나는 getup 몽타주 재생 + 카메라 1인칭(ArmLength 0) 시작. 몽타주 종료 시
	// HandleMontageEnded가 bBlendingCamera를 켜고 Tick이 3인칭(IntroArmLength)으로 보간한다.
	APlayerController* PC = Cast<APlayerController>(GetController());
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;

	// 안전 스킵: 필수 요소(몽타주/SpringArm/AnimInstance) 중 하나라도 없으면 연출 생략 + 평소 플레이(크래시 방지).
	// 입력은 아직 잠그기 전이라 EnableInput은 멱등(혹시 모를 잔존 잠금 방어) + 플래그 소거로 다음 진입 재시도 방지.
	if (!GetUpMontage || !SpringArm || !AnimInstance)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[OJJ_Player] 인트로 연출 스킵 — GetUpMontage/SpringArm/AnimInstance 중 누락. 평소 플레이로 진행. ")
			TEXT("BP_OJJ_Player에 GetUpMontage 할당 및 ABP DefaultSlot 노드 확인."));
		if (PC)
		{
			EnableInput(PC);
		}
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UOJJ_CharacterSelectionSubsystem* Selection = GameInstance->GetSubsystem<UOJJ_CharacterSelectionSubsystem>())
			{
				Selection->SetShouldPlayIntro(false);
			}
		}
		return;
	}

	// 입력 잠금(연출 중 이동/카메라 차단) + 1인칭 시작. PC가 유효할 때만 잠그고 복원과 1:1 짝을 위해 플래그 기록.
	// PC가 null인데 DisableInput(null)을 부르면 모든 컨트롤러에 broadcast되어 EnableInput(validPC)와 어긋난다 →
	// 입력 잠금 자체를 PC 유효 시로 한정(이번 케이스에선 BeginPlay 시점 PC null 가능, 그때는 잠그지 않음).
	if (PC)
	{
		DisableInput(PC);
		bIntroInputDisabled = true;
	}
	SpringArm->TargetArmLength = 0.f;

	// getup 몽타주 재생. Montage_Play는 실패 시 0을 반환(에셋 미로드/메시 비가시/블렌드웨이트 0 등). 그대로 두면
	// 종료 델리게이트가 안 와 bBlendingCamera가 영영 false → 입력이 영구 잠김(soft-lock). 실패 시 3인칭 복귀 +
	// 입력 복구 + 플래그 소거로 평소 플레이에 안전 수렴(Codex 리뷰 2026-06-22).
	// 반환값(MontageLength)은 몽타주 길이(초, 기본 ReturnType=MontageLength) — 안전 타임아웃을 이 길이 기준으로 잡는다.
	const float MontageLength = AnimInstance->Montage_Play(GetUpMontage);
	if (MontageLength <= 0.f)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[OJJ_Player] 인트로 getup 몽타주 재생 실패 — 3인칭 복귀 + 평소 플레이. ABP DefaultSlot/몽타주 상태 확인."));
		SpringArm->TargetArmLength = IntroArmLength;
		TryRestoreIntroInput();
		return;
	}

	// 몽타주 재생 성공 → 카메라를 메시 HeadSocket에 부착해 진짜 머리 시점(1인칭)으로 본다. 소켓 미존재 시
	// 내부에서 부착 스킵 → 위에서 설정한 ArmLength 0 폴백이 그대로 유지된다(머리 위 내려보기 대신 SpringArm 원점).
	// 재생 실패 경로에서는 호출되지 않아 별도 복원이 불필요(부착 자체가 없음).
	AttachCameraToHeadSocket();

	// [soft-lock 최종 방어] 안전 타임아웃 = 몽타주 길이 + 여유(블렌드+버퍼). 옛 절대 6초는 몽타주(MT_WakeUp)보다
	// 짧아 재생 중에 터져 카메라/입력을 조기 복구하는 버그를 유발했다 → 몽타주 길이 기준으로 잡아 정상 재생·블렌드를
	// 절대 방해하지 않게 한다. 정상 완료(Tick 블렌드 종료) 시 ClearTimer로 취소된다(진짜 비정상일 때만 발동).
	const float SafetyDelay = MontageLength + IntroSafetyExtraSeconds;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			IntroSafetyTimerHandle, this, &AOJJ_Player::ForceFinishIntro, SafetyDelay, false);
	}
	UE_LOG(LogTemp, Log,
		TEXT("[OJJ_Player] 인트로 시작 — 몽타주 길이 %.1fs, 안전 타임아웃 %.1fs(=%.1f+%.1f) 가동. 입력잠금=%d"),
		MontageLength, SafetyDelay, MontageLength, IntroSafetyExtraSeconds, bIntroInputDisabled ? 1 : 0);

	// 그 몽타주 인스턴스에 종료 델리게이트를 건다(FOnMontageEnded는 비동적 델리게이트).
	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &AOJJ_Player::HandleMontageEnded);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, GetUpMontage);
}

bool AOJJ_Player::TryRestoreIntroInput()
{
	// [인트로] 입력 복구 + 1회성 플래그 소거의 단일 출처. DisableInput을 실제 적용했을 때만(bIntroInputDisabled)
	// EnableInput을 1:1로 호출 — 불균형/이중 호출 방지. PC가 아직 없으면 복구 보류 + false(호출부 다음 틱 재시도).
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (bIntroInputDisabled)
	{
		if (!PC)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[OJJ_Player] 인트로 입력 복구 보류 — 컨트롤러 아직 없음. 다음 틱/타임아웃에서 재시도."));
			return false;
		}
		EnableInput(PC);

		// 시네마틱(L_Cinematic) 경유 진입 시 위젯이 남긴 UI 입력모드/마우스 커서를 게임 전용으로 복원한다.
		// EnableInput만으로는 PlayerController의 InputMode(UI)가 안 풀려 마우스+이동이 먹통이던 잠김을 차단(TPS=게임 전용).
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;

		bIntroInputDisabled = false;
		UE_LOG(LogTemp, Log, TEXT("[OJJ_Player] 인트로 입력 복구 완료(EnableInput + InputModeGameOnly)."));
	}

	// 1회성 인트로 플래그 소거 — 입력 복구가 보장된(또는 애초에 잠그지 않은) 뒤에만.
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UOJJ_CharacterSelectionSubsystem* Selection = GameInstance->GetSubsystem<UOJJ_CharacterSelectionSubsystem>())
		{
			Selection->SetShouldPlayIntro(false);
		}
	}
	return true;
}

void AOJJ_Player::ForceFinishIntro()
{
	// [soft-lock 최종 방어선] 안전 타임아웃 만료. 아직 인트로 잔여 상태면 카메라/입력을 강제 복구한다.
	const bool bIntroPending = bBlendingCamera || bIntroInputDisabled || bCameraAttachedToHead;
	if (!bIntroPending)
	{
		return; // 이미 정상 종료됨 — no-op.
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[OJJ_Player] 인트로 안전 타임아웃 만료 — 강제 카메라/입력 복구(soft-lock 방지). ")
		TEXT("블렌드=%d 입력잠금=%d 머리부착=%d"),
		bBlendingCamera ? 1 : 0, bIntroInputDisabled ? 1 : 0, bCameraAttachedToHead ? 1 : 0);

	// 진짜 비정상(몽타주가 끝나지 않고 멈춤)으로 들어온 경우 — 재생 중인 getup 몽타주를 블렌드아웃으로 깔끔히 멈춰
	// ABP가 locomotion으로 자연 전이되게 한다(중단 없이 두면 선 채로 미끄러지는 상태 꼬임 방지). 이미 끝났으면 no-op.
	if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		if (GetUpMontage && AnimInstance->Montage_IsPlaying(GetUpMontage))
		{
			AnimInstance->Montage_Stop(0.2f, GetUpMontage);
		}
	}

	RestoreCameraFromHeadSocket();
	if (SpringArm)
	{
		SpringArm->TargetArmLength = IntroArmLength;
	}
	bBlendingCamera = false;

	// 입력 복구 시도. 만에 하나 아직 PC가 없어 실패하면(세션 비정상) 짧게 재무장해 끝까지 재시도한다(one-shot 누락 방지).
	if (!TryRestoreIntroInput())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				IntroSafetyTimerHandle, this, &AOJJ_Player::ForceFinishIntro, 0.5f, false);
		}
	}
}

void AOJJ_Player::AttachCameraToHeadSocket()
{
	// [인트로 1인칭] 카메라를 GetMesh()의 HeadSocket에 부착(머리 본을 따라가며 1인칭 시점).
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!Camera || !MeshComp || !MeshComp->DoesSocketExist(HeadSocketName))
	{
		// 폴백: 소켓/카메라/메시 누락 시 부착 스킵 — 호출부의 ArmLength 0 기존 방식 유지(크래시 방지).
		UE_LOG(LogTemp, Warning,
			TEXT("[OJJ_Player] 인트로 머리 시점 부착 스킵 — Camera/메시 누락 또는 소켓 '%s' 미존재. ArmLength 0 폴백."),
			*HeadSocketName.ToString());
		return;
	}

	// 원래 부착 상태(부모·소켓·상대 트랜스폼) 저장 — 블렌드 시작 시 그대로 복원한다.
	IntroCameraOriginalParent = Camera->GetAttachParent();
	IntroCameraOriginalSocket = Camera->GetAttachSocketName();
	IntroCameraOriginalRelativeTransform = Camera->GetRelativeTransform();

	Camera->AttachToComponent(MeshComp, FAttachmentTransformRules::SnapToTargetIncludingScale, HeadSocketName);
	bCameraAttachedToHead = true;
}

void AOJJ_Player::RestoreCameraFromHeadSocket()
{
	// [인트로 1인칭] 카메라를 HeadSocket에서 떼고 저장해둔 원래 부모/소켓/상대 트랜스폼으로 복원.
	if (!bCameraAttachedToHead || !Camera)
	{
		return;
	}

	// 원래 부모(보통 SpringArm)로 복귀. weak가 유실됐으면 SpringArm으로 폴백(생성자 기본 부착 상태).
	// 진단: 복원이 SpringArm->TargetArmLength를 건드리지 않음을 before/after로 확인(블렌드 미수렴 원인 배제용).
	const float ArmBefore = SpringArm ? SpringArm->TargetArmLength : -1.f;
	USceneComponent* OriginalParent = IntroCameraOriginalParent.Get();
	if (!OriginalParent)
	{
		OriginalParent = SpringArm;
	}
	if (OriginalParent)
	{
		Camera->AttachToComponent(OriginalParent, FAttachmentTransformRules::KeepRelativeTransform, IntroCameraOriginalSocket);
		Camera->SetRelativeTransform(IntroCameraOriginalRelativeTransform);
	}

	bCameraAttachedToHead = false;
	IntroCameraOriginalParent = nullptr;

	UE_LOG(LogTemp, Log,
		TEXT("[OJJ_Player] 카메라 HeadSocket 복원 — 부모=%s, ArmLength %.1f→%.1f(불변 기대)."),
		OriginalParent ? *OriginalParent->GetName() : TEXT("null"),
		ArmBefore, SpringArm ? SpringArm->TargetArmLength : -1.f);
}

void AOJJ_Player::HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// getup 몽타주 종료 → 카메라를 HeadSocket(머리 시점)에서 떼 원위치(SpringArm 소켓)로 복원한 뒤 3인칭 블렌드 시작.
	// 복원 시점의 ArmLength는 0이라 카메라가 SpringArm 원점에 와 있고, Tick이 IntroArmLength로 당겨 3인칭으로 수렴.
	// Montage_SetEndDelegate로 GetUpMontage 전용 바인드라 다른 몽타주로는 호출되지 않음(별도 필터 불필요).
	UE_LOG(LogTemp, Log,
		TEXT("[OJJ_Player] getup 몽타주 종료(bInterrupted=%d, ArmLength=%.1f) — 카메라 복원 후 3인칭 블렌드 시작."),
		bInterrupted ? 1 : 0, SpringArm ? SpringArm->TargetArmLength : -1.f);
	RestoreCameraFromHeadSocket();
	bBlendingCamera = true;
}

void AOJJ_Player::OJJ_DebugSetCharacter(int32 CharacterIndex)
{
	// [게임진입 테스트] 콘솔 디버그 — 서브시스템 값 설정 후 즉시 재스왑(BeginPlay 외 런타임 반영 검증).
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UOJJ_CharacterSelectionSubsystem* Selection = GameInstance->GetSubsystem<UOJJ_CharacterSelectionSubsystem>())
		{
			Selection->SetSelectedCharacter(
				CharacterIndex == 1 ? EOJJ_CharacterType::Woman : EOJJ_CharacterType::Man);
			ApplySelectedCharacterAppearance();
			UE_LOG(LogTemp, Log, TEXT("[OJJ_Player] DebugSetCharacter=%d 적용"), CharacterIndex);
		}
	}
}

void AOJJ_Player::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 폰 파괴/언포제스 시 열려 있던 머신 상호작용 위젯·입력모드 정리.
	// ⚠️ EndPlay 시점엔 PlayerController가 이미 무효일 수 있다 — Cast가 null을 반환하면
	//    CloseMachineInteractWidget의 PC null-가드가 입력모드 복원을 스킵하고 위젯 제거만 수행한다
	//    (컨트롤러가 없으면 복원할 대상도 없으므로 안전).
	CloseMachineInteractWidget(Cast<APlayerController>(GetController()));

	// 등반/step-off 중 폰 파괴·언포제스 시 비행/중력0 상태가 남지 않도록 청산(폰 재사용 안전).
	AbortClimb();

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFactorySaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UFactorySaveSubsystem>())
		{
			SaveSubsystem->SaveCurrentGame();
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AOJJ_Player::ConnectFactoryAgentClient()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFactoryAgentClientSubsystem* AgentClient = GameInstance->GetSubsystem<UFactoryAgentClientSubsystem>())
		{
			if (AgentClient->GetConnectionState() == EFactoryAgentConnectionState::Disconnected)
			{
				AgentClient->Connect("ws://127.0.0.1:18000/ws/agent");
			}
		}
	}
}

void AOJJ_Player::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[OJJ_Player] EnhancedInputComponent 캐스트 실패 — 프로젝트가 EnhancedInput을 사용하는지 확인"));
		return;
	}

	if (IA_Move)
	{
		EnhancedInput->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AOJJ_Player::Move);
	}
	if (IA_Sprint)
	{
		EnhancedInput->BindAction(IA_Sprint, ETriggerEvent::Started, this, &AOJJ_Player::StartSprint);
		EnhancedInput->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &AOJJ_Player::StopSprint);
		// Hold/Chord 트리거가 붙어 뗌이 Completed 대신 Canceled로 와도 질주가 안 남도록 함께 바인딩.
		EnhancedInput->BindAction(IA_Sprint, ETriggerEvent::Canceled, this, &AOJJ_Player::StopSprint);
	}
	if (IA_Look)
	{
		EnhancedInput->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AOJJ_Player::Look);
	}
	if (IA_Zoom)
	{
		EnhancedInput->BindAction(IA_Zoom, ETriggerEvent::Triggered, this, &AOJJ_Player::Zoom);
	}
	if (IA_Jump)
	{
		// ACharacter 내장 Jump/StopJumping 사용
		EnhancedInput->BindAction(IA_Jump, ETriggerEvent::Started, this, &AOJJ_Player::StartJumpAction);
		EnhancedInput->BindAction(IA_Jump, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}
	if (IA_Interact)
	{
		// 머신 상호작용 토글(F) — 누를 때 한 번(Started). 빌드모드 가드는 핸들러 내부에서.
		EnhancedInput->BindAction(IA_Interact, ETriggerEvent::Started, this, &AOJJ_Player::OnInteract);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[OJJ_Player] IA_Interact 미할당 — 머신 상호작용(F) 비활성. IMC_Player/BP_OJJ_Player에 IA_Interact 할당 필요."));
	}
	if (IA_Build)
	{
		EnhancedInput->BindAction(IA_Build, ETriggerEvent::Started, this, &AOJJ_Player::ToggleBuild);
	}
	else
	{
		// 바인딩이 조용히 스킵되면 B키가 무반응이라 원인 파악이 어려움 → 명시적 경고
		UE_LOG(LogTemp, Warning,
			TEXT("[OJJ_Player] IA_Build 미할당 — 빌드모드 토글(B키) 비활성. BP_OJJ_Player에 IA_Build 에셋 할당 필요."));
	}
	// [TPS 빌드] V키 = 3인칭 빌드 토글. IA_Build와 동일하게 항상 활성인 IMC_Player에 매핑(빌드모드 밖에서도 진입 가능).
	if (IA_BuildTPS)
	{
		EnhancedInput->BindAction(IA_BuildTPS, ETriggerEvent::Started, this, &AOJJ_Player::ToggleBuildTPS);
	}
	else
	{
		// IA_BuildTPS .uasset(V 매핑)은 에디터에서 생성 후 BP_OJJ_Player에 할당해야 함 → 미할당 시 V 무반응
		UE_LOG(LogTemp, Warning,
			TEXT("[OJJ_Player] IA_BuildTPS 미할당 — TPS 빌드 토글(V키) 비활성. BP_OJJ_Player에 IA_BuildTPS 에셋 할당 필요."));
	}
	if (IA_BuildPlace)
	{
		// 누름(Started)=배치/드래그시작, 뗌(Completed)=드래그커밋, 취소(Canceled)=드래그취소.
		// 머신 모드에선 Completed/Canceled가 no-op(BuildController 내부 모드 가드).
		EnhancedInput->BindAction(IA_BuildPlace, ETriggerEvent::Started, this, &AOJJ_Player::BuildPlace);
		EnhancedInput->BindAction(IA_BuildPlace, ETriggerEvent::Completed, this, &AOJJ_Player::BuildPlaceReleased);
		EnhancedInput->BindAction(IA_BuildPlace, ETriggerEvent::Canceled, this, &AOJJ_Player::BuildPlaceCanceled);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[OJJ_Player] IA_BuildPlace 미할당 — 배치(좌클릭) 비활성. BP_OJJ_Player에 IA_BuildPlace 에셋 할당 필요."));
	}
	// [옛 빌드 입력 경로 전수 정리] Machine/Conveyor/Pipe/Tank/PowerNode/Shield/PowerLine/PowerPlant/
	// Grinder/Miner/Pump/Smelter/Warehouse 직행 IA BindAction은 카테고리 숫자키 슬롯이 완전 대체하여 제거.
	// 철거(Demolish)는 X 직행키로 유지.
	if (IA_SetDemolishMode)
	{
		EnhancedInput->BindAction(IA_SetDemolishMode, ETriggerEvent::Started, this, &AOJJ_Player::SetDemolishMode);
	}
	if (IA_BuildPan)
	{
		// 매 프레임 입력(연속 이동) → Triggered
		EnhancedInput->BindAction(IA_BuildPan, ETriggerEvent::Triggered, this, &AOJJ_Player::BuildPan);
	}
	if (IA_BuildRotate)
	{
		EnhancedInput->BindAction(IA_BuildRotate, ETriggerEvent::Triggered, this, &AOJJ_Player::BuildRotate);
	}
	if (IA_MachineRotate)
	{
		// 머신 회전은 1회성 토글이라 Started (누를 때 한 번)
		EnhancedInput->BindAction(IA_MachineRotate, ETriggerEvent::Started, this, &AOJJ_Player::BuildRotateMachine);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[OJJ_Player] IA_MachineRotate 미할당 — 호버 머신 회전(R) 비활성. IMC_Build/BP_OJJ_Player에 IA_MachineRotate 할당 필요."));
	}
	// [공중 Foundation] 높이 층 증감(Q/E). 한 번 누를 때 1층 — 이산 스텝이라 Started(머신 회전과 동일 패턴).
	if (IA_BuildHeight)
	{
		EnhancedInput->BindAction(IA_BuildHeight, ETriggerEvent::Started, this, &AOJJ_Player::BuildAdjustHeight);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[OJJ_Player] IA_BuildHeight 미할당 — TPS Foundation 높이 조절(Q/E) 비활성. IMC_BuildTPS/BP_OJJ_Player에 IA_BuildHeight 할당 필요."));
	}

	PlayerInputComponent->BindKey(EKeys::M, IE_Pressed, this, &AOJJ_Player::SendOperatorGuideRequest);
	PlayerInputComponent->BindKey(EKeys::K, IE_Pressed, this, &AOJJ_Player::TriggerHUDQuestRequest);
	PlayerInputComponent->BindKey(EKeys::J, IE_Pressed, this, &AOJJ_Player::TriggerHUDQuestWindowToggle);
	PlayerInputComponent->BindKey(EKeys::Slash, IE_Pressed, this, &AOJJ_Player::TriggerHUDAIGuideToggle);
	PlayerInputComponent->BindKey(EKeys::I, IE_Pressed, this, &AOJJ_Player::TriggerInventoryToggle);
	PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AOJJ_Player::TriggerTutorialDialogueReveal);
	// [옛 빌드 입력 경로 전수 정리] O(성형)/P(합성)/T(통신) 직행 BindKey는 카테고리 숫자키 슬롯이 완전 대체하여 제거.
	// 콘솔 SetBuildMode tower(통신탑)는 계속 동작.
	PlayerInputComponent->BindKey(EKeys::X, IE_Pressed, this, &AOJJ_Player::SetDemolishModeShortcut);
	// [공용키] 카테고리 무관 — 빌드모드 중 항상 동작. 전부 레거시 BindKey + 핸들러 IsInBuildMode 가드.
	// 옛 IA_SetFoundationMode(G)/IA_SetRampMode(H) IMC 매핑·IA 에셋은 폐기됨(F/G로 환원).
	// ⚠️ F는 IA_Interact(머신 상호작용)와 공유 — 빌드모드 중=Foundation, 밖=Interact. 상호배타는
	// SetFoundationModeShortcut(IsInBuildMode 요구)와 OnInteract(IsInBuildMode면 early-return)의 역가드에 의존.
	PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &AOJJ_Player::SetFoundationModeShortcut);     // 평면 플랫폼
	PlayerInputComponent->BindKey(EKeys::G, IE_Pressed, this, &AOJJ_Player::SetRampFoundationModeShortcut); // 경사면
	PlayerInputComponent->BindKey(EKeys::H, IE_Pressed, this, &AOJJ_Player::SetLadderModeShortcut);         // 사다리(기존 C에서 이동)
	PlayerInputComponent->BindKey(EKeys::Z, IE_Pressed, this, &AOJJ_Player::CancelPlacementShortcut);       // 마우스 초기화(취소)

	// [카테고리 숫자키] 1~9,0 → 현재 카테고리(LDJ UI_BuildModeMain)의 N번 슬롯 실행. 0키=10번 슬롯.
	// 참고: 옛 직행 IA BindAction(IA_SetMachineMode 등)은 C++에서 폐기됨 → 에디터에 남은 IMC_Build 매핑은
	//      바인딩이 없어 무동작(이중 발화 없음). 에디터 잔여 매핑 청소는 UI 담당 후속(정책상 .uasset 미변경).
	PlayerInputComponent->BindKey(EKeys::One,   IE_Pressed, this, &AOJJ_Player::SetHotbarSlot1);
	PlayerInputComponent->BindKey(EKeys::Two,   IE_Pressed, this, &AOJJ_Player::SetHotbarSlot2);
	PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AOJJ_Player::SetHotbarSlot3);
	PlayerInputComponent->BindKey(EKeys::Four,  IE_Pressed, this, &AOJJ_Player::SetHotbarSlot4);
	PlayerInputComponent->BindKey(EKeys::Five,  IE_Pressed, this, &AOJJ_Player::SetHotbarSlot5);
	PlayerInputComponent->BindKey(EKeys::Six,   IE_Pressed, this, &AOJJ_Player::SetHotbarSlot6);
	PlayerInputComponent->BindKey(EKeys::Seven, IE_Pressed, this, &AOJJ_Player::SetHotbarSlot7);
	PlayerInputComponent->BindKey(EKeys::Eight, IE_Pressed, this, &AOJJ_Player::SetHotbarSlot8);
	PlayerInputComponent->BindKey(EKeys::Nine,  IE_Pressed, this, &AOJJ_Player::SetHotbarSlot9);
	PlayerInputComponent->BindKey(EKeys::Zero,  IE_Pressed, this, &AOJJ_Player::SetHotbarSlot10); // 0키 = 10번 슬롯

	// [카테고리 순환] ←/→ 방향키 = 빌드모드(TPS+TopDown) 설치 카테고리 순환(기계↔전력↔건물). 슬롯키와 동일한
	// 레거시 BindKey 패턴(IA 에셋 불필요). 빌드모드 가드는 핸들러(CycleBuildCategory)에서 처리 — None(빌드 밖) 무동작.
	PlayerInputComponent->BindKey(EKeys::Right, IE_Pressed, this, &AOJJ_Player::CycleCategoryNext);
	PlayerInputComponent->BindKey(EKeys::Left,  IE_Pressed, this, &AOJJ_Player::CycleCategoryPrev);

	// [빌드 작업등] L = 빌드모드 작업등 토글. 핸들러(ToggleWorkLight)가 IsInBuildMode 가드 — 빌드 밖 무동작.
	// TPS=NightSpotLight 재활용, TopDown=BuildCamera 하향광. 실제 점등은 UpdateNightSpotLightVisibility가 매 틱 분배.
	PlayerInputComponent->BindKey(EKeys::L, IE_Pressed, this, &AOJJ_Player::ToggleWorkLight);
}

void AOJJ_Player::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (!Controller || Axis.IsNearlyZero())
	{
		return;
	}

	// 탑다운 빌드모드에선 캐릭터 이동 잠금(팀 의도 — 탑다운은 캐릭터 조작 없음).
	// TPS 빌드모드/None은 통과 → 캐릭터 이동 유지. (IsInBuildMode는 TPS도 true라 TPS 먹통 회귀 → TopDown 한정)
	if (BuildController && BuildController->GetBuildViewMode() == EBuildViewMode::TopDown)
	{
		return;
	}

	// step-off 안착 보간 중엔 이동 입력 잠금(보간이 위치를 전담 → 진동/끼임 방지).
	if (bSteppingOff)
	{
		return;
	}

	// 등반 중(#184): 전후축(W/S)을 수직 이동으로 재해석, 좌우(D/A)는 무시(사다리 축 고정).
	// 상/하단 도달 시 등반 종료 — 상단은 step-off, 하단은 지면 복귀.
	if (CurrentLadder)
	{
		AddMovementInput(FVector::UpVector, Axis.Y);

		// 발 밑 Z로 상/하단 도달 판정. ClimbReachMargin 여유로 경계 떨림 방지(도달은 살짝 일찍).
		const float FeetZ = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

		// [#184/#343 옵션A] 긴 사다리만 마지막 FinishTriggerDistance 구간에서 Finish(올라서기 오버레이) 1회 재생.
		// 짧은 사다리(ClimbHeight < FinishTriggerDistance)는 Finish 아예 스킵 — Loop만 + step-off 안착으로 종료
		// (BeginClimb 직후 즉시트리거/허공 올라서기 방지). 옛 min(dist, ClimbHeight*0.5) 클램프는 "어쨌든 재생"
		// 방식이라 폐기. Finish hips는 평탄화(Loop 높이 고정)라 캡슐 비행이 수직 전담 → 포즈 가산 상승 0(이중튐 없음).
		// 올라서기 = 평탄 hips 위 팔/다리 모션 오버레이. ⚠️ 몽타주 Root Motion OFF 유지(이동은 비행 전담).
		// bFinishPlaying = 한 등반당 1회 가드(BeginClimb/AbortClimb에서 리셋).
		if (Axis.Y > 0.f && !bFinishPlaying && LadderFinishMontage
			&& CurrentLadder->GetClimbHeight() >= FinishTriggerDistance)
		{
			const float RemainingToTop = CurrentLadder->GetClimbTopZ() - FeetZ;
			if (RemainingToTop <= FinishTriggerDistance)
			{
				PlayAnimMontage(LadderFinishMontage);
				bFinishPlaying = true;
			}
		}

		if (Axis.Y > 0.f && FeetZ >= CurrentLadder->GetClimbTopZ() - ClimbReachMargin)
		{
			EndClimb(/*bStepOffTop=*/true);
		}
		else if (Axis.Y < 0.f && FeetZ <= CurrentLadder->GetClimbBottomZ() + ClimbReachMargin)
		{
			EndClimb(/*bStepOffTop=*/false);
		}
		return;
	}

	// [#184] 등반 시작 — 진입-오버랩 의존 제거: 사다리 겹침 중 W(위) 누르면 시작(이미 트리거 안이어도).
	// BeginClimb 내부 가드(쿨다운·밑동 위치 FeetZ>Bottom+80)가 상면 재등반/직후 재진입을 막는다.
	if (Axis.Y > 0.f && OverlappingLadder.IsValid())
	{
		BeginClimb(OverlappingLadder.Get());
		if (CurrentLadder)
		{
			return; // 시작 성공 → 이번 프레임 걷기 입력 스킵(다음 프레임부터 등반 분기).
		}
	}

	// 카메라(컨트롤러) yaw 기준 전/후·좌/우 방향 산출 — pitch/roll은 이동에서 제외
	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, Axis.Y); // W/S
	AddMovementInput(Right, Axis.X);   // D/A

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
		{
			QuestManager->NotifyMainQuestInputAction(TEXT("Move"));
			QuestManager->NotifyTutorialEvent(TEXT("InputAction"), TEXT("Move"));
		}
	}
}

void AOJJ_Player::BeginClimb(AOJJ_Ladder* Ladder)
{
	if (!Ladder || CurrentLadder || bSteppingOff)
	{
		return; // 이미 등반 중·step-off 보간 중·잘못된 사다리 — 무시(단일 진실원).
	}

	// 재진입 쿨다운: step-off 직후 같은 트리거에 다시 잡혀 MOVE_Flying로 복귀하는 무한 토글(진동) 차단.
	const UWorld* World = GetWorld();
	if (World && World->GetTimeSeconds() < ClimbCooldownUntil)
	{
		return;
	}

	// 위치 기반 진입 가드(핵심): 발이 사다리 '밑동 근처'(하단 + 여유 이내)일 때만 등반 시작.
	// 상면에서 걸어다니는 캐릭터(발 Z ≈ 상단)가 전체높이 트리거에 닿아 등반으로 오인되는 것을 차단.
	const float FeetZ = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	if (FeetZ > Ladder->GetClimbBottomZ() + ClimbEntryZTolerance)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[Climb] 진입 거부 — 상면 높이(FeetZ=%.1f > Bottom=%.1f+%.1f)"),
			FeetZ, Ladder->GetClimbBottomZ(), ClimbEntryZTolerance);
		return;
	}

	CurrentLadder = Ladder;
	bClimbing = true;
	bFinishPlaying = false; // [#184] 새 등반 시작 — Finish 마무리 몽타주 재트리거 허용
	UE_LOG(LogTemp, Verbose, TEXT("[Climb] BeginClimb Bottom=%.1f Top=%.1f"),
		Ladder->GetClimbBottomZ(), Ladder->GetClimbTopZ());

	// [#357] 점프 직후 사다리 진입은 Landed를 거치지 않고 아래에서 MOVE_Flying로 전환되므로, 점프 슬롯 애니가
	// 등반 포즈를 덮을 수 있다 — 진입 시 명시 정지(codex P2).
	StopJumpMontage();

	// 사다리 마주보게 1회 정렬: 캐릭터는 사다리 바깥쪽에 서서 안쪽(GetStepOffDirection=+X, 벽/Foundation)을
	// 바라봐야 한다. yaw만(pitch/roll 0). 메시/애니 방향 보정은 LadderFacingYawOffset(PIE 튜닝)로 더한다.
	const float FaceYaw = Ladder->GetStepOffDirection().Rotation().Yaw + LadderFacingYawOffset;
	SetActorRotation(FRotator(0.f, FaceYaw, 0.f));

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		// 중력 끄고 비행 모드로 수직 이동. 키를 떼면 빠르게 멈춰 사다리에서 호버하도록 제동 강하게.
		Movement->SetMovementMode(MOVE_Flying);
		Movement->GravityScale = 0.f;
		Movement->MaxFlySpeed = ClimbSpeed;
		Movement->BrakingDecelerationFlying = 2048.f;
		Movement->StopMovementImmediately();
		// 등반 중엔 수직 이동만이라 OrientRotationToMovement가 yaw를 못 잡는다(XY 0). 위 사다리-facing이
		// 흔들리지 않게 끄고, EndClimb/AbortClimb에서 걷기용으로 복원한다.
		Movement->bOrientRotationToMovement = false;
	}
}

void AOJJ_Player::EndClimb(bool bStepOffTop)
{
	if (!CurrentLadder)
	{
		return;
	}

	AOJJ_Ladder* Ladder = CurrentLadder;
	CurrentLadder = nullptr;
	bClimbing = false;
	// [#184] Finish 마무리 몽타주는 top 도착 '이전'에 Move() 거리트리거(FinishTriggerDistance)로 이미 재생됨
	// — 여기서 재생하면 늦으므로(올라선 뒤 또 올라서기) 두지 않는다. bFinishPlaying은 다음 BeginClimb에서 리셋.
	// [#343 codex교차검증 #5] Finish 몽타주(4s)가 마운트(~0.58s)보다 길어, 정지하지 않으면 step-off/초기 걷기에
	// 팔·다리 올라서기 모션이 덧씌워진다(평탄화로 수직 튐은 없으나 포즈 잔상). top 도달 = 마운트 완료이므로
	// 여기서 블렌드아웃. 인자 명시 → Finish가 활성일 때만 정지(다른 몽타주 영향 없음). 하단 종료 시엔 미재생이라 no-op.
	if (LadderFinishMontage)
	{
		StopAnimMontage(LadderFinishMontage);
	}

	// 상단 도달: Foundation 상면으로 '부드럽게' 보간 안착(StepOffDuration). 즉시 텔레포트는 순간이동 느낌이라
	// 짧은 lerp로 부드럽게 + 보간 중 입력 잠금(진동 방지). 완료 시 Walking 복귀 + 쿨다운(Tick에서).
	if (bStepOffTop && Ladder && StepOffDuration > KINDA_SMALL_NUMBER)
	{
		const FVector LadderLoc = Ladder->GetActorLocation();
		const FVector Dir = Ladder->GetStepOffDirection();
		const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const float StepDist = GetCapsuleComponent()->GetScaledCapsuleRadius() + StepOffForward; // [#184] 캡슐반경 + 여유(기본 0)
		StepOffTarget = FVector(
			LadderLoc.X + Dir.X * StepDist,
			LadderLoc.Y + Dir.Y * StepDist,
			Ladder->GetClimbTopZ() + HalfHeight + StepOffZMargin);
		StepOffStart = GetActorLocation();
		StepOffElapsed = 0.f;
		bSteppingOff = true;

		// 보간 동안 낙하 방지(비행/중력0 유지). 위치는 Tick의 lerp가 전담.
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->SetMovementMode(MOVE_Flying);
			Movement->GravityScale = 0.f;
			Movement->StopMovementImmediately();
		}
		return; // Walking 복귀/쿨다운은 보간 완료 시(Tick)에서.
	}

	// 상단(보간 끔) 즉시 안착 또는 하단/기타 종료: 바로 걷기 복귀.
	if (bStepOffTop && Ladder)
	{
		const FVector LadderLoc = Ladder->GetActorLocation();
		const FVector Dir = Ladder->GetStepOffDirection();
		const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const float StepDist = GetCapsuleComponent()->GetScaledCapsuleRadius() + StepOffForward; // [#184] 캡슐반경 + 여유(기본 0)
		SetActorLocation(FVector(
			LadderLoc.X + Dir.X * StepDist,
			LadderLoc.Y + Dir.Y * StepDist,
			Ladder->GetClimbTopZ() + HalfHeight + StepOffZMargin),
			/*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
	}
	ResumeWalkingWithCooldown();
}

void AOJJ_Player::ResumeWalkingWithCooldown()
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->GravityScale = 1.f;
		Movement->StopMovementImmediately();
		Movement->SetMovementMode(MOVE_Walking);
		Movement->bOrientRotationToMovement = true; // 등반 중 끈 것 복원(걷기 방향 회전 정상화)
	}

	// 재진입 쿨다운 개시 — step-off로 상면에 올라간 직후 같은 트리거에 다시 잡히는 진동 차단.
	if (const UWorld* World = GetWorld())
	{
		ClimbCooldownUntil = World->GetTimeSeconds() + ClimbReentryCooldown;
	}
}

void AOJJ_Player::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateNightSpotLightVisibility();

	// [L_Planet 인트로] getup 몽타주 종료 후 카메라를 1인칭(ArmLength 0)→3인칭(IntroArmLength)으로 부드럽게 블렌드.
	// 아래 step-off early-return보다 먼저 처리해야 평상시에도 보간이 돈다(등반/step-off와 독립).
	if (bBlendingCamera && SpringArm)
	{
		const float NewArm = FMath::FInterpTo(SpringArm->TargetArmLength, IntroArmLength, DeltaSeconds, IntroBlendSpeed);
		SpringArm->TargetArmLength = NewArm;
		if (FMath::Abs(NewArm - IntroArmLength) <= 1.f)
		{
			// 목표 근접 → 길이 확정. 단, 블렌드 종료/플래그 소거는 '입력 복구 성공'에 묶는다 — PC가 일시 null인
			// 프레임이면 TryRestoreIntroInput이 false를 반환하고 bBlendingCamera를 유지해 다음 틱에 재시도한다
			// (이전엔 EnableInput만 if(PC) 안에서 스킵되고 플래그는 소거돼 입력이 영구 잠기던 soft-lock을 차단).
			SpringArm->TargetArmLength = IntroArmLength;
			if (TryRestoreIntroInput())
			{
				bBlendingCamera = false;
				if (UWorld* World = GetWorld())
				{
					World->GetTimerManager().ClearTimer(IntroSafetyTimerHandle);
				}
				UE_LOG(LogTemp, Log, TEXT("[OJJ_Player] 인트로 카메라 블렌드 완료 — 3인칭 복귀 + 입력 복구."));
			}
		}
	}

	// 안전망: 등반 중 표시인데 사다리가 사라졌으면(파괴/GC로 CurrentLadder=null) 걷기 복귀 — 비행/중력0 고착 방지.
	// 사다리 파괴/invalid(pending-kill 포함) 시에도 강제 청산 — AbortClimb이 Flying 해제 + GravityScale 복원 +
	// bOrientRotationToMovement=true(BeginClimb에서 끈 것) 복원을 보장한다. TObjectPtr는 weak 아니라 stale 가능 →
	// 단순 null 체크론 부족(IsValid). 누락 시 등반 중 끈 회전이 영구 고착되어 걸어도 안 돌게 됨.
	if (bClimbing && !IsValid(CurrentLadder))
	{
		AbortClimb();
	}

	// [#184] 등반 중 X/Y를 사다리 등반 면으로 '부드럽게' 당김(즉시 SetActorLocation은 멀리서 시작 시 순간이동
	// → VInterpTo). Z는 등반(비행 수직)이 전담하므로 현재 Z 유지. 가까이서 W로 시작하면 거의 즉시 붙음.
	if (bClimbing && IsValid(CurrentLadder))
	{
		const FVector Cur = GetActorLocation();
		const FVector Face = OJJ_GetClimbFaceLocation(CurrentLadder, Cur.Z);
		const FVector NewLoc = FMath::VInterpTo(Cur, FVector(Face.X, Face.Y, Cur.Z), DeltaSeconds, ClimbAttachInterpSpeed);
		SetActorLocation(NewLoc,
			/*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
	}

	if (!bSteppingOff)
	{
		return; // 평상시 무비용.
	}

	StepOffElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(StepOffElapsed / FMath::Max(StepOffDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);
	const float Smooth = FMath::SmoothStep(0.f, 1.f, Alpha); // ease in-out으로 자연스러운 안착
	SetActorLocation(FMath::Lerp(StepOffStart, StepOffTarget, Smooth),
		/*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);

	if (Alpha >= 1.f)
	{
		bSteppingOff = false;
		ResumeWalkingWithCooldown();
	}
}

void AOJJ_Player::UpdateNightSpotLightVisibility()
{
	// ⚠️ 매 틱 호출 — 모든 조명 상태(밤 점등 + 빌드 작업등)의 단일 진입점. 외부에서 SetVisibility 하면
	//    다음 틱에 여기서 덮어쓰므로, 작업등 토글(bWorkLightOn)도 반드시 이 함수에서 분배해야 한다.

	// 1) 밤 시간대 판정(기존 로직): 18시~6시.
	bool bNight = false;
	if (const UWorld* World = GetWorld())
	{
		if (const UPlanetEventManagerSubsystem* EventManager = World->GetSubsystem<UPlanetEventManagerSubsystem>())
		{
			const int32 CurrentHour24 = EventManager->GetCurrentHour24();
			bNight = CurrentHour24 >= NightLightStartHour24 || CurrentHour24 < NightLightEndHour24;
		}
	}

	// 2) 빌드 작업등 상태 — 모드별 분배.
	const EBuildViewMode ViewMode = BuildController ? BuildController->GetBuildViewMode() : EBuildViewMode::None;
	const bool bWorkLightActive = bWorkLightOn && ViewMode != EBuildViewMode::None;
	const bool bTPSWorkLight = bWorkLightActive && ViewMode == EBuildViewMode::TPS;
	const bool bTopDownWorkLight = bWorkLightActive && ViewMode == EBuildViewMode::TopDown;

	// 3) NightSpotLight(플레이어 정면): 밤이거나 TPS 작업등이면 점등. TPS 작업등일 땐 작업등답게 상향,
	//    그 외(밤 일반)엔 기본값으로 원복. (탑다운에선 플레이어가 숨겨져 어차피 렌더 안 됨 → BuildCamera 담당)
	if (NightSpotLight)
	{
		const bool bSpotOn = bNight || bTPSWorkLight;
		if (NightSpotLight->IsVisible() != bSpotOn)
		{
			NightSpotLight->SetVisibility(bSpotOn);
		}

		const float TargetIntensity = bTPSWorkLight ? TPSWorkLightIntensity : BaseNightLightIntensity;
		const float TargetRadius = bTPSWorkLight ? TPSWorkLightRadius : BaseNightLightRadius;
		if (!FMath::IsNearlyEqual(NightSpotLight->Intensity, TargetIntensity))
		{
			NightSpotLight->SetIntensity(TargetIntensity);
		}
		if (!FMath::IsNearlyEqual(NightSpotLight->AttenuationRadius, TargetRadius))
		{
			NightSpotLight->SetAttenuationRadius(TargetRadius);
		}
	}

	// 4) BuildCamera 하향광(탑다운 전용): 탑다운 작업등일 때만 점등.
	if (BuildCamera)
	{
		BuildCamera->SetWorkLightEnabled(bTopDownWorkLight);
	}
}

void AOJJ_Player::ToggleWorkLight()
{
	// 빌드모드(TPS/TopDown)에서만 토글. 빌드 밖이면 무동작(키 오발동 차단). 실제 점등 분배는 매 틱 갱신.
	if (!BuildController || !BuildController->IsInBuildMode())
	{
		return;
	}
	bWorkLightOn = !bWorkLightOn;
}

void AOJJ_Player::AbortClimb()
{
	// 등반/step-off를 즉시 청산하고 걷기로 수렴(중력 복원). 빌드모드 진입·EndPlay·사다리 소멸 등
	// 비정상 종료 경로에서 MOVE_Flying/GravityScale=0 고착을 방지하는 단일 안전 청산점.
	if (!bClimbing && !bSteppingOff && !CurrentLadder)
	{
		return;
	}
	CurrentLadder = nullptr;
	bClimbing = false;
	bSteppingOff = false;
	bFinishPlaying = false; // [#184] 비정상 청산 — 다음 등반서 Finish 재트리거 허용
	ResumeWalkingWithCooldown();
}

FVector AOJJ_Player::OJJ_GetClimbFaceLocation(const AOJJ_Ladder* Ladder, float WorldZ) const
{
	// 등반 면 = Foundation 반대(바깥). 액터 전방(+X)=inward이므로 바깥 = -GetStepOffDirection.
	// 거리 = 캡슐 반경 + ClimbFaceGap(소량). 사다리가 벽면 라인에 있으니 캡슐이 면에 살짝 닿는 위치로 붙임.
	const FVector LadderLoc = Ladder->GetActorLocation();
	const FVector Outward = -Ladder->GetStepOffDirection();
	const float Dist = GetCapsuleComponent()->GetScaledCapsuleRadius() + ClimbFaceGap;
	return FVector(LadderLoc.X + Outward.X * Dist, LadderLoc.Y + Outward.Y * Dist, WorldZ);
}

void AOJJ_Player::NotifyLadderOverlap(AOJJ_Ladder* Ladder)
{
	// 근접 사다리 갱신만(시작은 W 입력에서). 등반 시작을 트리거 진입 순간에 묶지 않아, 이미 트리거 안이어도
	// W로 시작 가능 + 멀리서 강제 스냅(순간이동) 없음.
	if (Ladder)
	{
		OverlappingLadder = Ladder;
	}
}

void AOJJ_Player::NotifyLadderEndOverlap(AOJJ_Ladder* Ladder)
{
	// 이탈한 사다리가 현재 후보면 해제(다른 사다리 포인터는 안 건드림). 등반 중(CurrentLadder)은 별개라 영향 없음.
	if (OverlappingLadder.Get() == Ladder)
	{
		OverlappingLadder = nullptr;
	}
}

void AOJJ_Player::Look(const FInputActionValue& Value)
{
	// 탑다운 빌드모드에선 마우스룩 잠금(팀 의도). TPS 빌드모드/None은 통과 → 마우스 카메라 유지.
	// (IsInBuildMode는 TPS도 true라 TPS 진입 시 마우스룩 먹통 회귀 → TopDown 한정)
	if (BuildController && BuildController->GetBuildViewMode() == EBuildViewMode::TopDown)
	{
		return;
	}

	const FVector2D Axis = Value.Get<FVector2D>();
	// 마우스 raw 델타가 그대로 회전량이 되지 않도록 감도 배율을 곱해 완화
	AddControllerYawInput(Axis.X * LookYawSensitivity);
	const float PitchSign = bInvertLookPitch ? -1.0f : 1.0f;
	AddControllerPitchInput(Axis.Y * LookPitchSensitivity * PitchSign);
}

void AOJJ_Player::Zoom(const FInputActionValue& Value)
{
	const float Scroll = Value.Get<float>();
	if (FMath::IsNearlyZero(Scroll))
	{
		return;
	}

	// 탑다운 빌드만 뷰타겟이 BuildCamera라 그쪽을 줌(플레이어 SpringArm은 안 보임).
	// ⚠️ TPS 빌드는 뷰타겟이 플레이어 SpringArm이므로 가드를 통과시켜 아래 일반 SpringArm 줌(150~800)을 탄다.
	if (BuildController && BuildController->GetBuildViewMode() == EBuildViewMode::TopDown)
	{
		if (BuildCamera)
		{
			BuildCamera->Zoom(Scroll);
		}
		return;
	}

	if (!SpringArm)
	{
		return;
	}

	// TPS: 스크롤 업(+) → 줌인(팔 길이 감소)
	const float NewLength = SpringArm->TargetArmLength - Scroll * ZoomStep;
	SpringArm->TargetArmLength = FMath::Clamp(NewLength, MinArmLength, MaxArmLength);
}

void AOJJ_Player::StartJumpAction(const FInputActionValue& Value)
{
	// 등반/step-off 중 점프 무시(상태 상호배제).
	if (CurrentLadder || bSteppingOff)
	{
		return;
	}

	// [#357 P2-①] 점프가 실제 수락될 때만 슬롯 애니를 재생하려면 CanJump()를 Jump() '전에' 판정한다(Jump()이
	// bPressedJump 등 상태를 바꾸기 전, 깨끗한 지상 상태 기준). 공중 재입력(이미 점프 중/낙하 중 추가 점프 불가)은
	// Jump()이 no-op이라 — 이 가드가 없으면 가짜 공중 점프 애니가 나간다(codex P2-①).
	const bool bJumpAccepted = CanJump();

	Jump();

	// [#357] 점프 수락 순간 도약 애니를 DefaultSlot로 즉시 재생 — ABP 스테이트머신의 IsFalling 진입은 발이
	// 땅에서 떨어진 뒤라 한 박자 늦다(서서 뜬 뒤 점프 포즈). 슬롯 동적 몽타주가 스테이트머신 출력을 덮어 입력
	// 순간 점프 포즈가 나간다(LadderFinish 슬롯 패턴 공용, 스테이트머신 무수정). 단일 시퀀스 한계 보정:
	// ① 준비동작(무릎 구부림)은 JumpAnimStartPosition으로 건너뛰어 2단 점프 느낌 제거 ② 착지 잔상은 Landed에서
	// ActiveJumpMontage를 끊어 제거. 미수락/미할당/AnimInstance 없으면 안전 스킵(기존 동작).
	if (bJumpAccepted && JumpAnim)
	{
		if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
		{
			// 인자: BlendIn/Out + PlayRate 1 / LoopCount 1 / BlendOutTrigger -1(끝에서 자동) / StartPosition(준비동작 스킵).
			// [P2-②] 반환 핸들을 직접 캐시(UE5.7은 UAnimMontage* 반환) → Landed에서 이 몽타주만 정지. 재생 실패 시
			// nullptr이라 Landed 정지 스킵(GetCurrentActiveMontage는 다른 몽타주를 잡을 수 있어 미사용).
			ActiveJumpMontage = AnimInstance->PlaySlotAnimationAsDynamicMontage(
				JumpAnim, TEXT("DefaultSlot"), JumpAnimBlendInTime, JumpAnimBlendOutTime,
				1.0f, 1, -1.0f, JumpAnimStartPosition);
		}
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
		{
			QuestManager->NotifyMainQuestInputAction(TEXT("Jump"));
			QuestManager->NotifyTutorialEvent(TEXT("InputAction"), TEXT("Jump"));
		}
	}
}

void AOJJ_Player::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	// [#357] 착지 즉시 점프 슬롯 애니를 끊어 locomotion 복귀(통짜 시퀀스가 착지까지 나가 서서 미끄러지는 잔상 제거).
	StopJumpMontage();
}

void AOJJ_Player::StopJumpMontage()
{
	// [#357] 점프 슬롯 애니(ActiveJumpMontage)가 재생 중이면 정지 — 착지(Landed)뿐 아니라 사다리 진입 등 Landed를
	// 거치지 않는 상태 전환에서도 점프 포즈가 다음 동작(등반)을 덮지 않게 한다(codex P2). 인자 명시 → 점프 슬롯만
	// 정지(다른 몽타주 영향 0), 몽타주 BlendOut으로 부드럽게. 이미 끝났거나 미재생이면 no-op(LadderFinish 선례).
	if (ActiveJumpMontage)
	{
		StopAnimMontage(ActiveJumpMontage);
		ActiveJumpMontage = nullptr;
	}
}

bool AOJJ_Player::ShouldPlayFallAnim() const
{
	// [#368] ABP 점프/falling 상태 진입 게이트 — raw IsFalling은 낮은 턱 내려갈 때도 잠깐 true라 점프 포즈가
	// 뜬다. 하강 중(IsFalling)이면서 하강 속도가 임계 초과(Velocity.Z < -임계)일 때만 진짜 낙하로 본다.
	// ⚠️ Velocity.Z 부호: 하강 = 음수. 낮은 턱(짧은 낙하)은 착지 전 속도가 작아 false → 진입 안 함.
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	return Move && Move->IsFalling() && Move->Velocity.Z < -FallAnimVelocityThreshold;
}

void AOJJ_Player::ToggleBuild(const FInputActionValue& Value)
{
	// B키 = 탑다운 빌드. 토글 규칙은 HandleBuildModeKey가 담당(None→TopDown / TopDown→None / TPS→TopDown).
	HandleBuildModeKey(EBuildViewMode::TopDown);
}

void AOJJ_Player::ToggleBuildTPS(const FInputActionValue& Value)
{
	// V키 = TPS 빌드. 토글 규칙: None→TPS / TPS→None / TopDown→TPS.
	HandleBuildModeKey(EBuildViewMode::TPS);
}

void AOJJ_Player::HandleBuildModeKey(EBuildViewMode TargetMode)
{
	if (!BuildController)
	{
		return;
	}

	// 등반/step-off 중 빌드모드 진입 시 MOVE_Flying/중력0이 잔존하지 않도록 먼저 청산(걷기 복귀).
	AbortClimb();

	// 토글 규칙: 이미 대상 모드면 해제(None), 아니면 대상 모드로 진입/전환.
	const EBuildViewMode PrevMode = BuildController->GetBuildViewMode();
	const EBuildViewMode Desired = (PrevMode == TargetMode) ? EBuildViewMode::None : TargetMode;

	BuildController->SetBuildViewMode(Desired); // 단일 진실원 — 진입/해제/전환을 내부 처리
	const EBuildViewMode NewMode = BuildController->GetBuildViewMode();

	// 모드가 실제로 바뀐 경우에만 뷰 적용(진입/해제/TopDown↔TPS 전환 모두 포함).
	// SetBuildViewMode가 검증 실패로 미전환(NewMode==PrevMode)이면 카메라/IMC도 안 건드림(half-state 방지).
	if (NewMode != PrevMode)
	{
		ApplyBuildModeView(NewMode);
	}

	// 퀘스트/튜토리얼 알림은 None→빌드 최초 진입에서만(모드 전환 시 중복 발사 방지).
	if (PrevMode == EBuildViewMode::None && NewMode != EBuildViewMode::None)
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
			{
				QuestManager->NotifyMainQuestBuildModeEntered();
				QuestManager->NotifyTutorialEvent(TEXT("BuildMode"));
			}
		}
	}
}

void AOJJ_Player::ApplyBuildModeView(EBuildViewMode NewMode)
{
    // 카메라 뷰타겟 블렌드 + 플레이어 가시성 + 입력 컨텍스트를 모드별로 전환.
    // None=해제, TopDown=빌드캠+숨김+IMC_Build, TPS=플레이어캠+보임+IMC_BuildTPS.
    APlayerController* PC = Cast<APlayerController>(GetController());

    if (NewMode != EBuildViewMode::None)
    {
       // ── 진입 또는 모드 전환 공통: 열려 있던 기계창/창고/가방 UI 정리 ──
    	if (MachineInteractWidgetInstance.IsValid() || WarehouseInteractWidgetInstance || SynthesizerInteractWidgetInstance)
    	{
    		CloseMachineInteractWidget(PC);
    	}
       if (bIsInventoryOpen && InventoryWidgetInstance)
       {
          InventoryWidgetInstance->RemoveFromParent();
          bIsInventoryOpen = false;
          GetWorldTimerManager().ClearTimer(InventoryRefreshTimerHandle);

          if (UGameInstance* GameInstance = GetGameInstance())
          {
             if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
             {
                QuestManager->NotifyTutorialEvent(TEXT("InventoryClose"));
             }
          }
       }

       // ── 카메라 + 가시성: 모드별 ──
       if (NewMode == EBuildViewMode::TopDown)
       {
          // 빌드캠 XY = 플레이어 현재 위치, Z = 그리드 평면. 그 후 빌드캠으로 블렌드 + 캐릭터 숨김.
          if (BuildCamera && BuildController)
          {
             if (const AOJJ_Grid* Grid = BuildController->GetTargetGrid())
             {
                const FVector PlayerLoc = GetActorLocation();
                const FVector AnchorXY = Grid->IsValidGridCell(Grid->WorldToGrid(PlayerLoc))
                   ? PlayerLoc : Grid->GetGridCenter();
                const FVector CamLoc(AnchorXY.X, AnchorXY.Y, Grid->GetGridCenter().Z);
                BuildCamera->SetActorLocation(CamLoc);
             }
          }
          if (PC && BuildCamera)
          {
             PC->SetViewTargetWithBlend(BuildCamera, CameraBlendTime);
          }
          // 탑다운에서 플레이어가 안 보이도록 숨김
          SetActorHiddenInGame(true);

          // 마우스: 커서 보임 + GameAndUI(커서로 셀 조준/클릭). EnterBuildMode가 켠 커서를 모드별로 확정.
          // (IMC_Build엔 IA_Look 없음 → 마우스는 카메라 회전 안 하고 커서로만 동작.)
          if (PC)
          {
             FInputModeGameAndUI TopDownInputMode;
             TopDownInputMode.SetHideCursorDuringCapture(false);
             PC->SetInputMode(TopDownInputMode);
             PC->bShowMouseCursor = true;
          }
       }
       else // EBuildViewMode::TPS
       {
          // ⚠️ 3인칭 유지: 뷰타겟 = 플레이어(소유 Pawn). TopDown→TPS 전환 시 빌드캠에서 플레이어로 복귀.
          //    None→TPS면 이미 플레이어 뷰라 사실상 no-op(블렌드만). 캐릭터는 계속 보임.
          if (PC)
          {
             PC->SetViewTargetWithBlend(this, CameraBlendTime);
          }
          SetActorHiddenInGame(false);

          // [빌드 작업등] TopDown→TPS 전환 시 BuildCamera 하향광 즉시 끔(월드 스폿이라 1프레임 잔광이 TPS 뷰에
          // 보이는 것 방지). TPS 작업등은 NightSpotLight가 담당 — 다음 틱 UpdateNightSpotLightVisibility가 분배.
          if (BuildCamera)
          {
             BuildCamera->SetWorkLightEnabled(false);
          }

          // ⚠️ 마우스: 커서 숨김 + GameOnly(마우스 = 3인칭 카메라 Look). 화면중앙 조준이라 OS 커서 불필요.
          //    EnterBuildMode가 켠 커서를 여기서 끔. (크로스헤어 UI는 후속 — 일단 화면중앙 감각.)
          if (PC)
          {
             PC->SetInputMode(FInputModeGameOnly());
             PC->bShowMouseCursor = false;
          }
       }

       // 안전장치: 질주 중 진입 시 속도 고착 방지
       if (UCharacterMovementComponent* Movement = GetCharacterMovement())
       {
          Movement->MaxWalkSpeed = WalkSpeed;
       }

       // 입력 컨텍스트(모드별 + 미할당 함정 가드)
       ApplyBuildInputContext(NewMode);

       // HUD 숨김 + 빌드 위젯 표시(두 빌드 모드 공통)
       if (MainHUDWidgetInstance)
       {
          MainHUDWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
       }
       if (PC && BuildModeWidgetClass && !BuildModeWidgetInstance)
       {
          BuildModeWidgetInstance = CreateWidget<UUserWidget>(PC, BuildModeWidgetClass);
          if (BuildModeWidgetInstance)
          {
             BuildModeWidgetInstance->AddToViewport();
          }
       }
       if (BuildModeWidgetInstance)
       {
          BuildModeWidgetInstance->SetVisibility(ESlateVisibility::Visible);
       }
    }
    else
    {
       // ── 빌드모드 해제(None) ──
       // [빌드 작업등] 빌드 나가면 작업등 끔(리셋). 라이트 실소등은 다음 틱 UpdateNightSpotLightVisibility가 처리하나,
       // BuildCamera 하향광은 즉시 꺼 1프레임 잔광을 막는다.
       bWorkLightOn = false;
       if (BuildCamera)
       {
          BuildCamera->SetWorkLightEnabled(false);
       }

       if (PC)
       {
          // 복귀 뷰타겟은 플레이어 자신(소유 Pawn)
          PC->SetViewTargetWithBlend(this, CameraBlendTime);
       }
       SetActorHiddenInGame(false);

       // 마우스: 일반 플레이 복귀 — 커서 숨김 + GameOnly. (TopDown/TPS에서 바꾼 커서·입력모드 원복)
       if (PC)
       {
          PC->SetInputMode(FInputModeGameOnly());
          PC->bShowMouseCursor = false;
       }

       // 입력 복귀: 빌드 IMC 제거 + IMC_Player 복원
       ApplyBuildInputContext(EBuildViewMode::None);

       if (BuildModeWidgetInstance)
       {
          BuildModeWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
       }
       if (MainHUDWidgetInstance)
       {
          // 드래그 씹힘 방지
          MainHUDWidgetInstance->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
       }
    }
}

void AOJJ_Player::ApplyBuildInputContext(EBuildViewMode Mode)
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    UEnhancedInputLocalPlayerSubsystem* Subsystem = PC
       ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer())
       : nullptr;
    if (!Subsystem)
    {
       return;
    }

    // [codex M1] None(빌드 종료) 케이스 분리. 빌드 IMC(Build/BuildTPS)는 IMC_Player 유무와 무관하게 무조건 제거 —
    // 종료 후 빌드 입력이 잔류해 이동/시점/상호작용이 먹통되는 회귀 방지(기존 Exit 동작 보존). IMC_Player는 있으면 복원
    // (중복 방지 위해 제거 후 재추가). 미할당이면 복원만 스킵(빌드 IMC 제거는 그대로 — 최소한 빌드 컨트롤은 안 남김).
    if (Mode == EBuildViewMode::None)
    {
       if (IMC_Build)    { Subsystem->RemoveMappingContext(IMC_Build); }
       if (IMC_BuildTPS) { Subsystem->RemoveMappingContext(IMC_BuildTPS); }
       if (IMC_Player)
       {
          Subsystem->RemoveMappingContext(IMC_Player);
          Subsystem->AddMappingContext(IMC_Player, 0);
       }
       else
       {
          UE_LOG(LogTemp, Warning,
             TEXT("[OJJ_Player] IMC_Player 미할당 — 빌드 종료 시 복원할 일반 입력 컨텍스트 없음. BP_OJJ_Player에 할당 필요."));
       }
       return;
    }

    // 진입/전환(TopDown/TPS): 목표 빌드 IMC.
    UInputMappingContext* Desired = (Mode == EBuildViewMode::TopDown) ? IMC_Build : IMC_BuildTPS;

    // ⚠️ 락아웃 가드(진입/전환 한정): 목표 빌드 IMC가 미할당이면 현재 컨텍스트를 제거하지 않는다(입력 먹통 = 못 빠져나옴).
    //    Exit(None)은 위에서 별도 처리되므로 이 가드가 종료 경로의 빌드 IMC 제거를 막지 않는다.
    if (!Desired)
    {
       UE_LOG(LogTemp, Warning,
          TEXT("[OJJ_Player] %s 미할당 — 입력 컨텍스트 전환 중단(기존 IMC 유지). BP_OJJ_Player에 할당 필요."),
          Mode == EBuildViewMode::TopDown ? TEXT("IMC_Build") : TEXT("IMC_BuildTPS"));
       return;
    }

    // 후보 컨텍스트 전부 제거(추가 안 돼 있으면 no-op) → 목표만 추가. 모드 전환 시 누락/중복 원천 차단.
    if (IMC_Player)   { Subsystem->RemoveMappingContext(IMC_Player); }
    if (IMC_Build)    { Subsystem->RemoveMappingContext(IMC_Build); }
    if (IMC_BuildTPS) { Subsystem->RemoveMappingContext(IMC_BuildTPS); }
    Subsystem->AddMappingContext(Desired, 0);
}

void AOJJ_Player::BuildPlace(const FInputActionValue& Value)
{
	if (!BuildController)
	{
		return;
	}
	// 빌드모드 밖이면 OnLeftClickPressed 내부 가드(bIsBuildMode)로 no-op
	BuildController->OnLeftClickPressed();
}

void AOJJ_Player::BuildPlaceReleased(const FInputActionValue& Value)
{
	if (!BuildController)
	{
		return;
	}
	// 좌클릭 뗌 — 컨베이어 모드면 드래그 커밋, 머신 모드면 내부 가드로 no-op.
	BuildController->OnLeftClickReleased();
}

void AOJJ_Player::BuildPlaceCanceled(const FInputActionValue& Value)
{
	if (!BuildController)
	{
		return;
	}
	// 좌클릭 입력 취소 — 진행 중 컨베이어 드래그 취소(머신 모드는 드래그 없어 no-op).
	BuildController->CancelConveyorDrag();
	BuildController->CancelPowerLineDrag();
}

void AOJJ_Player::SetBuildMode(const FString& ModeName)
{
	// [임시 진입로] 콘솔 exec — IA/UI 미와이어링 모드(pipe/tank/tower) 전용. 빌드모드 여부는 검사하지
	// 않음(빌드모드 밖 호출 = 다음 진입 모드 예약). SetPlacementMode 직접 위임이라 직행키/슬롯 경로와 독립.
	if (!BuildController)
	{
		return;
	}
	const FString Lower = ModeName.ToLower();
	if (Lower == TEXT("pipe"))
	{
		BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Pipe);
	}
	else if (Lower == TEXT("tank") || Lower == TEXT("liquidtank"))
	{
		BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::LiquidTank);
	}
	else if (Lower == TEXT("tower") || Lower == TEXT("telecommunicationtower"))
	{
		BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::TeleCommunicationTower);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[OJJ_Player] SetBuildMode: unknown mode '%s' (pipe|tank|tower)"), *ModeName);
	}
}

void AOJJ_Player::SetDemolishModeShortcut()
{
	if (!BuildController || !BuildController->IsInBuildMode())
	{
		return;
	}

	BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Demolish);
}

// [#184] C키 — 사다리 빌드 서브모드 진입. 레거시 BindKey라 IMC 게이팅이 없으므로 빌드모드 가드 필수(Demolish 패턴).
void AOJJ_Player::SetLadderModeShortcut()
{
	if (!BuildController || !BuildController->IsInBuildMode())
	{
		return;
	}

	BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Ladder);

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
		{
			QuestManager->NotifyTutorialEvent(TEXT("SelectLadderMode"));
		}
	}
}

// [공용키 F] 평면 Foundation 직행. 레거시 BindKey라 IMC 게이팅 없음 → 빌드모드 가드 필수(Ladder/Demolish 패턴).
void AOJJ_Player::SetFoundationModeShortcut()
{
	if (!BuildController || !BuildController->IsInBuildMode())
	{
		return;
	}
	BuildController->OJJ_SelectFoundationKind(false);

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
		{
			QuestManager->NotifyTutorialEvent(TEXT("SelectFlatFoundationMode"));
		}
	}
}

// [공용키 G] 경사 RampFoundation 직행.
void AOJJ_Player::SetRampFoundationModeShortcut()
{
	if (!BuildController || !BuildController->IsInBuildMode())
	{
		return;
	}
	BuildController->OJJ_SelectFoundationKind(true);

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
		{
			QuestManager->NotifyTutorialEvent(TEXT("SelectRampFoundationMode"));
		}
	}
}

// [공용키 Z] 마우스 초기화 — 들고 있던 placement 고스트 취소(None 모드), 빌드모드/배치된 액터는 그대로.
void AOJJ_Player::CancelPlacementShortcut()
{
	if (!BuildController || !BuildController->IsInBuildMode())
	{
		return;
	}
	// [TPS 2클릭] 경로형 앵커(전선 등)가 잡혀 있으면 앵커만 무르고 모드 유지(2클릭 재시도). 없으면 기존 동작(모드 None 초기화).
	if (BuildController->CancelPendingConnectAnchor())
	{
		return;
	}
	BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::None);
}

// [카테고리 숫자키] 현재 카테고리의 SlotIndex(1~10)번 슬롯 실행 — LDJ UI_BuildModeMain에 위임(슬롯 클릭과 동일 경로).
// 카테고리 상태(CurrentSubMode)는 위젯이 보유 → 슬롯 번호만 넘기면 위젯이 현재 카테고리 기준 해석.
void AOJJ_Player::ExecuteHotbarSlot(int32 SlotIndex)
{
	if (!BuildController || !BuildController->IsInBuildMode())
	{
		return;
	}
	// BuildModeWidgetClass = WBP_BuildModeMain(UI_BuildModeMain) 전제(에디터 확인됨). 안전망: 아니면 무동작+경고.
	if (UUI_BuildModeMain* BuildMenu = Cast<UUI_BuildModeMain>(BuildModeWidgetInstance))
	{
		BuildMenu->ExecutePlacementMode(SlotIndex);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[OJJ_Player] 핫바 슬롯 %d 무시 — BuildModeWidgetInstance가 UUI_BuildModeMain 아님/없음(BuildModeWidgetClass 확인)"),
			SlotIndex);
	}
}

void AOJJ_Player::SetHotbarSlot1()  { ExecuteHotbarSlot(1); }
void AOJJ_Player::SetHotbarSlot2()  { ExecuteHotbarSlot(2); }
void AOJJ_Player::SetHotbarSlot3()  { ExecuteHotbarSlot(3); }
void AOJJ_Player::SetHotbarSlot4()  { ExecuteHotbarSlot(4); }
void AOJJ_Player::SetHotbarSlot5()  { ExecuteHotbarSlot(5); }
void AOJJ_Player::SetHotbarSlot6()  { ExecuteHotbarSlot(6); }
void AOJJ_Player::SetHotbarSlot7()  { ExecuteHotbarSlot(7); }
void AOJJ_Player::SetHotbarSlot8()  { ExecuteHotbarSlot(8); }
void AOJJ_Player::SetHotbarSlot9()  { ExecuteHotbarSlot(9); }
void AOJJ_Player::SetHotbarSlot10() { ExecuteHotbarSlot(10); }

// [카테고리 순환] ←/→ → 현재 카테고리(LDJ UI_BuildModeMain)를 Dir 방향으로 1칸 순환 위임.
// ExecuteHotbarSlot과 동일 위임 구조이나, 가드는 IsInBuildMode()가 아니라 GetBuildViewMode()==None 차단으로
// — 빌드모드(TPS+TopDown) 공용. None(빌드 밖)에서만 ←/→ 카테고리 오발동을 차단한다.
void AOJJ_Player::CycleBuildCategory(int32 Dir)
{
	if (!BuildController || BuildController->GetBuildViewMode() == EBuildViewMode::None)
	{
		return;
	}
	// BuildModeWidgetClass = WBP_BuildModeMain(UI_BuildModeMain) 전제. 안전망: 아니면 무동작+경고.
	if (UUI_BuildModeMain* BuildMenu = Cast<UUI_BuildModeMain>(BuildModeWidgetInstance))
	{
		BuildMenu->CycleSubMode(Dir);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[OJJ_Player] 카테고리 순환 무시 — BuildModeWidgetInstance가 UUI_BuildModeMain 아님/없음(BuildModeWidgetClass 확인)"));
	}
}

void AOJJ_Player::CycleCategoryNext() { CycleBuildCategory(+1); }
void AOJJ_Player::CycleCategoryPrev() { CycleBuildCategory(-1); }

// 철거 모드 진입(X키). 호버 대상 빨강 하이라이트 + 좌클릭 제거.
void AOJJ_Player::SetDemolishMode(const FInputActionValue& Value)
{
	if (!BuildController)
	{
		return;
	}
	BuildController->SetPlacementMode(EOJJ_BuildPlacementMode::Demolish);

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
		{
			QuestManager->NotifyTutorialEvent(TEXT("DemolishMode"));
		}
	}
}

void AOJJ_Player::StartSprint(const FInputActionValue& Value)
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = SprintSpeed;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
		{
			QuestManager->NotifyMainQuestInputAction(TEXT("Sprint"));
			QuestManager->NotifyTutorialEvent(TEXT("InputAction"), TEXT("Sprint"));
		}
	}
}

void AOJJ_Player::StopSprint(const FInputActionValue& Value)
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = WalkSpeed;
	}
}

void AOJJ_Player::BuildPan(const FInputActionValue& Value)
{
	// IA_BuildPan은 IMC_Build에만 매핑되므로 빌드모드에서만 호출됨. Pan 내부에서 0입력/DeltaSeconds 처리.
	if (!BuildController || !BuildController->IsInBuildMode())
	{
		return;
	}

	if (BuildCamera)
	{
		BuildCamera->Pan(Value.Get<FVector2D>());
	}
}

void AOJJ_Player::BuildRotate(const FInputActionValue& Value)
{
	if (!BuildController || !BuildController->IsInBuildMode())
	{
		return;
	}

	const float RotateInput = Value.Get<float>();

	if (BuildCamera)
	{
		BuildCamera->Rotate(RotateInput);
	}

	if (FMath::IsNearlyZero(RotateInput))
	{
		return;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
		{
			QuestManager->NotifyTutorialEvent(
				RotateInput > 0.0f ? TEXT("RotateViewRight") : TEXT("RotateViewLeft"));
		}
	}
}

void AOJJ_Player::BuildAdjustHeight(const FInputActionValue& Value)
{
	// [공중 Foundation] Q/E 축 부호 → ±1층. 대상/모드 가드(TPS·Foundation·Flat)는 BuildController::AdjustBuildHeight가 담당.
	if (!BuildController)
	{
		return;
	}
	const float Axis = Value.Get<float>();
	if (FMath::IsNearlyZero(Axis))
	{
		return;
	}
	BuildController->AdjustBuildHeight(Axis > 0.0f ? 1 : -1);
}

void AOJJ_Player::BuildRotateMachine(const FInputActionValue& Value)
{
	// 호버 머신 회전은 BuildController가 상태(step) 소유 — 빌드모드 가드도 거기서 처리.
	if (BuildController)
	{
		BuildController->RotateHoverClockwise();
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
		{
			QuestManager->NotifyTutorialEvent(TEXT("RotatePlacement"));
		}
	}
}

void AOJJ_Player::SendOperatorGuideRequest()
{
	UGameInstance* GameInstance = GetGameInstance();
	UFactoryAgentClientSubsystem* AgentClient = GameInstance
		? GameInstance->GetSubsystem<UFactoryAgentClientSubsystem>()
		: nullptr;
	if (!AgentClient)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OJJ_Player] FactoryAgentClientSubsystem not found."));
		return;
	}

	if (!AgentClient->IsConnected())
	{
		if (AgentClient->GetConnectionState() == EFactoryAgentConnectionState::Disconnected)
		{
			AgentClient->ConnectToDefaultServer();
		}
		UE_LOG(LogTemp, Warning, TEXT("[OJJ_Player] Factory agent WebSocket is not connected yet."));
		return;
	}

	const FString Question = TEXT("\uAE30\uC5B4 \uB9CC\uB4E4\uB824\uBA74 \uBB50\uAC00 \uD544\uC694\uD574?");
	if (AgentClient->SendOperatorGuideQuestion(Question, TEXT("unreal-client")))
	{
		UE_LOG(LogTemp, Log, TEXT("[OJJ_Player] Sent operator guide request."));
	}
}

void AOJJ_Player::OnInteract(const FInputActionValue& Value)
{
    if (!IsLocallyControlled()) return;

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;

    if (BuildController && BuildController->IsInBuildMode()) return;
    
    FInputModeGameAndUI QuickFixMode;
    QuickFixMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    QuickFixMode.SetWidgetToFocus(nullptr); 
    PC->SetInputMode(QuickFixMode);

    // 이미 켜져 있을 때 끄고 탈출하는 가드 조건에 합성기 위젯 인스턴스도 병합합니다.
    if (bIsInventoryOpen || 
        (MachineInteractWidgetInstance.IsValid() && MachineInteractWidgetInstance->IsInViewport()) ||
        (WarehouseInteractWidgetInstance && WarehouseInteractWidgetInstance->IsInViewport()) ||
        (SynthesizerInteractWidgetInstance && SynthesizerInteractWidgetInstance->IsInViewport()))
    {
       if (MachineInteractWidgetInstance.IsValid())
       {
          MachineInteractWidgetInstance->RemoveFromParent();
          MachineInteractWidgetInstance = nullptr;
       }

       if (WarehouseInteractWidgetInstance)
       {
          WarehouseInteractWidgetInstance->RemoveFromParent();
          WarehouseInteractWidgetInstance = nullptr;
       }

       // 합성기 끄기
       if (SynthesizerInteractWidgetInstance)
       {
          SynthesizerInteractWidgetInstance->RemoveFromParent();
          SynthesizerInteractWidgetInstance = nullptr;
       }

       if (InventoryWidgetInstance)
       {
          InventoryWidgetInstance->RemoveFromParent();
       }
       bIsInventoryOpen = false;
       GetWorldTimerManager().ClearTimer(InventoryRefreshTimerHandle);

       PC->SetInputMode(FInputModeGameOnly());
       PC->SetShowMouseCursor(false);
       return;
    }

    UWorld* World = GetWorld();
    if (!Camera || !World) return;

    const FVector TraceStart = Camera->GetComponentLocation();
    const FVector TraceEnd = TraceStart + Camera->GetForwardVector() * MaxInteractDistance;
    FHitResult Hit;
    FCollisionQueryParams TraceParams(FName(TEXT("OJJMachineInteract")), false, this);
    const bool bHit = World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, TraceParams);
    if (!bHit) return;

    AMachineBase* Machine = Cast<AMachineBase>(Hit.GetActor());
    if (!Machine) return;

    // 1. 창고 포트 및 액체 탱크 레이아웃 개방 분기
    if (Machine->IsA(AWarehousePort::StaticClass()) || Machine->IsA(ALiquidTank::StaticClass()) || Machine->GetName().Contains(TEXT("Warehouse")))
    {
       if (!WarehouseInteractWidgetClass)
       {
          UE_LOG(LogTemp, Warning, TEXT("[OJJ_Player] WarehouseInteractWidgetClass 미할당! BP에서 할당하세요."));
          return;
       }

       if (UUI_MainHUD* MainHUD = Cast<UUI_MainHUD>(MainHUDWidgetInstance))
       {
          MainHUD->CloseQuestWindow();
       }

       UUI_WarehouseInteract* WHWidget = CreateWidget<UUI_WarehouseInteract>(PC, WarehouseInteractWidgetClass);
       if (WHWidget)
       {
          WHWidget->SetTargetMachine(Machine);
          WarehouseInteractWidgetInstance = WHWidget;
          WHWidget->AddToViewport();
          WHWidget->OnClosed.AddDynamic(this, &AOJJ_Player::RestoreGameInputMode);
       }
    }
    // 합성기 전용
    else if (Machine->GetMachineType() == TEXT("Synthesizer"))
    {
       if (!SynthesizerInteractWidgetClass)
       {
          UE_LOG(LogTemp, Warning, TEXT("[OJJ_Player] SynthesizerInteractWidgetClass 미할당! BP에서 할당하세요."));
          return;
       }

       UUI_SynthesizerInteract* SynWidget = CreateWidget<UUI_SynthesizerInteract>(PC, SynthesizerInteractWidgetClass);
       if (SynWidget)
       {
          SynWidget->SetTargetMachine(Machine);
          SynthesizerInteractWidgetInstance = SynWidget;
          SynWidget->AddToViewport();
          SynWidget->OnClosed.AddDynamic(this, &AOJJ_Player::RestoreGameInputMode);
       }
    }
    // 3. 일반 기계 분기 (제련기, 분쇄기 등)
    else
    {
       if (!MachineInteractWidgetClass) return;

       UUI_MachineInteract* Widget = CreateWidget<UUI_MachineInteract>(PC, MachineInteractWidgetClass);
       if (Widget)
       {
          Widget->SetTargetMachine(Machine);
          MachineInteractWidgetInstance = Widget;
          Widget->AddToViewport();
          Widget->OnClosed.AddDynamic(this, &AOJJ_Player::RestoreGameInputMode);
       }
    }

    // 창고 포트뿐만 아니라 '액체 탱크' 계열 상호작용 시에도 우측에 인벤토리 생성
    if (Machine->IsA(AWarehousePort::StaticClass()) || Machine->IsA(ALiquidTank::StaticClass()) || Machine->GetName().Contains(TEXT("Warehouse")))
    {
       if (!InventoryWidgetInstance && InventoryWidgetClass)
       {
          InventoryWidgetInstance = CreateWidget<UUI_Inventory>(PC, InventoryWidgetClass);
       }

        if (InventoryWidgetInstance)
        {
           InventoryWidgetInstance->AdjustInventoryLayout(true); 
           InventoryWidgetInstance->AddToViewport(-1); 
           InventoryWidgetInstance->RefreshInventoryWindow();
           bIsInventoryOpen = true;

           GetWorldTimerManager().SetTimer(
             InventoryRefreshTimerHandle, 
             this, 
             &AOJJ_Player::UpdateInventoryRealtime, 
             0.1f, 
             true
          );
            
           UE_LOG(LogTemp, Log, TEXT("[물류 상호작용 성공] 창고형 기계 연동 모드로 가방 창 동시 활성화 완료!"));
        }
    }

    FInputModeGameAndUI InputModeData;
    InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PC->SetInputMode(InputModeData);
    PC->SetShowMouseCursor(true);
}

void AOJJ_Player::CloseMachineInteractWidget(APlayerController* PC)
{
	// 1. 기존 일반 기계창 끄기
	if (UUI_MachineInteract* Widget = MachineInteractWidgetInstance.Get())
	{
		Widget->RemoveFromParent();
	}
	MachineInteractWidgetInstance = nullptr;

	// 2. 창고 전용 UI 창 끄기
	if (WarehouseInteractWidgetInstance)
	{
		WarehouseInteractWidgetInstance->RemoveFromParent();
		WarehouseInteractWidgetInstance = nullptr;
	}

	// 3. 합성기 UI 창 끄기
	if (SynthesizerInteractWidgetInstance)
	{
		SynthesizerInteractWidgetInstance->RemoveFromParent();
		SynthesizerInteractWidgetInstance = nullptr;
	}

	if (PC)
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
}

void AOJJ_Player::RestoreGameInputMode()
{
	if ((MachineInteractWidgetInstance.IsValid() && MachineInteractWidgetInstance->IsInViewport()) ||
		(SynthesizerInteractWidgetInstance && SynthesizerInteractWidgetInstance->IsInViewport()))
	{
		return;
	}
	
	// 스테일 브로드캐스트 가드 — 우리가 위젯 A를 닫고 새 위젯 B를 연 사이에 A의 지연된
	// NativeDestruct가 broadcast될 수 있다. 현재 살아있는 위젯이 열려 있으면 그 상태를 건드리지 않는다
	// (B의 weak 포인터/GameAndUI를 망가뜨리지 않도록).
	if (MachineInteractWidgetInstance.IsValid() && MachineInteractWidgetInstance->IsInViewport())
	{
		return;
	}

	// 멱등 — 우리 직접 닫기(CloseMachineInteractWidget)로 이미 복원된 뒤 지연 Destruct 브로드캐스트가
	// 한 번 더 들어와도 사실상 no-op. weak 인스턴스 정리.
	MachineInteractWidgetInstance = nullptr;
	SynthesizerInteractWidgetInstance = nullptr;

	// ⚠️ pawn 소멸 중 위젯 Destruct로 재진입할 수 있어 컨트롤러 유효성 체크(무효면 복원 스킵 —
	//    복원 대상 자체가 없으므로 안전).
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
}
void AOJJ_Player::TriggerHUDQuestRequest()
{
	// 빌드모드 중에는 화면에 메인 HUD가 꺼지므로 슬래시 키 작동을 막습니다.
	if (BuildController && BuildController->IsInBuildMode())
	{
		return;
	}
	if (UUI_MainHUD* MainHUD = Cast<UUI_MainHUD>(MainHUDWidgetInstance))
	{
		// 퀘스트 요청 함수 원격 실행
		MainHUD->OnRequestQuestsClicked();
	}
}
void AOJJ_Player::TriggerHUDQuestWindowToggle()
{
	// 빌드모드 내부에 있는 퀘스트 창을 찾아 토글합니다.
	if (BuildController && BuildController->IsInBuildMode())
	{
		if (BuildModeWidgetInstance)
		{
			UUI_QuestWindow* BuildQuestWindow = Cast<UUI_QuestWindow>(BuildModeWidgetInstance->GetWidgetFromName(TEXT("WBP_QuestWindow")));
			if (BuildQuestWindow)
			{
				BuildQuestWindow->ToggleQuestWindow();
			}
		}
		return; // 빌드모드 처리가 끝났으므로 아래 일반 HUD 로직으로 내려가지 못하게 차단
	}

	// 기존 메인 HUD 내부의 퀘스트 창 토글
	if (UUI_MainHUD* MainHUD = Cast<UUI_MainHUD>(MainHUDWidgetInstance))
	{
		MainHUD->ToggleQuestWindow();
	}
}

void AOJJ_Player::TriggerHUDAIGuideToggle()
{
	if (BuildController && BuildController->IsInBuildMode()) return;

	UWorld* World = GetWorld();
	if (!World) return;

	TArray<UUserWidget*> FoundWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, FoundWidgets, UUI_DialogueBalloon::StaticClass(), false);
	if (FoundWidgets.IsEmpty()) return;

	UUI_DialogueBalloon* DialogueBalloon = Cast<UUI_DialogueBalloon>(FoundWidgets[0]);
	APlayerController* PC = Cast<APlayerController>(GetController());
	
	if (DialogueBalloon && PC)
	{
		DialogueBalloon->ToggleAIGuide(PC);
	}
}

void AOJJ_Player::TriggerTutorialDialogueReveal()
{
	// UI_MainHUD 대신 월드에서 UI_DialogueBalloon을 직접 찾아 가이드창 오픈 여부를 검사합니다.
	UWorld* World = GetWorld();
	if (World)
	{
		TArray<UUserWidget*> FoundWidgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, FoundWidgets, UUI_DialogueBalloon::StaticClass(), false);
		if (!FoundWidgets.IsEmpty())
		{
			if (UUI_DialogueBalloon* DialogueBalloon = Cast<UUI_DialogueBalloon>(FoundWidgets[0]))
			{
				// 대화창 내의 AI 입력 칸이 켜져 있다면(QnA 진행 중이라면) 튜토리얼 자동 넘기기를 가드합니다.
				if (DialogueBalloon->ET_OperatorInput && DialogueBalloon->ET_OperatorInput->GetVisibility() != ESlateVisibility::Collapsed)
				{
					return;
				}
			}
		}
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
		{
			if (QuestManager->HasPendingTutorialStartDialogue())
			{
				QuestManager->RevealPendingTutorialStartDialogue();
			}
			else if (!QuestManager->AdvanceTutorialManualStep())
			{
				QuestManager->DismissTutorialCompletionDialogue();
			}
		}
	}
}

void AOJJ_Player::TriggerInventoryToggle()
{
    if (BuildController && BuildController->IsInBuildMode()) return;

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;

	// 1. 이미 열려 있다면 닫기
	if (bIsInventoryOpen)
	{
		if (InventoryWidgetInstance)
		{
			InventoryWidgetInstance->RemoveFromParent();
			bIsInventoryOpen = false;

			if (UGameInstance* GameInstance = GetGameInstance())
			{
				if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
				{
					QuestManager->NotifyTutorialEvent(TEXT("InventoryClose"));
				}
			}
		}
		
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);

       GetWorldTimerManager().ClearTimer(InventoryRefreshTimerHandle);
       return;
    }
	
    UWorld* World = GetWorld();
    if (!Camera || !World) return;

    FVector TraceStart = Camera->GetComponentLocation();
    FVector TraceEnd = TraceStart + Camera->GetForwardVector() * MaxInteractDistance;
    FHitResult Hit;
    FCollisionQueryParams TraceParams(FName(TEXT("OJJInventoryInteract")), false, this);

    bool bHit = World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, TraceParams);
    
    // 창고 컴포넌트 구조체 캐스팅
    bool bIsValidWarehouse = false;
    FName InventoryFormFilter = NAME_None;
    if (bHit && Hit.GetActor())
    {
        // AWarehousePort 뿐만 아니라 일반 머신 베이스 계열인지도 체크
        if (Hit.GetActor()->IsA(AWarehousePort::StaticClass()) || Hit.GetActor()->GetName().Contains(TEXT("Warehouse")))
        {
            bIsValidWarehouse = true;
            InventoryFormFilter = TEXT("solid");
        }
        else if (Hit.GetActor()->IsA(ALiquidTank::StaticClass()) || Hit.GetActor()->GetName().Contains(TEXT("LiquidTank")))
        {
            bIsValidWarehouse = true;
            InventoryFormFilter = TEXT("liquid");
        }
        else if (Hit.GetActor()->IsA(AMachineBase::StaticClass()))
        {
            bIsValidWarehouse = true;
        }
    }

    // 테스트 환경 편의를 위해, 바라보고 있는 액터가 감지가 안 되더라도 
    // 최소한의 방어선만 치고 UI가 무조건 생성되도록 우회 통과시킵니다.
    if (!bIsValidWarehouse)
    {
        UE_LOG(LogTemp, Warning, TEXT("[가방 경고] 시선 끝에 창고(WarehousePort)가 정확히 조준되지 않았습니다! (디버깅을 위해 생성을 강제 진행합니다)"));
    }

    // 3. 인벤토리 오픈 및 UI 생성
    if (!InventoryWidgetInstance && InventoryWidgetClass)
    {
       InventoryWidgetInstance = CreateWidget<UUI_Inventory>(PC, InventoryWidgetClass);
    }

	if (InventoryWidgetInstance)
	{
		// 화면에 뷰포트 업로드하기 직전에 연산 가동!
		// 바라보는 시선 끝에 창고가 없으면(bIsValidWarehouse = false) 왼쪽(중앙 방향)으로 이동합니다.
		InventoryWidgetInstance->AdjustInventoryLayout(bIsValidWarehouse);

		// 중복되던 슬롯 필터 및 뷰포트 추가 로직 하나로 깔끔하게 압축
		InventoryWidgetInstance->SetItemFormFilter(InventoryFormFilter);
		InventoryWidgetInstance->AddToViewport(-1); 
		InventoryWidgetInstance->RefreshInventoryWindow();
		bIsInventoryOpen = true;
       
		// UI 포커스 및 인풋 모드 설정
		FInputModeGameAndUI InputModeData;
		InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputModeData);
		PC->SetShowMouseCursor(true);

		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
			{
				QuestManager->NotifyTutorialEvent(TEXT("InventoryOpen"));
			}
		}
       
		// 위젯 자체에 강제로 마우스 포커스를 심어 드래그 스타트 신호 보호
		InventoryWidgetInstance->SetKeyboardFocus();

		GetWorldTimerManager().SetTimer(
		   InventoryRefreshTimerHandle, 
		   this, 
		   &AOJJ_Player::UpdateInventoryRealtime, 
		   0.1f, 
		   true
		);
	}
}

void AOJJ_Player::TutorialAdvance()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
		{
			QuestManager->CompleteCurrentTutorialQuestForTest();
		}
	}
}

void AOJJ_Player::TutorialLog()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UQuestManagerSubsystem* QuestManager = GameInstance->GetSubsystem<UQuestManagerSubsystem>())
		{
			QuestManager->LogCurrentTutorialQuestTestState();
		}
	}
}

void AOJJ_Player::SetMachineLevel(const FString& MachineTypeName, int32 NewLevel)
{
	if (MachineTypeName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SetMachineLevel] MachineTypeName is empty."));
		return;
	}

	if (NewLevel <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SetMachineLevel] NewLevel must be greater than 0."));
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UMachineSubsystem* MachineSubsystem = GameInstance
		? GameInstance->GetSubsystem<UMachineSubsystem>()
		: nullptr;
	if (!MachineSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SetMachineLevel] MachineSubsystem not found."));
		return;
	}

	const FName MachineType(*MachineTypeName);
	if (!MachineSubsystem->SetMachineLevel(MachineType, NewLevel))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[SetMachineLevel] Failed to set %s to level %d."),
			*MachineTypeName,
			NewLevel);
		return;
	}

	UE_LOG(LogTemp, Log,
		TEXT("[SetMachineLevel] %s level set to %d."),
		*MachineTypeName,
		NewLevel);
}

void AOJJ_Player::UpgradeMachineLevel(const FString& MachineTypeName, int32 UpgradeCount)
{
	if (MachineTypeName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[UpgradeMachineLevel] MachineTypeName is empty."));
		return;
	}

	if (UpgradeCount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UpgradeMachineLevel] UpgradeCount must be greater than 0."));
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UMachineSubsystem* MachineSubsystem = GameInstance
		? GameInstance->GetSubsystem<UMachineSubsystem>()
		: nullptr;
	if (!MachineSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UpgradeMachineLevel] MachineSubsystem not found."));
		return;
	}

	const FName MachineType(*MachineTypeName);
	int32 SuccessCount = 0;
	for (int32 Index = 0; Index < UpgradeCount; ++Index)
	{
		if (!MachineSubsystem->SetMachineLevel(MachineType, MachineSubsystem->GetMachineLevel(MachineType) + 1))
		{
			break;
		}

		++SuccessCount;
	}

	if (SuccessCount == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[UpgradeMachineLevel] Failed to upgrade %s. Current level=%d."),
			*MachineTypeName,
			MachineSubsystem->GetMachineLevel(MachineType));
		return;
	}

	UE_LOG(LogTemp, Log,
		TEXT("[UpgradeMachineLevel] %s upgraded by %d. Current level=%d."),
		*MachineTypeName,
		SuccessCount,
		MachineSubsystem->GetMachineLevel(MachineType));
}

void AOJJ_Player::ResetGame()
{
	UGameInstance* GameInstance = GetGameInstance();
	UFactorySaveSubsystem* SaveSubsystem = GameInstance
		? GameInstance->GetSubsystem<UFactorySaveSubsystem>()
		: nullptr;
	if (!SaveSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ResetGame] FactorySaveSubsystem not found."));
		return;
	}

	const bool bDeletedSave = SaveSubsystem->ResetToNewGame();
	UE_LOG(LogTemp, Log, TEXT("[ResetGame] Save reset requested. DeletedExistingSave=%s"),
		bDeletedSave ? TEXT("true") : TEXT("false"));

	if (UPlayerWarehouseSubsystem* WarehouseSubsystem = GameInstance->GetSubsystem<UPlayerWarehouseSubsystem>())
	{
		const int32 PreviousItemTypeCount = WarehouseSubsystem->GetStoredItems().Num();
		WarehouseSubsystem->ClearWarehouse();
		UE_LOG(LogTemp, Log, TEXT("[ResetGame] Warehouse cleared. PreviousItemTypes=%d"), PreviousItemTypeCount);
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FString LevelName = World->GetOutermost()->GetName();
	if (LevelName.IsEmpty())
	{
		LevelName = UWorld::RemovePIEPrefix(World->GetMapName());
	}

	UE_LOG(LogTemp, Log, TEXT("[ResetGame] Reopening level: %s"), *LevelName);
	UGameplayStatics::OpenLevel(this, FName(*LevelName));
}

void AOJJ_Player::BackupAndResetGame()
{
	UGameInstance* GameInstance = GetGameInstance();
	UFactorySaveSubsystem* SaveSubsystem = GameInstance
		? GameInstance->GetSubsystem<UFactorySaveSubsystem>()
		: nullptr;
	if (!SaveSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BackupAndResetGame] FactorySaveSubsystem not found."));
		return;
	}

	const bool bBackedUp = SaveSubsystem->BackupCurrentGame();
	UE_LOG(LogTemp, Log, TEXT("[BackupAndResetGame] BackupRequested=%s"),
		bBackedUp ? TEXT("true") : TEXT("false"));
	if (!bBackedUp)
	{
		return;
	}

	ResetGame();
}

void AOJJ_Player::RestoreBackupGame()
{
	UGameInstance* GameInstance = GetGameInstance();
	UFactorySaveSubsystem* SaveSubsystem = GameInstance
		? GameInstance->GetSubsystem<UFactorySaveSubsystem>()
		: nullptr;
	if (!SaveSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RestoreBackupGame] FactorySaveSubsystem not found."));
		return;
	}

	const bool bRestored = SaveSubsystem->RestoreBackupGame();
	UE_LOG(LogTemp, Log, TEXT("[RestoreBackupGame] BackupRestoreRequested=%s"),
		bRestored ? TEXT("true") : TEXT("false"));
	if (!bRestored)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FString LevelName = World->GetOutermost()->GetName();
	if (LevelName.IsEmpty())
	{
		LevelName = UWorld::RemovePIEPrefix(World->GetMapName());
	}

	UE_LOG(LogTemp, Log, TEXT("[RestoreBackupGame] Reopening level: %s"), *LevelName);
	UGameplayStatics::OpenLevel(this, FName(*LevelName));
}

void AOJJ_Player::ClearWarehouse()
{
	UGameInstance* GameInstance = GetGameInstance();
	UPlayerWarehouseSubsystem* WarehouseSubsystem = GameInstance
		? GameInstance->GetSubsystem<UPlayerWarehouseSubsystem>()
		: nullptr;
	if (!WarehouseSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ClearWarehouse] PlayerWarehouseSubsystem not found."));
		return;
	}

	const int32 PreviousItemTypeCount = WarehouseSubsystem->GetStoredItems().Num();
	WarehouseSubsystem->ClearWarehouse();

	bool bSaved = false;
	if (UFactorySaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UFactorySaveSubsystem>())
	{
		bSaved = SaveSubsystem->SaveCurrentGame();
	}

	UE_LOG(LogTemp, Log, TEXT("[ClearWarehouse] Warehouse cleared. PreviousItemTypes=%d Saved=%s"),
		PreviousItemTypeCount,
		bSaved ? TEXT("true") : TEXT("false"));
}

void AOJJ_Player::Give(const FString& ItemID, int32 Count)
{
	if (ItemID.TrimStartAndEnd().IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Give] ItemID is empty. Usage: give iron_ingot 10"));
		return;
	}

	if (Count <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Give] Count must be greater than 0. Usage: give iron_ingot 10"));
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UPlayerWarehouseSubsystem* WarehouseSubsystem = GameInstance
		? GameInstance->GetSubsystem<UPlayerWarehouseSubsystem>()
		: nullptr;
	if (!WarehouseSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Give] PlayerWarehouseSubsystem not found."));
		return;
	}

	const FName TargetItemID(*ItemID.TrimStartAndEnd());
	if (!WarehouseSubsystem->AddItem(TargetItemID, Count))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Give] Failed to add item. ItemID=%s Count=%d"), *TargetItemID.ToString(), Count);
		return;
	}

	bool bSaved = false;
	if (GameInstance)
	{
		if (UFactorySaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UFactorySaveSubsystem>())
		{
			bSaved = SaveSubsystem->SaveCurrentGame();
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Give] Added %s x%d to warehouse. NewCount=%d Saved=%s"),
		*TargetItemID.ToString(),
		Count,
		WarehouseSubsystem->GetItemCount(TargetItemID),
		bSaved ? TEXT("true") : TEXT("false"));
}

void AOJJ_Player::TriggerPlanetEvent(const FString& EventName, float Severity, float DurationSeconds)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TriggerPlanetEvent] World is null."));
		return;
	}

	UPlanetEventManagerSubsystem* PlanetManager = World->GetSubsystem<UPlanetEventManagerSubsystem>();
	if (!PlanetManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TriggerPlanetEvent] PlanetEventManagerSubsystem not found."));
		return;
	}

	const FString NormalizedEventName = EventName.TrimStartAndEnd().ToLower();
	if (NormalizedEventName.IsEmpty())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[TriggerPlanetEvent] EventName is empty. Use magnetic, sand, or none."));
		return;
	}

	if (NormalizedEventName == TEXT("none") || NormalizedEventName == TEXT("clear"))
	{
		PlanetManager->EndActiveEvent();
		UE_LOG(LogTemp, Log, TEXT("[TriggerPlanetEvent] Cleared active planet event."));
		return;
	}

	EPlanetEventType EventType = EPlanetEventType::None;
	if (NormalizedEventName == TEXT("magnetic") || NormalizedEventName == TEXT("magneticstorm"))
	{
		EventType = EPlanetEventType::MagneticStorm;
	}
	else if (NormalizedEventName == TEXT("sand") || NormalizedEventName == TEXT("sandstorm"))
	{
		EventType = EPlanetEventType::SandStorm;
	}
	else
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[TriggerPlanetEvent] Unknown event '%s'. Use magnetic, sand, or none."),
			*EventName);
		return;
	}

	if (PlanetManager->GetEventState().Type != EPlanetEventType::None)
	{
		PlanetManager->EndActiveEvent();
	}

	const bool bStarted = PlanetManager->StartPlanetEvent(EventType, Severity, DurationSeconds);
	if (bStarted)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[TriggerPlanetEvent] Event=%s Severity=%.2f Duration=%.2f Started=true"),
			*UEnum::GetValueAsString(EventType),
			Severity,
			DurationSeconds);
		return;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[TriggerPlanetEvent] Event=%s Severity=%.2f Duration=%.2f Started=false"),
		*UEnum::GetValueAsString(EventType),
		Severity,
		DurationSeconds);
}

void AOJJ_Player::UpdateInventoryRealtime()
{
	if (bIsInventoryOpen && InventoryWidgetInstance)
	{
		InventoryWidgetInstance->UpdateSlotQuantitiesOnly();
	}
	else
	{
		GetWorldTimerManager().ClearTimer(InventoryRefreshTimerHandle);
	}
}
