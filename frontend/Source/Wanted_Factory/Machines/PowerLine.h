// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PowerLine.generated.h"

class APowerGridNode;

UCLASS()
class WANTED_FACTORY_API APowerLine : public AActor
{
	GENERATED_BODY()

public:
	APowerLine();

	UFUNCTION(BlueprintCallable, Category = "Power Line")
	void ConfigurePowerLine(APowerGridNode* NewSourceNode, APowerGridNode* NewTargetNode);

	UFUNCTION(BlueprintCallable, Category = "Power Line")
	void UpdateLineVisual();

	UFUNCTION(BlueprintPure, Category = "Power Line")
	APowerGridNode* GetSourceNode() const { return SourceNode.Get(); }

	UFUNCTION(BlueprintPure, Category = "Power Line")
	APowerGridNode* GetTargetNode() const { return TargetNode.Get(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Power Line")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Power Line")
	TObjectPtr<UStaticMeshComponent> LineMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Power Line", meta = (ClampMin = "0.0"))
	float LineHeightOffset = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Power Line", meta = (ClampMin = "0.1"))
	float LineThickness = 8.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Power Line")
	TWeakObjectPtr<APowerGridNode> SourceNode;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Power Line")
	TWeakObjectPtr<APowerGridNode> TargetNode;

private:
	void RegisterToFactoryManager();
	void UnregisterFromFactoryManager();
};
