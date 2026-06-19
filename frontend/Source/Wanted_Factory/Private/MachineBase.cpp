
#include "MachineBase.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "FactoryManagerSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Machines/MachineSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "PlanetEventManagerSubsystem.h"
#include "PlayerWarehouseSubsystem.h"
#include "RecipeManagerSubsystem.h"
#include "Wanted_Factory.h"
#include "Algo/Count.h"

namespace
{
	void AddRecipeItemQuantity(TMap<FName, int32>& ItemQuantities, FName ItemID, int32 Count)
	{
		if (ItemID.IsNone() || Count <= 0)
		{
			return;
		}

		ItemQuantities.FindOrAdd(ItemID) += Count;
	}

	TMap<FName, int32> BuildInputQuantities(const FRecipeTable& Recipe)
	{
		TMap<FName, int32> InputQuantities;
		AddRecipeItemQuantity(InputQuantities, Recipe.InputItem1, Recipe.InputQty1);
		AddRecipeItemQuantity(InputQuantities, Recipe.InputItem2, Recipe.InputQty2);
		AddRecipeItemQuantity(InputQuantities, Recipe.InputItem3, Recipe.InputQty3);
		return InputQuantities;
	}

	TMap<FName, int32> BuildOutputQuantities(const FRecipeTable& Recipe)
	{
		TMap<FName, int32> OutputQuantities;
		AddRecipeItemQuantity(OutputQuantities, Recipe.OutputItem1, Recipe.OutputQty1);
		AddRecipeItemQuantity(OutputQuantities, Recipe.OutputItem2, Recipe.OutputQty2);
		return OutputQuantities;
	}

	FString FormatItemMap(const TMap<FName, int32>& Items)
	{
		if (Items.Num() == 0)
		{
			return TEXT("None");
		}

		FString Result;
		for (const TPair<FName, int32>& Item : Items)
		{
			if (Item.Key.IsNone() || Item.Value <= 0)
			{
				continue;
			}

			if (!Result.IsEmpty())
			{
				Result += TEXT("\n");
			}
			Result += FString::Printf(TEXT("%s x%d"), *Item.Key.ToString(), Item.Value);
		}

		return Result.IsEmpty() ? FString(TEXT("None")) : Result;
	}

	void FitMeshToGrid(UStaticMeshComponent* MeshComponent, FIntPoint GridSize, FVector MeshScaleMultiplier)
	{
		if (!MeshComponent)
		{
			return;
		}

		// 정규화 스케일은 단일 진실원(OJJ_ComputeMeshFitScale)에 위임 — 고스트 프리뷰와 동일 식(무회귀).
		MeshComponent->SetWorldScale3D(
			AMachineBase::OJJ_ComputeMeshFitScale(MeshComponent->GetStaticMesh(), GridSize, MeshScaleMultiplier));
	}

	void RequestPowerGridRefresh(AMachineBase* Machine)
	{
		if (!Machine)
		{
			return;
		}

		if (UGameInstance* GameInstance = Machine->GetGameInstance())
		{
			if (UFactoryManagerSubsystem* FactoryManager = GameInstance->GetSubsystem<UFactoryManagerSubsystem>())
			{
				FactoryManager->UpdatePowerGrid();
			}
		}
	}
}

FVector AMachineBase::OJJ_ComputeMeshFitScale(const UStaticMesh* Mesh, FIntPoint GridSize, FVector MeshScaleMultiplier)
{
	// 메시 네이티브 바운즈를 footprint(GridSize × MeshFitCellWorld)에 정규화 → 셀 자동 정합.
	// 폴백: 메시 널/바운즈 0이면 기존 식(GridSize 그대로). 기본 큐브(100uu) + CellWorld 100이면
	// SX/SY = GridSize라 기존과 수치 동일(무회귀). 높이(Z)는 XY 중 작은 스케일을 따라 키 큰 메시 왜곡 방지.
	FVector Scale(GridSize.X, GridSize.Y, 1.0f);
	if (Mesh)
	{
		const FVector MeshSize = Mesh->GetBoundingBox().GetSize();
		if (MeshSize.X > KINDA_SMALL_NUMBER && MeshSize.Y > KINDA_SMALL_NUMBER)
		{
			const float SX = (GridSize.X * MeshFitCellWorld) / MeshSize.X;
			const float SY = (GridSize.Y * MeshFitCellWorld) / MeshSize.Y;
			Scale = FVector(SX, SY, FMath::Min(SX, SY));
		}
	}

	// 머신별 미세조정 배율(기본 1,1,1 → 정규화 결과 그대로).
	return Scale * MeshScaleMultiplier;
}

AMachineBase::AMachineBase()
{
	InputPortCount = 1;
	PrimaryActorTick.bCanEverTick = true;
	OutputPortCount = 1;
	MachineType = TEXT("None");


	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	MeshComponent =
		CreateDefaultSubobject<UStaticMeshComponent>(
			TEXT("Mesh"));

	MeshComponent->SetupAttachment(Root);

	// 메쉬 방향 보정: 머신 메쉬의 시각적 입출력부가 논리 포트 방향(액터 forward 기반)과 -90° Yaw
	// 어긋나는 문제(전 머신 균일, PIE 관찰 확정)를 +90° 회전으로 상쇄. RelativeRotation은 자식 메쉬만
	// 회전시키므로 액터 forward/footprint/포트 셀 계산(GetActorForwardVector 기반)에는 무영향 —
	// 시각 정렬만 보정한다. 기본 Cube는 대칭이라 무해. 부호(+90)는 PIE로 검증: 전 머신 배출부가
	// 출력(주황) 포트 화살표 방향과 일치, BP 수동 RelativeRotation 오버라이드(이중 보정) 없음 확인.
	MeshComponent->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));

	DebugBufferText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("DebugBufferText"));
	DebugBufferText->SetupAttachment(Root);
	DebugBufferText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DebugBufferText->SetHorizontalAlignment(EHTA_Center);
	DebugBufferText->SetVerticalAlignment(EVRTA_TextCenter);
	DebugBufferText->SetWorldSize(DebugTextWorldSize);
	DebugBufferText->SetRelativeLocation(DebugTextOffset);
	DebugBufferText->SetRelativeRotation(FRotator(60.0f, 0.0f, 0.0f));

	ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(CubeMesh.Object);
	}
	ConstructorHelpers::FObjectFinder<UMaterial> MaterialAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MaterialAsset.Succeeded())
	{
		MeshComponent->SetMaterial(0, MaterialAsset.Object);
	}
}

void AMachineBase::ApplyMachineData(const FMachineTableRow& MachineData)
{
	InputPortCount = FMath::Max(0, MachineData.InputPortCnt);
	OutputPortCount = FMath::Max(0, MachineData.OutputPortCnt);
	InputBufferCount = FMath::Max(0, MachineData.InputBufCnt);
	OutputBufferCount = FMath::Max(0, MachineData.OutputBufCnt);
	GridSize = FIntPoint(FMath::Max(1, MachineData.Xlen), FMath::Max(1, MachineData.Ylen));

	MaxDurability = FMath::Max(1.f, MachineData.Durability);
	CurrentDurability = FMath::Clamp(CurrentDurability, 0.f, MaxDurability);
	PowerConsumption = FMath::Max(0.f, MachineData.Power);
	RepairCostItemID = MachineData.CostType;
	RepairBaseCostQty = FMath::Max(0, MachineData.CostQty);

	if (MeshComponent)
	{
		if (!MachineData.StaticMeshAsset.IsNull())
		{
			if (UStaticMesh* StaticMeshAsset = MachineData.StaticMeshAsset.LoadSynchronous())
			{
				MeshComponent->EmptyOverrideMaterials();
				MeshComponent->SetStaticMesh(StaticMeshAsset);
			}
		}

		if (!MachineData.MaterialAsset.IsNull())
		{
			if (UMaterialInterface* MaterialAsset = MachineData.MaterialAsset.LoadSynchronous())
			{
				const int32 MaterialCount = FMath::Max(1, MeshComponent->GetNumMaterials());
				for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
				{
					MeshComponent->SetMaterial(MaterialIndex, MaterialAsset);
				}
			}
		}

		FitMeshToGrid(MeshComponent, GridSize, MeshScaleMultiplier);
	}

	InputPorts.Reset();
	for (int32 PortIndex = 0; PortIndex < InputPortCount; ++PortIndex)
	{
		FMachinePortData InputPort;
		InputPort.PortIndex = PortIndex;
		InputPort.PortType = EPortType::Input;
		InputPorts.Add(InputPort);
	}

	OutputPorts.Reset();
	for (int32 PortIndex = 0; PortIndex < OutputPortCount; ++PortIndex)
	{
		FMachinePortData OutputPort;
		OutputPort.PortIndex = PortIndex;
		OutputPort.PortType = EPortType::Output;
		OutputPorts.Add(OutputPort);
	}
}

bool AMachineBase::ApplyMachineDataFromSubsystem()
{
	if (MachineType.IsNone())
	{
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return false;
	}

	UMachineSubsystem* MachineSubsystem = GameInstance->GetSubsystem<UMachineSubsystem>();
	if (!MachineSubsystem)
	{
		return false;
	}

	FMachineTableRow MachineData;
	if (!MachineSubsystem->FindMachineData(MachineType, MachineData))
	{
		return false;
	}

	ApplyMachineData(MachineData);
	return true;
}


// Called when the game starts or when spawned
void AMachineBase::BeginPlay()
{
	Super::BeginPlay();

	ApplyMachineDataFromSubsystem();
	CurrentDurability = MaxDurability;
	if (UWorld* World = GetWorld())
	{
		if (UPlanetEventManagerSubsystem* PlanetEventManager = World->GetSubsystem<UPlanetEventManagerSubsystem>())
		{
			PlanetEventManager->RegisterMachine(this);
		}
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UFactoryManagerSubsystem* FactoryManager = GameInstance->GetSubsystem<UFactoryManagerSubsystem>())
			{
				FactoryManager->RegisterMachine(this);
			}
		}
	}

	RefreshMachineState();
}

void AMachineBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UPlanetEventManagerSubsystem* PlanetEventManager = World->GetSubsystem<UPlanetEventManagerSubsystem>())
		{
			PlanetEventManager->UnregisterMachine(this);
		}
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UFactoryManagerSubsystem* FactoryManager = GameInstance->GetSubsystem<UFactoryManagerSubsystem>())
			{
				FactoryManager->UnregisterMachine(this);
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AMachineBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyMachineDataFromSubsystem();
	if (MeshComponent)
	{
		// 정규화 스케일은 단일 진실원(OJJ_ComputeMeshFitScale)에 위임 — 고스트 프리뷰와 동일 식(무회귀).
		MeshComponent->SetWorldScale3D(
			OJJ_ComputeMeshFitScale(MeshComponent->GetStaticMesh(), GridSize, MeshScaleMultiplier));
	}

	CurrentDurability = FMath::Clamp(CurrentDurability, 0.f, MaxDurability);
	UpdateDebugBufferText();
	UpdateDebugTextFacingPlayer();
}

// Called every frame
void AMachineBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bShowDebugBufferText)
	{
		UpdateDebugTextFacingPlayer();
	}
}

bool AMachineBase::CanPlace()
{
	// ---------------------------
	// 나중에 GridManager에서 체크하기
	// ---------------------------

	return true;
}

void AMachineBase::OnRemovedFromGrid()
{
	StopProcess();

	if (!ShouldRefundBuffersToWarehouseOnRemoval())
	{
		return;
	}

	RefundBufferedItemsToWarehouse();
}

bool AMachineBase::AddItem(FName ItemID, int32 Count)
{
	if (isBroken() && bDisableWhenBroken)
	{
		return false;
	}

	if (ItemID.IsNone() || Count <= 0)
	{
		return false;
	}
	
	if (!CanAddInputItem(ItemID, Count))
	{
		LOG_SSR_W(TEXT("Input Inventory Full : %s %d / %d"),
			*ItemID.ToString(),
			InputInventory.FindRef(ItemID),
			MaxInputPerItem
		);

		return false;
	}

	int32& ItemCount = InputInventory.FindOrAdd(ItemID);
	ItemCount += Count;

	LOG_SSR_W(TEXT("Input Inventory Added : %s x %d / %d"),
		*ItemID.ToString(),
		ItemCount,
		MaxInputPerItem
	);
	
	if (MachineState == EMachineState::Blocked)
	{
		MachineState = EMachineState::Idle;
	}

	UpdateDebugBufferText();
	TryStartProcess();
	
	return true;
}

void AMachineBase::TryStartProcess()
{
	RefreshMachineState();
	
	if (MachineState == EMachineState::Working)
	{
		return;
	}

	if (MachineState == EMachineState::Disabled ||
		MachineState == EMachineState::NoPower ||
		MachineState == EMachineState::Blocked)
	{
		return;
	}

	if (InputInventory.Num() <= 0)
	{
		return;
	}

	URecipeManagerSubsystem* RecipeManager =
		GetGameInstance() ? GetGameInstance()->GetSubsystem<URecipeManagerSubsystem>() : nullptr;

	if (!RecipeManager)
	{
		LOG_SSR_W(TEXT("RecipeManagerSubSystem is NULL"));
		return;
	}

	bool bHasBlockedCraftableRecipe = false;

	for (const TPair<FName, int32>& InputPair : InputInventory)
	{
		TArray<FRecipeTable> FoundRecipes;

		const bool bFoundRecipes =
			RecipeManager->FindRecipesByInputItem(InputPair.Key, FoundRecipes);

		if (!bFoundRecipes)
		{
			continue;
		}

		for (const FRecipeTable& Recipe : FoundRecipes)
		{
			if (Recipe.MachineType != MachineType)
			{
				continue;
			}

			if (!HasEnoughIngredients(Recipe))
			{
				continue;
			}
			
			if (!CanAddToOutputBuffer(Recipe))
			{
				bHasBlockedCraftableRecipe = true;
				continue;
			}

			CurrentRecipe = Recipe;
			ProcessTime = CurrentRecipe.CraftingTime;

			StartProcess();
			return;
		}
	}

	if (bHasBlockedCraftableRecipe)
	{
		MachineState = EMachineState::Blocked;

		LOG_SSR_W(TEXT("Cannot start process. Output Buffer Blocked."));
		return;
	}

	LOG_SSR_W(TEXT("No craftable recipe found."));
}

void AMachineBase::StartProcess()
{
	RefreshMachineState();

	if (MachineState == EMachineState::Working ||
		MachineState == EMachineState::Disabled ||
		MachineState == EMachineState::NoPower ||
		MachineState == EMachineState::Blocked)
	{
		return;
	}

	MachineState = EMachineState::Working;

	LOG_SSR_W(TEXT("Process Started: %s -> %s"),
		*CurrentRecipe.InputItem1.ToString(),
		*CurrentRecipe.OutputItem1.ToString()
	);

	GetWorld()->GetTimerManager().SetTimer(
		ProcessTimer,
		this,
		&AMachineBase::FinishProcess,
		GetEffectiveProcessTime(ProcessTime),
		false
	);
}

void AMachineBase::FinishProcess()
{
	if ((isBroken() && bDisableWhenBroken) ||
		MachineState == EMachineState::Disabled ||
		MachineState == EMachineState::NoPower)
	{
		return;
	}

	ProcessItem();

	MachineState = EMachineState::Idle;

	// 남은 재료가 있으면 자동으로 다음 생산
	TryStartProcess();
}

void AMachineBase::ProcessItem_Implementation()
{
	ConsumeIngredients(CurrentRecipe);

	AddOutputItem(CurrentRecipe.OutputItem1, CurrentRecipe.OutputQty1);
	AddOutputItem(CurrentRecipe.OutputItem2, CurrentRecipe.OutputQty2);

	LOG_SSR_W(
		TEXT("Machine Processed: %s x%d, %s x%d, %s x%d -> %s x%d, %s x%d"),
		*CurrentRecipe.InputItem1.ToString(),
		CurrentRecipe.InputQty1,
		*CurrentRecipe.InputItem2.ToString(),
		CurrentRecipe.InputQty2,
		*CurrentRecipe.InputItem3.ToString(),
		CurrentRecipe.InputQty3,
		*CurrentRecipe.OutputItem1.ToString(),
		CurrentRecipe.OutputQty1,
		*CurrentRecipe.OutputItem2.ToString(),
		CurrentRecipe.OutputQty2
	);
}

void AMachineBase::StopProcess()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ProcessTimer);
	}

	if (MachineState == EMachineState::Working)
	{
		MachineState = EMachineState::Idle;
	}
}

bool AMachineBase::CanAddInputItem(FName ItemID, int32 Count) const
{
	if (ItemID.IsNone() || Count <= 0)
	{
		return false;
	}
	
	const int32 CurrentCount = InputInventory.FindRef(ItemID);
	
	return CurrentCount + Count <= MaxInputPerItem;
}

bool AMachineBase::HasEnoughIngredients(const FRecipeTable& Recipe) const
{
	const TMap<FName, int32> InputQuantities = BuildInputQuantities(Recipe);

	for (const TPair<FName, int32>& InputQuantity : InputQuantities)
	{
		const int32* FoundCount = InputInventory.Find(InputQuantity.Key);

		if (!FoundCount)
		{
			return false;
		}

		if (*FoundCount < InputQuantity.Value)
		{
			return false;
		}
	}

	return true;
}

void AMachineBase::ConsumeIngredients(const FRecipeTable& Recipe)
{
	const TMap<FName, int32> InputQuantities = BuildInputQuantities(Recipe);

	for (const TPair<FName, int32>& InputQuantity : InputQuantities)
	{
		int32* FoundCount = InputInventory.Find(InputQuantity.Key);

		if (!FoundCount)
		{
			continue;
		}

		*FoundCount -= InputQuantity.Value;

		if (*FoundCount <= 0)
		{
			InputInventory.Remove(InputQuantity.Key);
		}
	}

	UpdateDebugBufferText();
}

void AMachineBase::AddOutputItem(FName ItemID, int32 Count)
{
	if (ItemID.IsNone() || Count <= 0)
	{
		return;
	}
	
	int32 CurrentCount = OutputBuffer.FindRef(ItemID);
	
	if (CurrentCount + Count > MaxBufferPerItem)
	{
		MachineState = EMachineState::Blocked;
		
		LOG_SSR_W(TEXT("Output Buffer Full : %s %d / %d"),
			*ItemID.ToString(),
			CurrentCount,
			MaxBufferPerItem
		);
		
		return;
	}

	int32& BufferCount = OutputBuffer.FindOrAdd(ItemID);
	BufferCount += Count;

	LOG_SSR_W(TEXT("Output Buffer Added : %s x %d / %d"),
		*ItemID.ToString(),
		BufferCount,
		MaxBufferPerItem
	);

	UpdateDebugBufferText();
}

bool AMachineBase::CanAddToOutputBuffer(const FRecipeTable& Recipe) const
{
	const TMap<FName, int32> OutputQuantities = BuildOutputQuantities(Recipe);

	for (const TPair<FName, int32>& OutputQuantity : OutputQuantities)
	{
		const int32 CurrentCount = OutputBuffer.FindRef(OutputQuantity.Key);
		
		if (CurrentCount + OutputQuantity.Value > MaxBufferPerItem)
		{
			return false;
		}
	}
	
	return true;
}

bool AMachineBase::TakeOutputItem(FName ItemID, int32 Count)
{
	if (ItemID.IsNone() || Count <= 0)
	{
		return false;
	}
	
	int32* FoundCount = OutputBuffer.Find(ItemID);
	
	if (!FoundCount || *FoundCount < Count)
	{
		LOG_SSR_W(TEXT("TakeOutputItem Failed : %s"), *ItemID.ToString());
		return false;
	}
	
	*FoundCount -= Count;
	
	LOG_SSR_W(TEXT("TakeOutputItem Success : %s x %d, Remain %d / %d"),
		*ItemID.ToString(),
		Count,
		*FoundCount,
		MaxBufferPerItem
	);

	if (*FoundCount <= 0)
	{
		OutputBuffer.Remove(ItemID);
	}

	if (MachineState == EMachineState::Blocked)
	{
		MachineState = EMachineState::Idle;
		TryStartProcess();
	}

	UpdateDebugBufferText();

	return true;
}

bool AMachineBase::PeekFirstOutputItem(FName& OutItemID) const
{
	OutItemID = NAME_None;

	for (const TPair<FName, int32>& Output : OutputBuffer)
	{
		if (!Output.Key.IsNone() && Output.Value > 0)
		{
			OutItemID = Output.Key;
			return true;
		}
	}

	return false;
}

bool AMachineBase::TryTakeFirstOutputItem(FName& OutItemID)
{
	if (!PeekFirstOutputItem(OutItemID))
	{
		return false;
	}

	if (!TakeOutputItem(OutItemID, 1))
	{
		OutItemID = NAME_None;
		return false;
	}

	return true;
}

bool AMachineBase::CanReceiveConveyorItem(FName ItemID, int32 Count) const
{
	return CanAddInputItem(ItemID, Count);
}

void AMachineBase::GetSaveState(
	TMap<FName, int32>& OutInputInventory,
	TMap<FName, int32>& OutOutputBuffer,
	float& OutCurrentDurability) const
{
	OutInputInventory = InputInventory;
	OutOutputBuffer = OutputBuffer;
	OutCurrentDurability = CurrentDurability;
}

void AMachineBase::ApplySaveState(
	const TMap<FName, int32>& InInputInventory,
	const TMap<FName, int32>& InOutputBuffer,
	float InCurrentDurability)
{
	StopProcess();
	InputInventory = InInputInventory;
	OutputBuffer = InOutputBuffer;
	CurrentDurability = FMath::Clamp(InCurrentDurability, 0.0f, MaxDurability);
	UpdateDebugBufferText();
	RefreshMachineState();
	if (MachineState == EMachineState::Idle)
	{
		TryStartProcess();
	}
}

bool AMachineBase::ReceiveConveyorItem(FName ItemID, int32 Count)
{
	const bool bAdded = AddItem(ItemID, Count);
	UpdateDebugBufferText();
	return bAdded;
}

void AMachineBase::UpdateDebugBufferText()
{
	if (!DebugBufferText)
	{
		return;
	}

	DebugBufferText->SetVisibility(bShowDebugBufferText);
	DebugBufferText->SetWorldSize(DebugTextWorldSize);
	DebugBufferText->SetRelativeLocation(DebugTextOffset);
	if (!bShowDebugBufferText)
	{
		return;
	}

	const FString DebugText = FString::Printf(
		TEXT("Input\n%s\n\nOutput\n%s"),
		*FormatItemMap(InputInventory),
		*FormatItemMap(OutputBuffer));
	DebugBufferText->SetText(FText::FromString(DebugText));
}

void AMachineBase::UpdateDebugTextFacingPlayer()
{
	if (!bShowDebugBufferText || !DebugBufferText)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!PlayerController)
	{
		return;
	}

	APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager;
	if (!CameraManager)
	{
		return;
	}

	FVector ToCamera = CameraManager->GetCameraLocation() - DebugBufferText->GetComponentLocation();
	ToCamera.Z = 0.0f;
	if (ToCamera.IsNearlyZero())
	{
		return;
	}

	const float FacingYaw = ToCamera.Rotation().Yaw;
	DebugBufferText->SetWorldRotation(FRotator(0.0f, FacingYaw, 0.0f));
}

bool AMachineBase::TransferOutputToMachine(AMachineBase* TargetMachine, FName ItemID, int32 Count)
{
	// 전송 할 머신이 없으면?
	if (!TargetMachine)
	{
		LOG_SSR_W(TEXT("TransferOutputToMachine Failed : TargetMachine is NULL"));
		return false;
	}
	
	// 전송 할 머신이 본인이라면?
	if (TargetMachine == this)
	{
		LOG_SSR_W(TEXT("TransferOutputToMachine Failed : Cannot transfer to self"));
		return false;
	}
	
	// 이상한 입력 아이템 방지
	if (ItemID.IsNone() || Count <= 0)
	{
		return false;
	}
	
	// 내 출력 버퍼에 아이템이 있는지 체크
	int32* FoundCount = OutputBuffer.Find(ItemID);
	
	if (!FoundCount || *FoundCount < Count)
	{
		LOG_SSR_W(TEXT("TransferOutputToMachine Failed : Not enough item %s"),
			*ItemID.ToString()
		);
		return false;
	}
	
	// 아이템이 입력 버퍼에 들어갈수 있는지 체크
	if (!TargetMachine->CanReceiveConveyorItem(ItemID, Count))
	{
		LOG_SSR_W(TEXT("TransferOutputToMachine Failed : Target cannot receive %s %d / %d"),
			*ItemID.ToString(),
			TargetMachine->InputInventory.FindRef(ItemID),
			TargetMachine->MaxInputPerItem
		);

		return false;
	}
	
	// 1. 내 버퍼에서 먼저 꺼내자
	const bool bTakeSuccess = TakeOutputItem(ItemID, Count);
	
	if (!bTakeSuccess)
	{
		return false;
	}
	
	// // 2. 전송 할 기계에 InputInventory에 넣음
	const bool bAddSuccess = TargetMachine->AddItem(ItemID, Count);
	
	if (!bAddSuccess)
	{
		LOG_SSR_W(TEXT("TransferOutputToMachine Failed : AddItem Failed"));
		OutputBuffer.FindOrAdd(ItemID) += Count;
		UpdateDebugBufferText();
		return false;
	}
	
	LOG_SSR_W(TEXT("Transfer Success : %s x %d -> %s"),
		*ItemID.ToString(),
		Count,
		*TargetMachine->GetName()
	);

	return true;
}

void AMachineBase::DebugInventory()
{
	LOG_SSR_W(TEXT("========== Input Inventory =========="));

	for (const TPair<FName, int32>& Input : InputInventory)
	{
		LOG_SSR_W(
			TEXT("%s x %d"),
			*Input.Key.ToString(),
			Input.Value
		);
	}

	LOG_SSR_W(TEXT("========== Output Buffer =========="));

	for (const TPair<FName, int32>& Buffer : OutputBuffer)
	{
		LOG_SSR_W(TEXT("%s x %d / %d"),
			*Buffer.Key.ToString(),
			Buffer.Value,
			MaxBufferPerItem
		);
	}
	
	
	
	LOG_SSR_W(TEXT("========== Output Inventory =========="));

	for (const TPair<FName, int32>& Output : OutputInventory)
	{
		LOG_SSR_W(
			TEXT("%s x %d"),
			*Output.Key.ToString(),
			Output.Value
		);
	}
}

bool AMachineBase::ConnectOutputToMachine(int32 OutputPortIndex, AMachineBase* TargetMachine, int32 TargetInputPortIndex)
{
	// 대상 기계가 없으면 연결 실패
	if (!TargetMachine)
	{
		LOG_SSR_W(TEXT("Connect Failed : TargetMachine is NULL"));
		return false;
	}

	// 자기 자신에게 연결 방지
	if (TargetMachine == this)
	{
		LOG_SSR_W(TEXT("Connect Failed : Cannot connect to self"));
		return false;
	}

	// 내 출력 포트 번호가 유효한지 체크
	if (!OutputPorts.IsValidIndex(OutputPortIndex))
	{
		LOG_SSR_W(TEXT("Connect Failed : Invalid OutputPortIndex %d"), OutputPortIndex);
		return false;
	}

	// 상대 입력 포트 번호가 유효한지 체크
	if (!TargetMachine->InputPorts.IsValidIndex(TargetInputPortIndex))
	{
		LOG_SSR_W(TEXT("Connect Failed : Invalid TargetInputPortIndex %d"), TargetInputPortIndex);
		return false;
	}

	// 연결 정보 생성
	FMachinePortConnection NewConnection;
	NewConnection.FromOutputPortIndex = OutputPortIndex;
	NewConnection.TargetMachine = TargetMachine;
	NewConnection.TargetInputPortIndex = TargetInputPortIndex;

	// 기존 같은 출력 포트 연결 제거
	for (const FMachinePortConnection& ExistingConnection : OutputConnections)
	{
		if (ExistingConnection.FromOutputPortIndex == OutputPortIndex &&
			ExistingConnection.TargetMachine &&
			ExistingConnection.TargetMachine->InputPorts.IsValidIndex(ExistingConnection.TargetInputPortIndex))
		{
			ExistingConnection.TargetMachine->InputPorts[ExistingConnection.TargetInputPortIndex].bIsConnected = false;
		}
	}

	OutputConnections.RemoveAll(
		[OutputPortIndex](const FMachinePortConnection& Connection)
		{
			return Connection.FromOutputPortIndex == OutputPortIndex;
		}
	);

	// 새 연결 등록
	OutputConnections.Add(NewConnection);

	// 포트 연결 상태 표시
	OutputPorts[OutputPortIndex].bIsConnected = true;
	TargetMachine->InputPorts[TargetInputPortIndex].bIsConnected = true;

	LOG_SSR_W(TEXT("Connect Success : %s OutputPort %d -> %s InputPort %d"),
		*GetName(),
		OutputPortIndex,
		*TargetMachine->GetName(),
		TargetInputPortIndex
	);

	return true;
}

bool AMachineBase::TransferOutputByPort(int32 OutputPortIndex, FName ItemID, int32 Count)
{
	// 내 출력 포트 번호가 유효한지 체크
	if (!OutputPorts.IsValidIndex(OutputPortIndex))
	{
		LOG_SSR_W(TEXT("TransferOutputByPort Failed : Invalid OutputPortIndex %d"), OutputPortIndex);
		return false;
	}

	// 연결 정보 찾기
	const FMachinePortConnection* FoundConnection = OutputConnections.FindByPredicate(
		[OutputPortIndex](const FMachinePortConnection& Connection)
		{
			return Connection.FromOutputPortIndex == OutputPortIndex;
		}
	);

	// 연결된 대상이 없으면 실패
	if (!FoundConnection || !FoundConnection->TargetMachine)
	{
		LOG_SSR_W(TEXT("TransferOutputByPort Failed : No Connection OutputPort %d"), OutputPortIndex);
		return false;
	}

	// 기존 전송 함수 재사용
	return TransferOutputToMachine(
		FoundConnection->TargetMachine,
		ItemID,
		Count
	);
}

bool AMachineBase::isBroken() const
{
	return CurrentDurability <= 0.f;
}

void AMachineBase::DamageDurability(float DamageAmount)
{
	if (DamageAmount <= 0.f)
	{
		return; 
	}
	
	CurrentDurability = FMath::Clamp(CurrentDurability - DamageAmount, 0.f, MaxDurability);
	
	LOG_SSR_W(TEXT("Durability Damaged : %.1f / %.1f"),
		CurrentDurability,
		MaxDurability
		);
	
	OnDurabilityChanged.Broadcast(CurrentDurability, MaxDurability);
	RefreshMachineState();
	RequestPowerGridRefresh(this);
}

void AMachineBase::RepairDurability(float RepairAmount)
{
	if (RepairAmount <= 0.f)
	{
		return;
	}
		
	CurrentDurability = FMath::Clamp(CurrentDurability + RepairAmount, 0.f, MaxDurability);
	
	LOG_SSR_W(TEXT("Durability Repaired : %.1f / %.1f"),
		CurrentDurability,
		MaxDurability
	);
	
	OnDurabilityChanged.Broadcast(CurrentDurability, MaxDurability);
	RefreshMachineState();
	RequestPowerGridRefresh(this);
}

int32 AMachineBase::GetMaxRepairCostQty() const
{
	return FMath::FloorToInt(static_cast<float>(RepairBaseCostQty) * 0.8f);
}

int32 AMachineBase::GetRepairCostQtyForCurrentDurability() const
{
	const int32 MaxRepairCostQty = GetMaxRepairCostQty();
	if (MaxRepairCostQty <= 0 || MaxDurability <= 0.f)
	{
		return 0;
	}

	const float MissingDurability = FMath::Max(0.f, MaxDurability - CurrentDurability);
	if (MissingDurability <= 0.f)
	{
		return 0;
	}

	return FMath::FloorToInt((MissingDurability / MaxDurability) * MaxRepairCostQty);
}

bool AMachineBase::RepairUsingWarehouse()
{
	if (RepairCostItemID.IsNone())
	{
		return false;
	}

	const int32 RequiredCostQty = GetRepairCostQtyForCurrentDurability();
	if (RequiredCostQty <= 0)
	{
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UPlayerWarehouseSubsystem* Warehouse = GameInstance
		? GameInstance->GetSubsystem<UPlayerWarehouseSubsystem>()
		: nullptr;
	if (!Warehouse)
	{
		return false;
	}

	const int32 AvailableCostQty = Warehouse->GetItemCount(RepairCostItemID);
	const int32 ConsumedCostQty = FMath::Min(RequiredCostQty, AvailableCostQty);
	const int32 MaxRepairCostQty = GetMaxRepairCostQty();
	if (ConsumedCostQty <= 0 || MaxRepairCostQty <= 0)
	{
		return false;
	}

	if (!Warehouse->TakeItem(RepairCostItemID, ConsumedCostQty))
	{
		return false;
	}

	const float RepairAmount = MaxDurability * static_cast<float>(ConsumedCostQty) / MaxRepairCostQty;
	RepairDurability(RepairAmount);

	LOG_SSR_W(TEXT("Machine Repaired Using Warehouse : %s x %d -> %.1f / %.1f"),
		*RepairCostItemID.ToString(),
		ConsumedCostQty,
		CurrentDurability,
		MaxDurability
	);

	return true;
}

void AMachineBase::ApplyDurabilityDamage(float DamageAmount)
{
	DamageDurability(DamageAmount);
}

void AMachineBase::SetProvidedPower(float NewPower)
{
	const bool bHadEnoughPower = HasEnoughPower();
	CurrentProvidedPower = FMath::Max(0.f, NewPower);
	
	LOG_SSR_W(TEXT("Power Updated : %.1f / %.1f"),
		CurrentProvidedPower,
		PowerConsumption
	);
	
	RefreshMachineState();
	if (!bHadEnoughPower && HasEnoughPower() && MachineState == EMachineState::Idle)
	{
		TryStartProcess();
	}
}

bool AMachineBase::HasEnoughPower() const
{
	if (!bNeedPower)
	{
		return true;
	}
	
	return CurrentProvidedPower >= PowerConsumption;
}

bool AMachineBase::RefundBufferedItemsToWarehouse()
{
	UGameInstance* GameInstance = GetGameInstance();
	UPlayerWarehouseSubsystem* Warehouse = GameInstance
		? GameInstance->GetSubsystem<UPlayerWarehouseSubsystem>()
		: nullptr;
	if (!Warehouse)
	{
		return false;
	}

	bool bRefundedAny = false;

	for (const TPair<FName, int32>& Input : InputInventory)
	{
		if (!Input.Key.IsNone() && Input.Value > 0)
		{
			Warehouse->AddItem(Input.Key, Input.Value);
			bRefundedAny = true;
		}
	}

	for (const TPair<FName, int32>& Output : OutputBuffer)
	{
		if (!Output.Key.IsNone() && Output.Value > 0)
		{
			Warehouse->AddItem(Output.Key, Output.Value);
			bRefundedAny = true;
		}
	}

	InputInventory.Reset();
	OutputBuffer.Reset();
	OutputInventory.Reset();
	CurrentRecipe = FRecipeTable();
	UpdateDebugBufferText();
	RefreshMachineState();
	return bRefundedAny;
}

void AMachineBase::RefreshMachineState()
{
	if (isBroken() && bDisableWhenBroken)
	{
		MachineState = EMachineState::Disabled;
		StopProcess();
		return;
	}

	if (!HasEnoughPower())
	{
		MachineState = EMachineState::NoPower;
		StopProcess();
		return;
	}
	
	if (MachineState == EMachineState::NoPower || MachineState == EMachineState::Disabled)
	{
		MachineState = EMachineState::Idle;
	}
}

namespace EfficiencyKeys
{
	const FName MagneticStorm(TEXT("MagneticStorm"));
	const FName Power(TEXT("Power"));
}

void AMachineBase::SetEfficiencyModifier(FName Key, float Value)
{
	if (Key.IsNone())
	{
		return; // 빈 키 거부 (F2: 의도치 않은 공유/덮어쓰기 방지)
	}
	if (!FMath::IsFinite(Value))
	{
		return; // NaN/Inf 거부 (F1: 곱 합성 오염 방지)
	}
	// 0 곱(효율 소멸)·무한대 누적 양쪽을 막기 위해 0.01~100.0으로 클램프.
	EfficiencyModifiers.Add(Key, FMath::Clamp(Value, 0.01f, 100.0f));
}

void AMachineBase::ClearEfficiencyModifier(FName Key)
{
	EfficiencyModifiers.Remove(Key);
}

float AMachineBase::GetFinalEfficiency() const
{
	float Final = 1.0f;
	for (const TPair<FName, float>& Modifier : EfficiencyModifiers)
	{
		Final *= Modifier.Value;
	}
	return FMath::Max(0.01f, Final);
}

void AMachineBase::SetPlanetProductionEfficiency(float NewEfficiency)
{
	// [DEPRECATED] 효율 modifier 시스템의 MagneticStorm 키로 위임 (멤버 미러 폐기 — F3 스테일 제거).
	SetEfficiencyModifier(EfficiencyKeys::MagneticStorm, NewEfficiency);
}

float AMachineBase::GetEffectiveProcessTime(float BaseProcessTime) const
{
	// 효율 반영 공식(Base/Efficiency) 유지 — 효율원만 단일값에서 modifier 곱으로 전환.
	return FMath::Max(0.01f, BaseProcessTime) / GetFinalEfficiency();
}
