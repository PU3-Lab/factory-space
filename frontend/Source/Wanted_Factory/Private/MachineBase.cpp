
#include "MachineBase.h"

#include "RecipeManagerSubsystem.h"
#include "Wanted_Factory.h"

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

void AMachineBase::AddItem(FName ItemID, int32 Count)
{
	if (ItemID.IsNone() || Count <= 0)
	{
		return;
	}

	int32& ItemCount = InputInventory.FindOrAdd(ItemID);
	ItemCount += Count;

	LOG_SSR_W(TEXT("Input Inventory Added : %s x %d"),
		*ItemID.ToString(),
		ItemCount
	);

	TryStartProcess();
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

			CurrentRecipe = Recipe;
			ProcessTime = CurrentRecipe.CraftingTime;

			StartProcess();
			return;
		}
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

bool AMachineBase::HasEnoughIngredients(const FRecipeTable& Recipe) const
{
	auto CheckIngredient = [this](FName ItemID, int32 Qty) -> bool
	{
		if (ItemID.IsNone() || Qty <= 0)
		{
			return true;
		}

		const int32* FoundCount = InputInventory.Find(ItemID);

		if (!FoundCount)
		{
			return false;
		}

		return *FoundCount >= Qty;
	};

	return
		CheckIngredient(Recipe.InputItem1, Recipe.InputQty1) &&
		CheckIngredient(Recipe.InputItem2, Recipe.InputQty2) &&
		CheckIngredient(Recipe.InputItem3, Recipe.InputQty3);
}

void AMachineBase::ConsumeIngredients(const FRecipeTable& Recipe)
{
	auto Consume = [this](FName ItemID, int32 Qty)
	{
		if (ItemID.IsNone() || Qty <= 0)
		{
			return;
		}

		int32* FoundCount = InputInventory.Find(ItemID);

		if (!FoundCount)
		{
			return;
		}

		*FoundCount -= Qty;

		if (*FoundCount <= 0)
		{
			InputInventory.Remove(ItemID);
		}
	};

	Consume(Recipe.InputItem1, Recipe.InputQty1);
	Consume(Recipe.InputItem2, Recipe.InputQty2);
	Consume(Recipe.InputItem3, Recipe.InputQty3);
}

void AMachineBase::AddOutputItem(FName ItemID, int32 Count)
{
	if (ItemID.IsNone() || Count <= 0)
	{
		return;
	}

	int32& OutputCount = OutputInventory.FindOrAdd(ItemID);
	OutputCount += Count;

	LOG_SSR_W(TEXT("Output Inventory Added : %s x %d"),
		*ItemID.ToString(),
		OutputCount
	);
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