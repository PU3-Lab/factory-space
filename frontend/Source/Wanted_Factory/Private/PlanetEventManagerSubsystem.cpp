#include "PlanetEventManagerSubsystem.h"

#include "MachineBase.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Wanted_Factory.h"

void UPlanetEventManagerSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (!InWorld.IsGameWorld())
	{
		return;
	}

	StartSimulation(InWorld);
}

void UPlanetEventManagerSubsystem::Deinitialize()
{
	StopSimulation();
	RegisteredMachines.Reset();
	Super::Deinitialize();
}

void UPlanetEventManagerSubsystem::RegisterMachine(AMachineBase* Machine)
{
	if (!Machine)
	{
		return;
	}

	RegisteredMachines.RemoveAllSwap(
		[Machine](const TWeakObjectPtr<AMachineBase>& RegisteredMachine)
		{
			return !RegisteredMachine.IsValid() || RegisteredMachine.Get() == Machine;
		});
	RegisteredMachines.Add(Machine);

	if (EventState.Type == EPlanetEventType::MagneticStorm)
	{
		Machine->SetPlanetProductionEfficiency(GetMagneticStormEfficiency());
	}
}

void UPlanetEventManagerSubsystem::UnregisterMachine(AMachineBase* Machine)
{
	if (!Machine)
	{
		return;
	}

	RegisteredMachines.RemoveAllSwap(
		[Machine](const TWeakObjectPtr<AMachineBase>& RegisteredMachine)
		{
			return !RegisteredMachine.IsValid() || RegisteredMachine.Get() == Machine;
		});
}

float UPlanetEventManagerSubsystem::GetDayProgress01() const
{
	const float FullDaySeconds = GetFullDaySeconds();
	if (FullDaySeconds <= 0.0f)
	{
		return 0.0f;
	}

	return FMath::Clamp(TimeState.DaySeconds / FullDaySeconds, 0.0f, 1.0f);
}

void UPlanetEventManagerSubsystem::RollWeatherTarget()
{
	WeatherStartState = WeatherState;
	WeatherTargetState.WindSpeed = FMath::Clamp(
		WeatherState.WindSpeed + FMath::FRandRange(-MaxWindDeltaPerUpdate, MaxWindDeltaPerUpdate),
		0.0f,
		1.0f);
	WeatherTargetState.Rainfall = FMath::Clamp(
		WeatherState.Rainfall + FMath::FRandRange(-MaxRainDeltaPerUpdate, MaxRainDeltaPerUpdate),
		0.0f,
		1.0f);
	WeatherBlendElapsedSeconds = 0.0f;

	OnWeatherChanged.Broadcast(WeatherState);
}

bool UPlanetEventManagerSubsystem::StartPlanetEvent(
	EPlanetEventType EventType,
	float Severity,
	float DurationSeconds)
{
	if (EventType == EPlanetEventType::None || EventState.Type != EPlanetEventType::None)
	{
		return false;
	}

	EventState.Type = EventType;
	EventState.Severity = FMath::Clamp(Severity, 0.0f, 1.0f);
	EventState.RemainingSeconds = DurationSeconds > 0.0f ? DurationSeconds : EventDurationSeconds;

	ApplyActiveEventToMachines();
	OnPlanetEventStarted.Broadcast(EventState.Type, EventState.Severity);

	LOG_SSR_W(
		TEXT("Planet event started: %s Severity=%.2f Duration=%.2f"),
		*UEnum::GetValueAsString(EventState.Type),
		EventState.Severity,
		EventState.RemainingSeconds);

	return true;
}

void UPlanetEventManagerSubsystem::EndActiveEvent()
{
	if (EventState.Type == EPlanetEventType::None)
	{
		return;
	}

	const EPlanetEventType EndedEventType = EventState.Type;
	if (EndedEventType == EPlanetEventType::MagneticStorm)
	{
		RestoreMachineEfficiencies();
	}

	EventState = FPlanetEventState();
	OnPlanetEventEnded.Broadcast(EndedEventType);

	LOG_SSR_W(TEXT("Planet event ended: %s"), *UEnum::GetValueAsString(EndedEventType));
}

void UPlanetEventManagerSubsystem::StartSimulation(UWorld& InWorld)
{
	StopSimulation();

	TimeState = FPlanetTimeState();
	WeatherState = FPlanetWeatherState();
	WeatherStartState = WeatherState;
	WeatherTargetState = WeatherState;
	EventState = FPlanetEventState();
	WeatherBlendElapsedSeconds = 0.0f;

	InWorld.GetTimerManager().SetTimer(
		SimulationTimerHandle,
		this,
		&UPlanetEventManagerSubsystem::AdvanceSimulation,
		FMath::Max(0.01f, SimulationTickSeconds),
		true);

	InWorld.GetTimerManager().SetTimer(
		WeatherTimerHandle,
		this,
		&UPlanetEventManagerSubsystem::RollWeatherTarget,
		FMath::Max(1.0f, WeatherUpdateIntervalSeconds),
		true);

	InWorld.GetTimerManager().SetTimer(
		EventRollTimerHandle,
		this,
		&UPlanetEventManagerSubsystem::RollEvent,
		FMath::Max(1.0f, EventRollIntervalSeconds),
		true);

	RollWeatherTarget();
}

void UPlanetEventManagerSubsystem::StopSimulation()
{
	if (EventState.Type == EPlanetEventType::MagneticStorm)
	{
		RestoreMachineEfficiencies();
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SimulationTimerHandle);
		World->GetTimerManager().ClearTimer(WeatherTimerHandle);
		World->GetTimerManager().ClearTimer(EventRollTimerHandle);
	}
}

void UPlanetEventManagerSubsystem::AdvanceSimulation()
{
	const float DeltaSeconds = FMath::Max(0.01f, SimulationTickSeconds);

	AdvanceTime(DeltaSeconds);
	AdvanceWeather(DeltaSeconds);

	if (EventState.Type != EPlanetEventType::None)
	{
		EventState.RemainingSeconds -= DeltaSeconds;
		if (EventState.RemainingSeconds <= 0.0f)
		{
			EndActiveEvent();
		}
	}
}

void UPlanetEventManagerSubsystem::AdvanceTime(float DeltaSeconds)
{
	const EPlanetDayPhase OldPhase = TimeState.Phase;
	const float FullDaySeconds = GetFullDaySeconds();
	TimeState.DaySeconds += DeltaSeconds;

	while (TimeState.DaySeconds >= FullDaySeconds)
	{
		TimeState.DaySeconds -= FullDaySeconds;
		TimeState.DayIndex++;
	}

	TimeState.Phase = ResolvePhase(TimeState.DaySeconds);
	if (TimeState.Phase != OldPhase)
	{
		OnDayPhaseChanged.Broadcast(TimeState.Phase);
	}
}

void UPlanetEventManagerSubsystem::AdvanceWeather(float DeltaSeconds)
{
	WeatherBlendElapsedSeconds += DeltaSeconds;

	const float Alpha = WeatherUpdateIntervalSeconds <= 0.0f
		? 1.0f
		: FMath::Clamp(WeatherBlendElapsedSeconds / WeatherUpdateIntervalSeconds, 0.0f, 1.0f);

	const FPlanetWeatherState PreviousWeatherState = WeatherState;
	WeatherState.WindSpeed = FMath::Lerp(WeatherStartState.WindSpeed, WeatherTargetState.WindSpeed, Alpha);
	WeatherState.Rainfall = FMath::Lerp(WeatherStartState.Rainfall, WeatherTargetState.Rainfall, Alpha);

	if (!FMath::IsNearlyEqual(PreviousWeatherState.WindSpeed, WeatherState.WindSpeed) ||
		!FMath::IsNearlyEqual(PreviousWeatherState.Rainfall, WeatherState.Rainfall))
	{
		OnWeatherChanged.Broadcast(WeatherState);
	}
}

void UPlanetEventManagerSubsystem::RollEvent()
{
	if (EventState.Type != EPlanetEventType::None)
	{
		return;
	}

	if (FMath::FRand() <= MagneticStormChance)
	{
		StartPlanetEvent(EPlanetEventType::MagneticStorm, FMath::FRandRange(0.5f, 1.0f), EventDurationSeconds);
		return;
	}

	float SandStormChance = SandStormBaseChance;
	if (WeatherState.WindSpeed >= SandStormWindThreshold && WeatherState.Rainfall <= SandStormRainfallMax)
	{
		SandStormChance += SandStormWeatherBonusChance;
	}

	if (FMath::FRand() <= SandStormChance)
	{
		StartPlanetEvent(EPlanetEventType::SandStorm, FMath::FRandRange(0.5f, 1.0f), EventDurationSeconds);
	}
}

void UPlanetEventManagerSubsystem::ApplyActiveEventToMachine(AMachineBase* Machine) const
{
	if (!Machine)
	{
		return;
	}

	if (EventState.Type == EPlanetEventType::MagneticStorm)
	{
		Machine->SetPlanetProductionEfficiency(GetMagneticStormEfficiency());
		return;
	}

	if (EventState.Type == EPlanetEventType::SandStorm)
	{
		Machine->ApplyDurabilityDamage(Machine->GetMaxDurability() * GetSandStormDamageRatio());
	}
}

void UPlanetEventManagerSubsystem::ApplyActiveEventToMachines() const
{
	ForEachRegisteredMachine(
		[this](AMachineBase& Machine)
		{
			ApplyActiveEventToMachine(&Machine);
		});
}

void UPlanetEventManagerSubsystem::RestoreMachineEfficiencies() const
{
	ForEachRegisteredMachine(
		[](AMachineBase& Machine)
		{
			Machine.SetPlanetProductionEfficiency(1.0f);
		});
}

void UPlanetEventManagerSubsystem::ForEachRegisteredMachine(
	TFunctionRef<void(AMachineBase&)> Callback) const
{
	for (const TWeakObjectPtr<AMachineBase>& RegisteredMachine : RegisteredMachines)
	{
		if (AMachineBase* Machine = RegisteredMachine.Get())
		{
			Callback(*Machine);
		}
	}
}

float UPlanetEventManagerSubsystem::GetFullDaySeconds() const
{
	return FMath::Max(1.0f, DayDurationSeconds + NightDurationSeconds);
}

float UPlanetEventManagerSubsystem::GetMagneticStormEfficiency() const
{
	return FMath::Lerp(
		MagneticStormLowSeverityEfficiency,
		MagneticStormHighSeverityEfficiency,
		FMath::Clamp(EventState.Severity, 0.0f, 1.0f));
}

float UPlanetEventManagerSubsystem::GetSandStormDamageRatio() const
{
	return FMath::Lerp(
		SandStormLowSeverityDamageRatio,
		SandStormHighSeverityDamageRatio,
		FMath::Clamp(EventState.Severity, 0.0f, 1.0f));
}

EPlanetDayPhase UPlanetEventManagerSubsystem::ResolvePhase(float DaySeconds) const
{
	return DaySeconds < DayDurationSeconds ? EPlanetDayPhase::Day : EPlanetDayPhase::Night;
}
