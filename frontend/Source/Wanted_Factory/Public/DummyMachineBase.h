// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Recipe/RecipeTable.h"
#include "DummyMachineBase.generated.h"

class ADummyMachineBase;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class EDummyMachineState : uint8
{
	Idle,
	Working,
	NoPower,
	Blocked,
	Disabled
};

UENUM(BlueprintType)
enum class EDummyPortType : uint8
{
	Input,
	Output
};

USTRUCT(BlueprintType)
struct FDummyMachinePortData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dummy Machine|Ports")
	int32 PortIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dummy Machine|Ports")
	EDummyPortType PortType = EDummyPortType::Input;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dummy Machine|Ports")
	FName AcceptedItemID;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dummy Machine|Ports")
	bool bIsConnected = false;
};

USTRUCT(BlueprintType)
struct FDummyMachinePortConnection
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dummy Machine|Ports")
	int32 FromOutputPortIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dummy Machine|Ports")
	ADummyMachineBase* TargetMachine = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dummy Machine|Ports")
	int32 TargetInputPortIndex = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDummyMachineDurabilityChanged, float, Durability, float, MaxDurability);

UCLASS(Blueprintable)
class WANTED_FACTORY_API ADummyMachineBase : public AActor
{
	GENERATED_BODY()

public:
	ADummyMachineBase();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy Machine|Grid", meta = (ClampMin = "1"))
	FIntPoint GridSize = FIntPoint(1, 1);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dummy Machine|Grid")
	FIntPoint GridPosision;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy Machine|Ports", meta = (ClampMin = "0"))
	int32 InputPortCount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy Machine|Ports", meta = (ClampMin = "0"))
	int32 OutputPortCount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy Machine|Settings", meta = (ClampMin = "0.01"))
	float ProcessTime = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy Machine|Settings")
	FName MachineType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy Machine|Settings")
	bool bNeedPower = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy Machine|Planet", meta = (ClampMin = "0.01"))
	float PlanetProductionEfficiency = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy Machine|Durability", meta = (ClampMin = "1.0"))
	float MaxDurability = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dummy Machine|Durability")
	float Durability = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dummy Machine|Durability")
	bool bIsBroken = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dummy Machine|Components")
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dummy Machine|Components")
	UStaticMeshComponent* MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dummy Machine|Debug")
	UTextRenderComponent* DebugBufferText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dummy Machine|State")
	EDummyMachineState MachineState = EDummyMachineState::Idle;

	FTimerHandle ProcessTimer;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dummy Machine|Inventory")
	TMap<FName, int32> InputInventory;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dummy Machine|Inventory", meta = (ClampMin = "1"))
	int32 MaxInputPerItem = 50;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dummy Machine|Inventory")
	TMap<FName, int32> OutputInventory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy Machine|Debug")
	bool bShowDebugBufferText = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy Machine|Debug")
	FVector DebugTextOffset = FVector(0.0f, 0.0f, 180.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy Machine|Debug", meta = (ClampMin = "1.0"))
	float DebugTextWorldSize = 24.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dummy Machine|Inventory")
	FRecipeTable CurrentRecipe;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dummy Machine|Inventory")
	TMap<FName, int32> OutputBuffer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dummy Machine|Inventory")
	int32 MaxBufferPerItem = 50;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dummy Machine|Ports")
	TArray<FDummyMachinePortData> InputPorts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dummy Machine|Ports")
	TArray<FDummyMachinePortData> OutputPorts;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dummy Machine|Ports")
	TArray<FDummyMachinePortConnection> OutputConnections;

public:
	UPROPERTY(BlueprintAssignable, Category = "Dummy Machine|Durability")
	FOnDummyMachineDurabilityChanged OnDurabilityChanged;

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintPure, Category = "Dummy Machine|Settings")
	FVector2D GetMachineSize() const { return FVector2D(GridSize.X, GridSize.Y); }

	UFUNCTION(BlueprintPure, Category = "Dummy Machine|Settings")
	int32 GetInputPortCount() const { return InputPortCount; }

	UFUNCTION(BlueprintPure, Category = "Dummy Machine|Settings")
	int32 GetOutputPortCount() const { return OutputPortCount; }

	UFUNCTION(BlueprintCallable, Category = "Dummy Machine|Placement")
	virtual bool CanPlace();

	UFUNCTION(BlueprintCallable, Category = "Dummy Machine|Inventory")
	virtual bool AddItem(FName ItemID, int32 Count);

	UFUNCTION(BlueprintCallable, Category = "Dummy Machine|Process")
	virtual void TryStartProcess();

	UFUNCTION(BlueprintCallable, Category = "Dummy Machine|Process")
	virtual void StartProcess();

	UFUNCTION(BlueprintCallable, Category = "Dummy Machine|Process")
	virtual void FinishProcess();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Dummy Machine|Process")
	void ProcessItem();

	virtual void ProcessItem_Implementation();

	UFUNCTION(BlueprintCallable, Category = "Dummy Machine|Process")
	virtual void StopProcess();

	UFUNCTION(BlueprintCallable, Category = "Dummy Machine|Planet")
	void SetPlanetProductionEfficiency(float NewEfficiency);

	UFUNCTION(BlueprintPure, Category = "Dummy Machine|Planet")
	float GetPlanetProductionEfficiency() const { return PlanetProductionEfficiency; }

	UFUNCTION(BlueprintPure, Category = "Dummy Machine|Planet")
	float GetEffectiveProcessTime(float BaseProcessTime) const;

	UFUNCTION(BlueprintCallable, Category = "Dummy Machine|Durability")
	void ApplyDurabilityDamage(float Damage);

	UFUNCTION(BlueprintCallable, Category = "Dummy Machine|Durability")
	void RepairDurability(float Amount);

	UFUNCTION(BlueprintPure, Category = "Dummy Machine|Durability")
	float GetDurability() const { return Durability; }

	UFUNCTION(BlueprintPure, Category = "Dummy Machine|Durability")
	float GetMaxDurability() const { return MaxDurability; }

	UFUNCTION(BlueprintPure, Category = "Dummy Machine|Durability")
	bool IsBroken() const { return bIsBroken; }

	UFUNCTION(BlueprintCallable, Category = "Dummy Machine|Inventory")
	bool CanAddInputItem(FName ItemID, int32 Count) const;

	bool HasEnoughIngredients(const FRecipeTable& Recipe) const;
	void ConsumeIngredients(const FRecipeTable& Recipe);
	virtual void AddOutputItem(FName ItemID, int32 Count);
	virtual bool CanAddToOutputBuffer(const FRecipeTable& Recipe) const;

	UFUNCTION(BlueprintCallable, Category = "Dummy Machine|Buffer")
	bool TakeOutputItem(FName ItemID, int32 Count);

	UFUNCTION(BlueprintPure, Category = "Dummy Machine|Conveyor")
	bool PeekFirstOutputItem(FName& OutItemID) const;

	UFUNCTION(BlueprintCallable, Category = "Dummy Machine|Conveyor")
	bool TryTakeFirstOutputItem(FName& OutItemID);

	UFUNCTION(BlueprintPure, Category = "Dummy Machine|Conveyor")
	virtual bool CanReceiveConveyorItem(FName ItemID, int32 Count = 1) const;

	UFUNCTION(BlueprintCallable, Category = "Dummy Machine|Conveyor")
	virtual bool ReceiveConveyorItem(FName ItemID, int32 Count = 1);

	UFUNCTION(BlueprintCallable, Category = "Dummy Machine|Debug")
	void UpdateDebugBufferText();

	UFUNCTION(BlueprintCallable, Category = "Dummy Machine|Transfer")
	bool TransferOutputToMachine(ADummyMachineBase* TargetMachine, FName ItemID, int32 Count);

	UFUNCTION(BlueprintCallable, Category = "Dummy Machine|Ports")
	bool ConnectOutputToMachine(int32 OutputPortIndex, ADummyMachineBase* TargetMachine, int32 TargetInputPortIndex);

	UFUNCTION(BlueprintCallable, Category = "Dummy Machine|Ports")
	bool TransferOutputByPort(int32 OutputPortIndex, FName ItemID, int32 Count);

	UFUNCTION(BlueprintCallable, Category = "Dummy Machine|Debug")
	void DebugInventory();
};
