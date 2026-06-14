// Fill out your copyright notice in the Description page of Project Settings.


#include "SolarPowerPlant.h"

#include "Engine/World.h"
#include "PlanetEventManagerSubsystem.h"

ASolarPowerPlant::ASolarPowerPlant()
{
	MachineType = TEXT("SolarPowerPlant");
	BasePowerOutput = 20.0f;
}

bool ASolarPowerPlant::CanGeneratePower() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const UPlanetEventManagerSubsystem* PlanetEventManager = World->GetSubsystem<UPlanetEventManagerSubsystem>();
	if (!PlanetEventManager)
	{
		return false;
	}

	const FPlanetWeatherState WeatherState = PlanetEventManager->GetWeatherState();
	const FPlanetEventState EventState = PlanetEventManager->GetEventState();
	if (!PlanetEventManager->IsDay())
	{
		return false;
	}

	if (WeatherState.Rainfall > 0.0f)
	{
		return false;
	}

	if (EventState.Type == EPlanetEventType::SandStorm)
	{
		return false;
	}

	return Super::CanGeneratePower();
}

float ASolarPowerPlant::CalculatePowerOutput() const
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

	const FPlanetTimeState TimeState = PlanetEventManager->GetTimeState();
	const float DayDurationSeconds = FMath::Max(1.0f, PlanetEventManager->DayDurationSeconds);
	const float DayProgress = FMath::Clamp(TimeState.DaySeconds / DayDurationSeconds, 0.0f, 1.0f);
	const float SunAngleAlpha = FMath::Sin(DayProgress * PI);

	return FMath::Clamp(SunAngleAlpha, 0.0f, 1.0f) * MaxSolarPowerOutput;
}
