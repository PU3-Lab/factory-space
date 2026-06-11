// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PowerLine.generated.h"

class AMachineBase;
class APowerGridNode;
class UMaterialInstanceDynamic;
class UMaterialInterface;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Power Line")
	TArray<TObjectPtr<UStaticMeshComponent>> LineSegments;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Power Line", meta = (ClampMin = "0.0"))
	float EndpointHeightOffset = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Power Line", meta = (ClampMin = "0.1"))
	float LineThickness = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Power Line|Material")
	TObjectPtr<UMaterialInterface> LineMaterialBase;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Power Line|Material")
	TObjectPtr<UMaterialInterface> ConnectedLineMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Power Line|Material")
	TObjectPtr<UMaterialInterface> DisconnectedLineMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Power Line|Material")
	FLinearColor ConnectedLineColor = FLinearColor(1.0f, 0.72f, 0.05f, 0.45f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Power Line|Material")
	FLinearColor DisconnectedLineColor = FLinearColor::Black;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Power Line|Material", meta = (ClampMin = "0.0"))
	float ConnectedEmissiveStrength = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Power Line|Sag", meta = (ClampMin = "0.0"))
	float SagRatio = 0.035f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Power Line|Sag", meta = (ClampMin = "0.0"))
	float MaxSagDepth = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Power Line|Sag", meta = (ClampMin = "1"))
	int32 MinSagSegments = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Power Line|Sag", meta = (ClampMin = "1"))
	int32 MaxSagSegments = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Power Line|Sag", meta = (ClampMin = "1.0"))
	float SegmentTargetLength = 150.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Power Line")
	TWeakObjectPtr<AMachineBase> SourceMachine;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Power Line")
	TWeakObjectPtr<AMachineBase> TargetMachine;

private:
	bool IsElectricallyConnected() const;
	FVector GetSagPoint(const FVector& SourceLocation, const FVector& TargetLocation, float Alpha, float SagDepth) const;
	UStaticMeshComponent* GetOrCreateLineSegment(int32 SegmentIndex);
	void HideUnusedLineSegments(int32 UsedSegmentCount);
	void UpdateLineSegment(UStaticMeshComponent* Segment, const FVector& StartLocation, const FVector& EndLocation);
	UMaterialInterface* GetPowerLineMaterial(bool bElectricallyConnected);
	void ConfigureMaterialInstance(UMaterialInstanceDynamic* MaterialInstance, bool bElectricallyConnected) const;
	void ApplyMaterialToSegment(UStaticMeshComponent* Segment, bool bElectricallyConnected);
	void RegisterToFactoryManager();
	void UnregisterFromFactoryManager();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ConnectedMaterialInstance;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DisconnectedMaterialInstance;
};
