#include "DummyMachineBase.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "PlanetEventManagerSubsystem.h"
#include "RecipeManagerSubsystem.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Wanted_Factory.h"

ADummyMachineBase::ADummyMachineBase()
{
	PrimaryActorTick.bCanEverTick = false;
	InputPortCount = 1;
	OutputPortCount = 1;
	MachineType = TEXT("None");

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetupAttachment(Root);

	DebugBufferText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("DebugBufferText"));
	DebugBufferText->SetupAttachment(Root);
	DebugBufferText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DebugBufferText->SetHorizontalAlignment(EHTA_Center);
	DebugBufferText->SetVerticalAlignment(EVRTA_TextCenter);
	DebugBufferText->SetWorldSize(DebugTextWorldSize);
	DebugBufferText->SetRelativeLocation(DebugTextOffset);
	DebugBufferText->SetRelativeRotation(FRotator(60.0f, 0.0f, 0.0f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(CubeMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterial> MaterialAsset(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MaterialAsset.Succeeded())
	{
		MeshComponent->SetMaterial(0, MaterialAsset.Object);
	}
}

void ADummyMachineBase::BeginPlay()
{
	Super::BeginPlay();

	Durability = MaxDurability;

	if (UWorld* World = GetWorld())
	{
		if (UPlanetEventManagerSubsystem* PlanetEventManager = World->GetSubsystem<UPlanetEventManagerSubsystem>())
		{
			PlanetEventManager->RegisterMachine(this);
		}
	}

	UpdateDebugBufferText();
}

void ADummyMachineBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UPlanetEventManagerSubsystem* PlanetEventManager = World->GetSubsystem<UPlanetEventManagerSubsystem>())
		{
			PlanetEventManager->UnregisterMachine(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ADummyMachineBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (MeshComponent)
	{
		MeshComponent->SetWorldScale3D(FVector(GridSize.X, GridSize.Y, 1.0f));
	}

	Durability = FMath::Clamp(Durability, 0.0f, MaxDurability);
	UpdateDebugBufferText();
}

void ADummyMachineBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

bool ADummyMachineBase::CanPlace()
{
	return true;
}

bool ADummyMachineBase::AddItem(FName ItemID, int32 Count)
{
	if (bIsBroken || ItemID.IsNone() || Count <= 0)
	{
		return false;
	}

	if (!CanAddInputItem(ItemID, Count))
	{
		LOG_SSR_W(
			TEXT("Dummy input inventory full: %s %d / %d"),
			*ItemID.ToString(),
			InputInventory.FindRef(ItemID),
			MaxInputPerItem);
		return false;
	}

	int32& ItemCount = InputInventory.FindOrAdd(ItemID);
	ItemCount += Count;

	if (MachineState == EDummyMachineState::Blocked)
	{
		MachineState = EDummyMachineState::Idle;
	}

	UpdateDebugBufferText();
	TryStartProcess();
	return true;
}

void ADummyMachineBase::TryStartProcess()
{
	if (MachineState == EDummyMachineState::Working || bIsBroken)
	{
		return;
	}

	if (MachineState == EDummyMachineState::Disabled ||
		MachineState == EDummyMachineState::NoPower ||
		MachineState == EDummyMachineState::Blocked)
	{
		return;
	}

	if (InputInventory.Num() <= 0)
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	URecipeManagerSubsystem* RecipeManager = GameInstance
		? GameInstance->GetSubsystem<URecipeManagerSubsystem>()
		: nullptr;
	if (!RecipeManager)
	{
		LOG_SSR_W(TEXT("RecipeManagerSubsystem is null."));
		return;
	}

	bool bHasBlockedCraftableRecipe = false;
	for (const TPair<FName, int32>& InputPair : InputInventory)
	{
		TArray<FRecipeTable> FoundRecipes;
		if (!RecipeManager->FindRecipesByInputItem(InputPair.Key, FoundRecipes))
		{
			continue;
		}

		for (const FRecipeTable& Recipe : FoundRecipes)
		{
			if (Recipe.MachineType != MachineType || !HasEnoughIngredients(Recipe))
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
		MachineState = EDummyMachineState::Blocked;
		LOG_SSR_W(TEXT("Dummy process blocked by output buffer."));
		return;
	}

	LOG_SSR_W(TEXT("No dummy craftable recipe found."));
}

void ADummyMachineBase::StartProcess()
{
	if (MachineState == EDummyMachineState::Working || bIsBroken)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	MachineState = EDummyMachineState::Working;
	const float EffectiveProcessTime = GetEffectiveProcessTime(ProcessTime);

	LOG_SSR_W(
		TEXT("Dummy process started: %s -> %s in %.2fs"),
		*CurrentRecipe.InputItem1.ToString(),
		*CurrentRecipe.OutputItem1.ToString(),
		EffectiveProcessTime);

	World->GetTimerManager().SetTimer(
		ProcessTimer,
		this,
		&ADummyMachineBase::FinishProcess,
		EffectiveProcessTime,
		false);
}

void ADummyMachineBase::FinishProcess()
{
	if (bIsBroken || MachineState == EDummyMachineState::Disabled)
	{
		return;
	}

	ProcessItem();
	MachineState = EDummyMachineState::Idle;
	TryStartProcess();
}

void ADummyMachineBase::ProcessItem_Implementation()
{
	ConsumeIngredients(CurrentRecipe);
	AddOutputItem(CurrentRecipe.OutputItem1, CurrentRecipe.OutputQty1);
	AddOutputItem(CurrentRecipe.OutputItem2, CurrentRecipe.OutputQty2);

	LOG_SSR_W(
		TEXT("Dummy processed: %s x%d, %s x%d, %s x%d -> %s x%d, %s x%d"),
		*CurrentRecipe.InputItem1.ToString(),
		CurrentRecipe.InputQty1,
		*CurrentRecipe.InputItem2.ToString(),
		CurrentRecipe.InputQty2,
		*CurrentRecipe.InputItem3.ToString(),
		CurrentRecipe.InputQty3,
		*CurrentRecipe.OutputItem1.ToString(),
		CurrentRecipe.OutputQty1,
		*CurrentRecipe.OutputItem2.ToString(),
		CurrentRecipe.OutputQty2);
}

void ADummyMachineBase::StopProcess()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ProcessTimer);
	}

	MachineState = bIsBroken ? EDummyMachineState::Disabled : EDummyMachineState::Idle;
}

void ADummyMachineBase::SetPlanetProductionEfficiency(float NewEfficiency)
{
	PlanetProductionEfficiency = FMath::Max(0.01f, NewEfficiency);
	UpdateDebugBufferText();
}

float ADummyMachineBase::GetEffectiveProcessTime(float BaseProcessTime) const
{
	return FMath::Max(0.01f, BaseProcessTime) / FMath::Max(0.01f, PlanetProductionEfficiency);
}

void ADummyMachineBase::ApplyDurabilityDamage(float Damage)
{
	if (Damage <= 0.0f || Durability <= 0.0f)
	{
		return;
	}

	Durability = FMath::Clamp(Durability - Damage, 0.0f, MaxDurability);
	if (Durability <= 0.0f && !bIsBroken)
	{
		bIsBroken = true;
		StopProcess();
		MachineState = EDummyMachineState::Disabled;
	}

	OnDurabilityChanged.Broadcast(Durability, MaxDurability);
	UpdateDebugBufferText();
}

void ADummyMachineBase::RepairDurability(float Amount)
{
	if (Amount <= 0.0f)
	{
		return;
	}

	const bool bWasBroken = bIsBroken;
	Durability = FMath::Clamp(Durability + Amount, 0.0f, MaxDurability);
	bIsBroken = Durability <= 0.0f;

	if (bWasBroken && !bIsBroken)
	{
		MachineState = EDummyMachineState::Idle;
		TryStartProcess();
	}

	OnDurabilityChanged.Broadcast(Durability, MaxDurability);
	UpdateDebugBufferText();
}
