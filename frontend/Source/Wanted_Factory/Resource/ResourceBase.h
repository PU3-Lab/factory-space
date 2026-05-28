// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ResourceBase.generated.h"

UENUM(BlueprintType)
enum class EResourceType : uint8
{
	None,
	Electricity,
	Fire,
	Water,
	Ore,
	Plant
};

UCLASS()
class WANTED_FACTORY_API AResourceBase : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AResourceBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UStaticMeshComponent* Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
	EResourceType ResourceType = EResourceType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
	FName ResourceID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
	int32 Amount = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
	int32 MaxAmount = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
	bool bIsInfinite = false;

public:
	UFUNCTION(BlueprintCallable)
	virtual bool ConsumeResource(int32 ConsumeAmount);

	UFUNCTION(BlueprintCallable)
	virtual void AddResource(int32 AddAmount);

	UFUNCTION(BlueprintCallable)
	bool IsEmpty() const;

};
