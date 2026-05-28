
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

	CurrentInputItem = ItemID;
	CurrentInputCount = Count;

	LOG_SSR_W(TEXT("Input Item : %s x %d"), *ItemID.ToString(), CurrentInputCount);

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

	if (CurrentInputItem.IsNone() || CurrentInputCount <= 0)
	{
		return;
	}

	URecipeManagerSubsystem* RecipeManager = GetGameInstance()->GetSubsystem<URecipeManagerSubsystem>();

	if (!RecipeManager)
	{
		LOG_SSR_W(TEXT("RecipeManagerSubSystem is NULL"));
		return;
	}

	FRecipeTable FoundRecipe;

	const bool bFoundRecipe = RecipeManager->FindRecipeByInputItem(CurrentInputItem, FoundRecipe);

	if (!bFoundRecipe)
	{
		LOG_SSR_W(TEXT("No recipe found for input item: %s"), *CurrentInputItem.ToString());
		return;
	}

	if (FoundRecipe.MachineType != MachineType)
	{
		LOG_SSR_W(
			TEXT("Recipe machine type mismatch. Machine: %s / Recipe: %s"),
			*MachineType.ToString(),
			*FoundRecipe.MachineType.ToString()
		);
		return;
	}

	if (CurrentInputCount < FoundRecipe.InputQty)
	{
		LOG_SSR_W(
			TEXT("Not enough input. Need %d / Has %d"),
			FoundRecipe.InputQty,
			CurrentInputCount
		);
		return;
	}

	CurrentRecipe = FoundRecipe;
	ProcessTime = CurrentRecipe.CraftingTime;

	StartProcess();
}

void AMachineBase::StartProcess()
{
	if (MachineState == EMachineState::Working)
	{
		return;
	}

	MachineState = EMachineState::Working;

	LOG_SSR_W(TEXT("Process Started: %s -> %s"),
		*CurrentRecipe.InputItem.ToString(),
		*CurrentRecipe.OutputItem.ToString()
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
	CurrentInputCount -= CurrentRecipe.InputQty;

	if (CurrentInputCount <= 0)
	{
		CurrentInputCount = 0;
		CurrentInputItem = NAME_None;
	}

	int32& OutputCount = OutputInventory.FindOrAdd(CurrentRecipe.OutputItem);
	OutputCount += CurrentRecipe.OutputQty;

	LOG_SSR_W(
		TEXT("Machine Processed: %s x%d -> %s x%d"),
		*CurrentRecipe.InputItem.ToString(),
		CurrentRecipe.InputQty,
		*CurrentRecipe.OutputItem.ToString(),
		CurrentRecipe.OutputQty
	);

	LOG_SSR_W(
		TEXT("Output Inventory: %s x%d"),
		*CurrentRecipe.OutputItem.ToString(),
		OutputInventory[CurrentRecipe.OutputItem]
	);
}

void AMachineBase::StopProcess()
{
	GetWorld()->GetTimerManager().ClearTimer(ProcessTimer);

	MachineState = EMachineState::Idle;
}
