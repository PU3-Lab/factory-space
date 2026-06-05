// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PowerLine.generated.h"

class AMachineBase;
class APowerGridNode;

UCLASS()
class WANTED_FACTORY_API APowerLine : public AActor
{
	GENERATED_BODY()

public:
	APowerLine();

	UFUNCTION(BlueprintCallable, Category = "Power Line")
	void ConfigurePowerLine(AMachineBase* NewSourceMachine, AMachineBase* NewTargetMachine);

	UFUNCTION(BlueprintCallable, Category = "Power Line")
	void UpdateLineVisual();

	static FVector GetEndpointLocationForActor(const AActor* Actor, float AdditionalHeightOffset);

	UFUNCTION(BlueprintPure, Category = "Power Line")
	AMachineBase* GetSourceMachine() const { return SourceMachine.Get(); }

	UFUNCTION(BlueprintPure, Category = "Power Line")
	AMachineBase* GetTargetMachine() const { return TargetMachine.Get(); }

	UFUNCTION(BlueprintPure, Category = "Power Line")
	APowerGridNode* GetSourceNode() const;

	UFUNCTION(BlueprintPure, Category = "Power Line")
	APowerGridNode* GetTargetNode() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Power Line")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Power Line")
	TObjectPtr<UStaticMeshComponent> LineMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Power Line", meta = (ClampMin = "0.0"))
	float EndpointHeightOffset = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Power Line", meta = (ClampMin = "0.1"))
	float LineThickness = 8.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Power Line")
	TWeakObjectPtr<AMachineBase> SourceMachine;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Power Line")
	TWeakObjectPtr<AMachineBase> TargetMachine;

private:
	void RegisterToFactoryManager();
	void UnregisterFromFactoryManager();
};
