// Fill out your copyright notice in the Description page of Project Settings.

#include "PowerLine.h"

#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "FactoryManagerSubsystem.h"
#include "MachineBase.h"
#include "Machines/PowerGridNode.h"
#include "Machines/PowerPlant.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/UnrealMathUtility.h"
#include "UObject/ConstructorHelpers.h"

APowerLine::APowerLine()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	LineMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LineMesh"));
	LineMesh->SetupAttachment(Root);
	LineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LineSegments.Add(LineMesh);

	HumAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("HumAudio"));
	HumAudioComponent->SetupAttachment(Root);
	HumAudioComponent->bAutoActivate = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		LineMesh->SetStaticMesh(CylinderMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicShapeMaterial(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicShapeMaterial.Succeeded())
	{
		LineMaterialBase = BasicShapeMaterial.Object;
		LineMesh->SetMaterial(0, LineMaterialBase);
	}

	PulsePhaseOffset = FMath::FRandRange(0.0f, 2.0f * PI);
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

void APowerLine::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateMaterialPulse();
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
		RefreshHumSound(false);
		return;
	}

	FVector SourceLocation = GetEndpointLocationForActor(Source, EndpointHeightOffset);
	FVector TargetLocation = GetEndpointLocationForActor(Target, EndpointHeightOffset);

	if (Cast<APowerGridNode>(Source))
	{
		SourceLocation.Z -= PowerGridNodeEndpointLowerOffset;
	}
	else if (Cast<APowerPlant>(Source))
	{
		SourceLocation.Z -= PowerPlantEndpointLowerOffset;
	}

	if (Cast<APowerGridNode>(Target))
	{
		TargetLocation.Z -= PowerGridNodeEndpointLowerOffset;
	}
	else if (Cast<APowerPlant>(Target))
	{
		TargetLocation.Z -= PowerPlantEndpointLowerOffset;
	}

	const FVector Delta = TargetLocation - SourceLocation;
	const float Length = Delta.Size();
	if (Length <= UE_KINDA_SMALL_NUMBER)
	{
		HideUnusedLineSegments(0);
		RefreshHumSound(false);
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
	const bool bElectricallyConnected = IsElectricallyConnected();

	for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
	{
		const float StartAlpha = static_cast<float>(SegmentIndex) / static_cast<float>(SegmentCount);
		const float EndAlpha = static_cast<float>(SegmentIndex + 1) / static_cast<float>(SegmentCount);
		UStaticMeshComponent* Segment = GetOrCreateLineSegment(SegmentIndex);
		UpdateLineSegment(
			Segment,
			GetSagPoint(SourceLocation, TargetLocation, StartAlpha, SagDepth),
			GetSagPoint(SourceLocation, TargetLocation, EndAlpha, SagDepth));
		ApplyMaterialToSegment(Segment, bElectricallyConnected);
	}

	HideUnusedLineSegments(SegmentCount);

	// 험 사운드: 라인 중앙(sag 최저점)에서 재생, 통전 상태에 맞춰 페이드 in/out.
	if (HumAudioComponent)
	{
		HumAudioComponent->SetWorldLocation(GetSagPoint(SourceLocation, TargetLocation, 0.5f, SagDepth));
	}
	RefreshHumSound(bElectricallyConnected);
}

bool APowerLine::IsElectricallyConnected() const
{
	if (!SourceMachine.IsValid() || !TargetMachine.IsValid())
	{
		return false;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFactoryManagerSubsystem* FactoryManager = GameInstance->GetSubsystem<UFactoryManagerSubsystem>())
		{
			return FactoryManager->IsPowerLineEnergized(this);
		}
	}

	return false;
}

void APowerLine::RefreshHumSound(bool bElectricallyConnected)
{
	if (!HumAudioComponent || !HumSound)
	{
		return;
	}

	if (bHumActive == bElectricallyConnected)
	{
		return;
	}

	bHumActive = bElectricallyConnected;
	if (bHumActive)
	{
		HumAudioComponent->SetSound(HumSound);
		HumAudioComponent->FadeIn(HumFadeTime);
	}
	else
	{
		HumAudioComponent->FadeOut(HumFadeTime, 0.0f);
	}
}

float APowerLine::GetCurrentConnectedEmissiveStrength() const
{
	const UWorld* World = GetWorld();
	if (!World || ConnectedEmissiveStrength <= 0.0f)
	{
		return ConnectedEmissiveStrength;
	}

	const float PulseTime = (World->GetTimeSeconds() * ConnectedPulseFrequency) + PulsePhaseOffset;
	const float PulseWave = 0.5f + (0.5f * FMath::Sin(PulseTime));
	const float ShapedPulse = FMath::Square(PulseWave);
	const float PulseMultiplier = FMath::Lerp(
		ConnectedPulseMinMultiplier,
		ConnectedPulseMinMultiplier + ConnectedPulseAmplitude,
		ShapedPulse);

	return ConnectedEmissiveStrength * PulseMultiplier;
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
	const FVector ActorLocation = GetActorLocation();
	const FVector LocalStart = StartLocation - ActorLocation;
	const FVector LocalEnd = EndLocation - ActorLocation;
	const FVector LocalDelta = LocalEnd - LocalStart;

	Segment->SetVisibility(true);
	Segment->SetRelativeLocation(LocalStart + (LocalDelta * 0.5f));
	Segment->SetRelativeRotation(ConnectionRotation);

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

	Segment->SetRelativeScale3D(MeshScale);
	Segment->AddLocalOffset(-(MeshBounds.Origin * MeshScale));
}

UMaterialInterface* APowerLine::GetPowerLineMaterial(bool bElectricallyConnected)
{
	TObjectPtr<UMaterialInstanceDynamic>& MaterialInstance = bElectricallyConnected
		? ConnectedMaterialInstance
		: DisconnectedMaterialInstance;

	UMaterialInterface* BaseMaterial = nullptr;
	if (bElectricallyConnected)
	{
		BaseMaterial = ConnectedLineMaterial
			? ConnectedLineMaterial.Get()
			: (LineMaterialBase ? LineMaterialBase.Get() : UMaterial::GetDefaultMaterial(MD_Surface));
	}
	else
	{
		BaseMaterial = DisconnectedLineMaterial
			? DisconnectedLineMaterial.Get()
			: (LineMaterialBase ? LineMaterialBase.Get() : UMaterial::GetDefaultMaterial(MD_Surface));
	}

	if (!MaterialInstance || MaterialInstance->Parent != BaseMaterial)
	{
		MaterialInstance = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	}

	ConfigureMaterialInstance(MaterialInstance, bElectricallyConnected);
	return MaterialInstance.Get();
}

void APowerLine::ConfigureMaterialInstance(
	UMaterialInstanceDynamic* MaterialInstance,
	bool bElectricallyConnected) const
{
	if (!MaterialInstance)
	{
		return;
	}

	const FLinearColor LineColor = bElectricallyConnected ? ConnectedLineColor : DisconnectedLineColor;
	const float EmissiveStrength = bElectricallyConnected ? GetCurrentConnectedEmissiveStrength() : 0.0f;
	const FLinearColor EmissiveColor = LineColor * EmissiveStrength;

	MaterialInstance->SetVectorParameterValue(TEXT("Color"), LineColor);
	MaterialInstance->SetVectorParameterValue(TEXT("BaseColor"), LineColor);
	MaterialInstance->SetVectorParameterValue(TEXT("Tint"), LineColor);
	MaterialInstance->SetVectorParameterValue(TEXT("EmissiveColor"), EmissiveColor);
	MaterialInstance->SetScalarParameterValue(TEXT("Opacity"), LineColor.A);
	MaterialInstance->SetScalarParameterValue(TEXT("Alpha"), LineColor.A);
	MaterialInstance->SetScalarParameterValue(TEXT("EmissiveStrength"), EmissiveStrength);
	MaterialInstance->SetScalarParameterValue(TEXT("PulseAlpha"), bElectricallyConnected ? EmissiveStrength : 0.0f);
}

void APowerLine::UpdateMaterialPulse()
{
	if (ConnectedMaterialInstance)
	{
		ConfigureMaterialInstance(ConnectedMaterialInstance, IsElectricallyConnected());
	}
}

void APowerLine::ApplyMaterialToSegment(UStaticMeshComponent* Segment, bool bElectricallyConnected)
{
	if (Segment)
	{
		Segment->SetMaterial(0, GetPowerLineMaterial(bElectricallyConnected));
	}
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
