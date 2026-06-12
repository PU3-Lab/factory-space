// Fill out your copyright notice in the Description page of Project Settings.


#include "WindPowerPlant.h"

#include "PlanetEventManagerSubsystem.h"
#include "Engine/World.h"

AWindPowerPlant::AWindPowerPlant()
{
	MachineType = TEXT("WindPowerPlant");
	BasePowerOutput = 20.0f;
}

float AWindPowerPlant::CalculatePowerOutput() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	const UPlanetEventManagerSubsystem* PlanetEventManager = World->GetSubsystem<UPlanetEventManagerSubsystem>();
	if (!PlanetEventManager)
	{
		return 0.0f;
	}

	const float WindSpeed = PlanetEventManager->GetWeatherState().WindSpeed;
	const float ClampedMaxWindSpeed = FMath::Max(MaxWindSpeed, MinWindSpeed);
	if (FMath::IsNearlyEqual(MinWindSpeed, ClampedMaxWindSpeed))
	{
		return WindSpeed >= ClampedMaxWindSpeed ? MaxWindPowerOutput : 0.0f;
	}

	const float WindAlpha = FMath::GetMappedRangeValueClamped(
		FVector2D(MinWindSpeed, ClampedMaxWindSpeed),
		FVector2D(0.0f, 1.0f),
		WindSpeed);

	return FMath::Lerp(0.0f, MaxWindPowerOutput, WindAlpha);
}
