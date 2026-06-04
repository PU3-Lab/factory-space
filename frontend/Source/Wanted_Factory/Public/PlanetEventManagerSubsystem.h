// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PlanetEventManagerSubsystem.generated.h"

class AMachineBase;

UENUM(BlueprintType)
enum class EPlanetDayPhase : uint8
{
	Day,
	Night
};

UENUM(BlueprintType)
enum class EPlanetEventType : uint8
{
	None,
	MagneticStorm,
	SandStorm
};

USTRUCT(BlueprintType)
struct FPlanetTimeState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet Event|Time")
	int32 DayIndex = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet Event|Time")
	float DaySeconds = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet Event|Time")
	EPlanetDayPhase Phase = EPlanetDayPhase::Day;
};

USTRUCT(BlueprintType)
struct FPlanetWeatherState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet Event|Weather")
	float WindSpeed = 0.2f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet Event|Weather")
	float Rainfall = 0.0f;
};

USTRUCT(BlueprintType)
struct FPlanetEventState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet Event|Event")
	EPlanetEventType Type = EPlanetEventType::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet Event|Event")
	float Severity = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet Event|Event")
	float RemainingSeconds = 0.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlanetDayPhaseChanged, EPlanetDayPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlanetWeatherChanged, const FPlanetWeatherState&, WeatherState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlanetEventStarted, EPlanetEventType, EventType, float, Severity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlanetEventEnded, EPlanetEventType, EventType);

UCLASS()
class WANTED_FACTORY_API UPlanetEventManagerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Time", meta = (ClampMin = "1.0"))
	float SimulationTickSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Time", meta = (ClampMin = "1.0"))
	float DayDurationSeconds = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Time", meta = (ClampMin = "1.0"))
	float NightDurationSeconds = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Weather", meta = (ClampMin = "1.0"))
	float WeatherUpdateIntervalSeconds = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Weather", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxWindDeltaPerUpdate = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Weather", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxRainDeltaPerUpdate = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Event", meta = (ClampMin = "1.0"))
	float EventRollIntervalSeconds = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Event", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MagneticStormChance = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Event", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SandStormBaseChance = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Event", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SandStormWeatherBonusChance = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Event", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SandStormWindThreshold = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Event", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SandStormRainfallMax = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Event", meta = (ClampMin = "1.0"))
	float EventDurationSeconds = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Magnetic Storm", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float MagneticStormLowSeverityEfficiency = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Magnetic Storm", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float MagneticStormHighSeverityEfficiency = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Sand Storm", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SandStormLowSeverityDamageRatio = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Event|Sand Storm", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SandStormHighSeverityDamageRatio = 0.45f;

	UPROPERTY(BlueprintAssignable, Category = "Planet Event|Time")
	FOnPlanetDayPhaseChanged OnDayPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "Planet Event|Weather")
	FOnPlanetWeatherChanged OnWeatherChanged;

	UPROPERTY(BlueprintAssignable, Category = "Planet Event|Event")
	FOnPlanetEventStarted OnPlanetEventStarted;

	UPROPERTY(BlueprintAssignable, Category = "Planet Event|Event")
	FOnPlanetEventEnded OnPlanetEventEnded;

	UFUNCTION(BlueprintCallable, Category = "Planet Event|Machines")
	void RegisterMachine(AMachineBase* Machine);

	UFUNCTION(BlueprintCallable, Category = "Planet Event|Machines")
	void UnregisterMachine(AMachineBase* Machine);

	UFUNCTION(BlueprintPure, Category = "Planet Event|Time")
	FPlanetTimeState GetTimeState() const { return TimeState; }

	UFUNCTION(BlueprintPure, Category = "Planet Event|Weather")
	FPlanetWeatherState GetWeatherState() const { return WeatherState; }

	UFUNCTION(BlueprintPure, Category = "Planet Event|Event")
	FPlanetEventState GetEventState() const { return EventState; }

	UFUNCTION(BlueprintPure, Category = "Planet Event|Time")
	bool IsDay() const { return TimeState.Phase == EPlanetDayPhase::Day; }

	UFUNCTION(BlueprintPure, Category = "Planet Event|Time")
	bool IsNight() const { return TimeState.Phase == EPlanetDayPhase::Night; }

	UFUNCTION(BlueprintPure, Category = "Planet Event|Time")
	float GetDayProgress01() const;

	UFUNCTION(BlueprintCallable, Category = "Planet Event|Weather")
	void RollWeatherTarget();

	UFUNCTION(BlueprintCallable, Category = "Planet Event|Event")
	bool StartPlanetEvent(EPlanetEventType EventType, float Severity, float DurationSeconds);

	UFUNCTION(BlueprintCallable, Category = "Planet Event|Event")
	void EndActiveEvent();

private:
	FPlanetTimeState TimeState;
	FPlanetWeatherState WeatherState;
	FPlanetWeatherState WeatherStartState;
	FPlanetWeatherState WeatherTargetState;
	FPlanetEventState EventState;

	FTimerHandle SimulationTimerHandle;
	FTimerHandle WeatherTimerHandle;
	FTimerHandle EventRollTimerHandle;

	float WeatherBlendElapsedSeconds = 0.0f;
	TArray<TWeakObjectPtr<AMachineBase>> RegisteredMachines;

	void StartSimulation(UWorld& InWorld);
	void StopSimulation();
	void AdvanceSimulation();
	void AdvanceTime(float DeltaSeconds);
	void AdvanceWeather(float DeltaSeconds);
	void RollEvent();
	void ApplyActiveEventToMachine(AMachineBase* Machine) const;
	void ApplyActiveEventToMachines() const;
	void RestoreMachineEfficiencies() const;
	void ForEachRegisteredMachine(TFunctionRef<void(AMachineBase&)> Callback) const;

	float GetFullDaySeconds() const;
	float GetMagneticStormEfficiency() const;
	float GetSandStormDamageRatio() const;
	EPlanetDayPhase ResolvePhase(float DaySeconds) const;
};
