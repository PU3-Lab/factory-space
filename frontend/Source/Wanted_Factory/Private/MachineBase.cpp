
#include "MachineBase.h"

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
}

AMachineBase::AMachineBase()
{
	PrimaryActorTick.bCanEverTick = false; // true로 바꿀듯
	InputPortCount = 1;
	OutputPortCount = 1;
	MachineType = TEXT("None");


	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	MeshComponent =
		CreateDefaultSubobject<UStaticMeshComponent>(
			TEXT("Mesh"));

	MeshComponent->SetupAttachment(Root);

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


// Called when the game starts or when spawned
void AMachineBase::BeginPlay()
{
	Super::BeginPlay();

}

void AMachineBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (MeshComponent)
	{
		float ScaleX = GridSize.X;
		float ScaleY = GridSize.Y;
		MeshComponent->SetWorldScale3D(FVector(ScaleX, ScaleY, 1.0f));
	}
}

// Called every frame
void AMachineBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

bool AMachineBase::CanPlace()
{
	// ---------------------------
	// 나중에 GridManager에서 체크하기
	// ---------------------------

	return true;
}

bool AMachineBase::AddItem(FName ItemID, int32 Count)
{
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

	TryStartProcess();
	
	return true;
}

void AMachineBase::TryStartProcess()
{
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
		GetGameInstance()->GetSubsystem<URecipeManagerSubsystem>();

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
	if (MachineState == EMachineState::Working)
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
		ProcessTime,
		false
	);
}

void AMachineBase::FinishProcess()
{
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
	GetWorld()->GetTimerManager().ClearTimer(ProcessTimer);

	MachineState = EMachineState::Idle;
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

	return true;
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
	if (!TargetMachine->CanAddInputItem(ItemID, Count))
	{
		LOG_SSR_W(TEXT("TransferOutputToMachine Failed : Target Input Full %s %d / %d"),
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
