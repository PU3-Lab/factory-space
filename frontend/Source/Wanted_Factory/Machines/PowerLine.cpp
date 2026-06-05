// Fill out your copyright notice in the Description page of Project Settings.

#include "PowerLine.h"

#include "Components/StaticMeshComponent.h"
#include "FactoryManagerSubsystem.h"
#include "MachineBase.h"
#include "Machines/PowerGridNode.h"
#include "UObject/ConstructorHelpers.h"

APowerLine::APowerLine()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	LineMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LineMesh"));
	LineMesh->SetupAttachment(Root);
	LineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		LineMesh->SetStaticMesh(CubeMesh.Object);
	}
}

void APowerLine::BeginPlay()
{
	Super::BeginPlay();
	RegisterToFactoryManager();
	UpdateLineVisual();
}

void APowerLine::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromFactoryManager();
	Super::EndPlay(EndPlayReason);
}

void APowerLine::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateLineVisual();
}

void APowerLine::ConfigurePowerLine(AMachineBase* NewSourceMachine, AMachineBase* NewTargetMachine)
{
	SourceMachine = NewSourceMachine;
	TargetMachine = NewTargetMachine;
	UpdateLineVisual();
	RegisterToFactoryManager();
}

APowerGridNode* APowerLine::GetSourceNode() const
{
	return Cast<APowerGridNode>(SourceMachine.Get());
}

APowerGridNode* APowerLine::GetTargetNode() const
{
	return Cast<APowerGridNode>(TargetMachine.Get());
}

FVector APowerLine::GetEndpointLocationForActor(const AActor* Actor, float AdditionalHeightOffset)
{
	if (!Actor)
	{
		return FVector::ZeroVector;
	}

	FVector BoundsOrigin = Actor->GetActorLocation();
	FVector BoundsExtent = FVector::ZeroVector;
	Actor->GetActorBounds(true, BoundsOrigin, BoundsExtent);
	if (BoundsExtent.IsNearlyZero())
	{
		Actor->GetActorBounds(false, BoundsOrigin, BoundsExtent);
	}

	return FVector(
		BoundsOrigin.X,
		BoundsOrigin.Y,
		BoundsOrigin.Z + BoundsExtent.Z + AdditionalHeightOffset);
}

void APowerLine::UpdateLineVisual()
{
	AMachineBase* Source = SourceMachine.Get();
	AMachineBase* Target = TargetMachine.Get();
	if (!LineMesh || !Source || !Target || Source == Target)
	{
		if (LineMesh)
		{
			LineMesh->SetVisibility(false);
		}
		return;
	}

	const FVector SourceLocation = GetEndpointLocationForActor(Source, EndpointHeightOffset);
	const FVector TargetLocation = GetEndpointLocationForActor(Target, EndpointHeightOffset);
	const FVector Delta = TargetLocation - SourceLocation;
	const float Length = Delta.Size();
	if (Length <= UE_KINDA_SMALL_NUMBER)
	{
		LineMesh->SetVisibility(false);
		return;
	}

	SetActorLocation(SourceLocation + (Delta * 0.5f));
	SetActorRotation(Delta.Rotation());

	LineMesh->SetVisibility(true);
	LineMesh->SetRelativeLocation(FVector::ZeroVector);
	LineMesh->SetRelativeRotation(FRotator::ZeroRotator);
	LineMesh->SetRelativeScale3D(FVector(Length / 100.0f, LineThickness / 100.0f, LineThickness / 100.0f));
}

void APowerLine::RegisterToFactoryManager()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFactoryManagerSubsystem* FactoryManager = GameInstance->GetSubsystem<UFactoryManagerSubsystem>())
		{
			FactoryManager->RegisterPowerLine(this);
		}
	}
}

void APowerLine::UnregisterFromFactoryManager()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFactoryManagerSubsystem* FactoryManager = GameInstance->GetSubsystem<UFactoryManagerSubsystem>())
		{
			FactoryManager->UnregisterPowerLine(this);
		}
	}
}
