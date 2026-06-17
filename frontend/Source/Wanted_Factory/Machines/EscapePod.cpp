#include "EscapePod.h"

#include "EngineUtils.h"
#include "OJJ_Grid.h"

namespace
{
	bool BuildRegistrationCandidate(AOJJ_Grid* Grid, AEscapePod* EscapePod, FIntPoint& OutOrigin, float& OutDistSquared)
	{
		if (!Grid || !EscapePod)
		{
			return false;
		}

		const FIntPoint CenterCell = Grid->WorldToGrid(EscapePod->GetActorLocation());
		const FIntPoint Size = AOJJ_Grid::EffectiveSize(EscapePod->GetMachineSize(), 0);
		const FIntPoint Origin = AOJJ_Grid::OJJ_OriginFromCursorCellForSize(CenterCell, Size);
		const FVector ExpectedLocation = Grid->GetMachinePlacementLocation(EscapePod, Origin);
		const FVector ActualLocation = EscapePod->GetActorLocation();

		OutOrigin = Origin;
		OutDistSquared = FVector::DistSquared2D(ExpectedLocation, ActualLocation);
		return true;
	}
}

AEscapePod::AEscapePod()
{
	PrimaryActorTick.bCanEverTick = false;

	MachineType = TEXT("EscapePod");
	bNeedPower = false;
	InputPortCount = 0;
	InputBufferCount = 0;
	OutputPortCount = 0;
	OutputBufferCount = 0;
	bShowDebugBufferText = false;
}

void AEscapePod::BeginPlay()
{
	Super::BeginPlay();

	RegisterToNearestGrid();
}

void AEscapePod::OnRemovedFromGrid()
{
	Super::OnRemovedFromGrid();
}

void AEscapePod::RegisterToNearestGrid()
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	AOJJ_Grid* BestGrid = nullptr;
	FIntPoint BestOrigin = FIntPoint::ZeroValue;
	float BestDistSquared = TNumericLimits<float>::Max();

	for (TActorIterator<AOJJ_Grid> It(GetWorld()); It; ++It)
	{
		AOJJ_Grid* Grid = *It;
		FIntPoint CandidateOrigin;
		float CandidateDistSquared = 0.0f;
		if (!BuildRegistrationCandidate(Grid, this, CandidateOrigin, CandidateDistSquared))
		{
			continue;
		}

		if (CandidateDistSquared < BestDistSquared)
		{
			BestGrid = Grid;
			BestOrigin = CandidateOrigin;
			BestDistSquared = CandidateDistSquared;
		}
	}

	if (!BestGrid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EscapePod] No grid found for pre-placed escape pod: %s"), *GetName());
		return;
	}

	const FVector PlacementLocation = BestGrid->GetMachinePlacementLocation(this, BestOrigin);
	const FVector CurrentLocation = GetActorLocation();
	SetActorLocation(FVector(PlacementLocation.X, PlacementLocation.Y, CurrentLocation.Z));

	FString OutReason;
	if (!BestGrid->RegisterExistingMachineOccupancyOnly(this, BestOrigin, OutReason))
	{
		UE_LOG(LogTemp, Warning, TEXT("[EscapePod] Failed to register pre-placed escape pod: %s (%s)"),
			*GetName(),
			*OutReason);
	}
}
