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
	LineSegments.Add(LineMesh);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		LineMesh->SetStaticMesh(CylinderMesh.Object);
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
		HideUnusedLineSegments(0);
		return;
	}

	SetActorLocation(SourceLocation + (Delta * 0.5f));
	SetActorRotation(FRotator::ZeroRotator);

	const float HorizontalDistance = FVector::Dist2D(SourceLocation, TargetLocation);
	const float SagDepth = FMath::Clamp(HorizontalDistance * SagRatio, 0.0f, MaxSagDepth);
	const int32 SegmentCount = FMath::Clamp(
		FMath::CeilToInt(Length / FMath::Max(SegmentTargetLength, 1.0f)),
		MinSagSegments,
		MaxSagSegments);

	for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
	{
		const float StartAlpha = static_cast<float>(SegmentIndex) / static_cast<float>(SegmentCount);
		const float EndAlpha = static_cast<float>(SegmentIndex + 1) / static_cast<float>(SegmentCount);
		UStaticMeshComponent* Segment = GetOrCreateLineSegment(SegmentIndex);
		UpdateLineSegment(
			Segment,
			GetSagPoint(SourceLocation, TargetLocation, StartAlpha, SagDepth),
			GetSagPoint(SourceLocation, TargetLocation, EndAlpha, SagDepth));
	}

	HideUnusedLineSegments(SegmentCount);
}

FVector APowerLine::GetSagPoint(
	const FVector& SourceLocation,
	const FVector& TargetLocation,
	float Alpha,
	float SagDepth) const
{
	const FVector Point = FMath::Lerp(SourceLocation, TargetLocation, Alpha);
	const float SagAlpha = 4.0f * Alpha * (1.0f - Alpha);
	return Point - FVector(0.0f, 0.0f, SagDepth * SagAlpha);
}

UStaticMeshComponent* APowerLine::GetOrCreateLineSegment(int32 SegmentIndex)
{
	if (LineSegments.IsValidIndex(SegmentIndex) && LineSegments[SegmentIndex])
	{
		return LineSegments[SegmentIndex];
	}

	UStaticMeshComponent* Segment = NewObject<UStaticMeshComponent>(this);
	Segment->SetStaticMesh(LineMesh ? LineMesh->GetStaticMesh() : nullptr);
	Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Segment->SetupAttachment(Root);
	Segment->RegisterComponent();
	LineSegments.Add(Segment);
	return Segment;
}

void APowerLine::HideUnusedLineSegments(int32 UsedSegmentCount)
{
	for (int32 SegmentIndex = UsedSegmentCount; SegmentIndex < LineSegments.Num(); ++SegmentIndex)
	{
		if (LineSegments[SegmentIndex])
		{
			LineSegments[SegmentIndex]->SetVisibility(false);
		}
	}
}

void APowerLine::UpdateLineSegment(
	UStaticMeshComponent* Segment,
	const FVector& StartLocation,
	const FVector& EndLocation)
{
	if (!Segment)
	{
		return;
	}

	const FVector Delta = EndLocation - StartLocation;
	const float Length = Delta.Size();
	if (Length <= UE_KINDA_SMALL_NUMBER)
	{
		Segment->SetVisibility(false);
		return;
	}

	const FVector Direction = Delta / Length;
	const FQuat ConnectionRotation = FQuat::FindBetweenNormals(FVector::UpVector, Direction);

	Segment->SetVisibility(true);
	Segment->SetWorldLocation(StartLocation + (Delta * 0.5f));
	Segment->SetWorldRotation(ConnectionRotation);

	const FBoxSphereBounds MeshBounds = Segment->GetStaticMesh()
		? Segment->GetStaticMesh()->GetBounds()
		: FBoxSphereBounds(FSphere(FVector::ZeroVector, 50.0f));
	const FVector MeshSize = MeshBounds.BoxExtent * 2.0f;
	const float MeshDiameterX = FMath::Max(MeshSize.X, UE_KINDA_SMALL_NUMBER);
	const float MeshDiameterY = FMath::Max(MeshSize.Y, UE_KINDA_SMALL_NUMBER);
	const float MeshLength = FMath::Max(MeshSize.Z, UE_KINDA_SMALL_NUMBER);
	const FVector MeshScale(
		LineThickness / MeshDiameterX,
		LineThickness / MeshDiameterY,
		Length / MeshLength);

	Segment->SetWorldScale3D(MeshScale);
	Segment->AddWorldOffset(ConnectionRotation.RotateVector(-(MeshBounds.Origin * MeshScale)));
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
