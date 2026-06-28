// Fill out your copyright notice in the Description page of Project Settings.


#include "PowerGridNode.h"

#include "Components/PointLightComponent.h"
#include "FactoryManagerSubsystem.h"
#include "PlanetEventManagerSubsystem.h"
#include "Wanted_Factory.h"

APowerGridNode::APowerGridNode()
{
	PrimaryActorTick.bCanEverTick = true;

	MachineType = TEXT("PowerGridNode");
	bNeedPower = false;
	bDisableWhenBroken = true;
	bInfiniteDurability = true;
	MeshScaleMultiplier = FVector(1.0f, 1.0f, 3.0f);

	NightIndicatorLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("NightIndicatorLight"));
	NightIndicatorLight->SetupAttachment(RootComponent);
	NightIndicatorLight->SetRelativeLocation(NightIndicatorLightOffset);
	NightIndicatorLight->SetLightColor(NightIndicatorLightColor);
	NightIndicatorLight->SetIntensity(NightIndicatorLightIntensity);
	NightIndicatorLight->SetAttenuationRadius(NightIndicatorLightRadius);
	NightIndicatorLight->SetCastShadows(true);
	NightIndicatorLight->SetVisibility(false);
}

void APowerGridNode::ApplyMachineData(const FMachineTableRow& MachineData)
{
	Super::ApplyMachineData(MachineData);

	PowerConsumption = 0.f;
	bNeedPower = false;
}

void APowerGridNode::BeginPlay()
{
	Super::BeginPlay();
	if (NightIndicatorLight)
	{
		NightIndicatorLight->SetRelativeLocation(NightIndicatorLightOffset);
		NightIndicatorLight->SetLightColor(NightIndicatorLightColor);
		NightIndicatorLight->SetIntensity(NightIndicatorLightIntensity);
		NightIndicatorLight->SetAttenuationRadius(NightIndicatorLightRadius);
	}
	RegisterToPowerGrid();
	UpdateNightIndicatorLight();
}

void APowerGridNode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (NightIndicatorLight)
	{
		NightIndicatorLight->SetVisibility(false);
	}
	UnregisterFromPowerGrid();
	Super::EndPlay(EndPlayReason);
}

void APowerGridNode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateNightIndicatorLight();
}

bool APowerGridNode::AddItem(FName ItemID, int32 Count)
{
	LOG_SSR_W(TEXT("PowerGridNode has no input port and cannot receive item: %s"), *ItemID.ToString());
	return false;
}

bool APowerGridNode::CanReceiveConveyorItem(FName ItemID, int32 Count) const
{
	return false;
}

bool APowerGridNode::IsPowerGridActive() const
{
	return !(isBroken() && bDisableWhenBroken);
}

void APowerGridNode::UpdateNightIndicatorLight()
{
	if (!NightIndicatorLight)
	{
		return;
	}

	NightIndicatorLight->SetRelativeLocation(NightIndicatorLightOffset);
	NightIndicatorLight->SetLightColor(NightIndicatorLightColor);
	NightIndicatorLight->SetIntensity(NightIndicatorLightIntensity);
	NightIndicatorLight->SetAttenuationRadius(NightIndicatorLightRadius);

	const bool bShouldBeVisible = ShouldEnableNightIndicatorLight();
	if (NightIndicatorLight->IsVisible() != bShouldBeVisible)
	{
		NightIndicatorLight->SetVisibility(bShouldBeVisible);
	}
}

bool APowerGridNode::ShouldEnableNightIndicatorLight() const
{
	if (!IsPowerGridActive())
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const UPlanetEventManagerSubsystem* EventManager = World->GetSubsystem<UPlanetEventManagerSubsystem>();
	if (!EventManager)
	{
		return false;
	}

	const int32 CurrentHour24 = EventManager->GetCurrentHour24();
	const bool bNight = CurrentHour24 >= NightLightStartHour24 || CurrentHour24 < NightLightEndHour24;
	if (!bNight)
	{
		return false;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return false;
	}

	const UFactoryManagerSubsystem* FactoryManager = GameInstance->GetSubsystem<UFactoryManagerSubsystem>();
	return FactoryManager && FactoryManager->IsPowerGridNodeEnergized(this);
}

void APowerGridNode::RegisterToPowerGrid()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFactoryManagerSubsystem* FactoryManager = GameInstance->GetSubsystem<UFactoryManagerSubsystem>())
		{
			FactoryManager->RegisterPowerGridNode(this);
		}
	}
}

void APowerGridNode::UnregisterFromPowerGrid()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFactoryManagerSubsystem* FactoryManager = GameInstance->GetSubsystem<UFactoryManagerSubsystem>())
		{
			FactoryManager->UnregisterPowerGridNode(this);
		}
	}
}
