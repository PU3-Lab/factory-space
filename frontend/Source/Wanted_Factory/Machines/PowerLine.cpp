// Fill out your copyright notice in the Description page of Project Settings.

#include "PowerLine.h"

#include "Components/StaticMeshComponent.h"
#include "FactoryManagerSubsystem.h"
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

void APowerLine::ConfigurePowerLine(APowerGridNode* NewSourceNode, APowerGridNode* NewTargetNode)
{
	SourceNode = NewSourceNode;
	TargetNode = NewTargetNode;
	UpdateLineVisual();
	RegisterToFactoryManager();
}

void APowerLine::UpdateLineVisual()
{
	APowerGridNode* Source = SourceNode.Get();
	APowerGridNode* Target = TargetNode.Get();
	if (!LineMesh || !Source || !Target || Source == Target)
	{
		if (LineMesh)
		{
			LineMesh->SetVisibility(false);
		}
		return;
	}

	const FVector SourceLocation = Source->GetActorLocation() + FVector(0.0f, 0.0f, LineHeightOffset);
	const FVector TargetLocation = Target->GetActorLocation() + FVector(0.0f, 0.0f, LineHeightOffset);
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
