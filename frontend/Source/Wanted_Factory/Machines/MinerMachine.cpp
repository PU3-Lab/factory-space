// Fill out your copyright notice in the Description page of Project Settings.


#include "MinerMachine.h"

#include "Wanted_Factory.h"
#include "OJJ_Grid.h"


// Sets default values
AMinerMachine::AMinerMachine()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
}

// Called when the game starts or when spawned
void AMinerMachine::BeginPlay()
{
	Super::BeginPlay();

}

void AMinerMachine::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 그리드 RemoveMachine를 거치지 않는 소멸(레벨 전환/직접 Destroy)에서도 선점 해제를 보장.
	// ClaimedBy가 weak라 자동 무효화되긴 하나, 명시 해제로 즉시 자유화 + LinkedResource 정리.
	if (IsValid(LinkedResource))
	{
		LinkedResource->Release(this);
	}
	LinkedResource = nullptr;

	Super::EndPlay(EndPlayReason);
}

bool AMinerMachine::CanPlaceAdditional(const AOJJ_Grid* Grid, FIntPoint Origin, int32 RotationSteps) const
{
	// 인접 미선점 Ore 광맥이 하나라도 있어야 배치 가능. (CDO에서도 호출 — Grid 인자만 사용.)
	return FindAdjacentUnclaimedOre(Grid, Origin, RotationSteps) != nullptr;
}

void AMinerMachine::OnPlacedOnGrid(AOJJ_Grid* Grid, FIntPoint Origin, int32 RotationSteps)
{
	Super::OnPlacedOnGrid(Grid, Origin, RotationSteps);

	AResourceBase* Ore = FindAdjacentUnclaimedOre(Grid, Origin, RotationSteps);
	if (!Ore)
	{
		// CanPlaceAdditional 통과 후 여기서 못 찾으면 경합/타이밍 이상(동기 경로라 보통 발생 안 함).
		LOG_SSR_W(TEXT("OnPlacedOnGrid: no adjacent unclaimed Ore (race?). Miner=%s"), *GetName());
		return;
	}

	if (Ore->Claim(this))
	{
		SetLinkedResource(Ore);
	}
	else
	{
		LOG_SSR_W(TEXT("OnPlacedOnGrid: Claim failed. Miner=%s Ore=%s"), *GetName(), *Ore->GetName());
	}
}

void AMinerMachine::OnRemovedFromGrid()
{
	Super::OnRemovedFromGrid();

	if (IsValid(LinkedResource))
	{
		LinkedResource->Release(this);
	}
	LinkedResource = nullptr;
}

AResourceBase* AMinerMachine::FindAdjacentUnclaimedOre(const AOJJ_Grid* Grid, FIntPoint Origin, int32 RotationSteps) const
{
	if (!Grid)
	{
		return nullptr;
	}

	// 풋프린트 셀 집합 (회전·정수화 규칙은 그리드와 동일한 EffectiveSize 사용).
	const FIntPoint Size = AOJJ_Grid::EffectiveSize(GetMachineSize(), RotationSteps);
	TSet<FIntPoint> Footprint;
	Footprint.Reserve(Size.X * Size.Y);
	for (int32 X = 0; X < Size.X; ++X)
	{
		for (int32 Y = 0; Y < Size.Y; ++Y)
		{
			Footprint.Add(Origin + FIntPoint(X, Y));
		}
	}

	static const FIntPoint Dirs[] = {
		FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1)
	};

	// footprint 바깥의 4방향 인접 셀만 검사(중복 후보 1회만).
	TSet<FIntPoint> Visited;
	for (const FIntPoint& Cell : Footprint)
	{
		for (const FIntPoint& Dir : Dirs)
		{
			const FIntPoint Neighbor = Cell + Dir;
			if (Footprint.Contains(Neighbor) || Visited.Contains(Neighbor))
			{
				continue;
			}
			Visited.Add(Neighbor);

			AResourceBase* Resource = Cast<AResourceBase>(Grid->GetActorAtCell(Neighbor));
			if (Resource && !Resource->IsClaimed() && Resource->HasShape(EResourceShape::Ore))
			{
				return Resource;
			}
		}
	}
	return nullptr;
}

void AMinerMachine::SetLinkedResource(AResourceBase* NewResource)
{
	LinkedResource = NewResource;

	if (LinkedResource)
	{
		LOG_SSR_W(TEXT("Miner linked to resource: %s"), *LinkedResource->GetName());
	}
	else
	{
		LOG_SSR_W(TEXT("Miner linked resource is null."));
	}
}

bool AMinerMachine::CanMine() const
{
	if (!LinkedResource)
	{
		LOG_SSR_W(TEXT("CanMine failed: LinkedResource is null."));
		return false;
	}
	
	FResourceData Data;
	if (!LinkedResource->GetResourceData(Data))
	{
		LOG_SSR_W(
			TEXT("CanMine failed: ResourceData is invalid. Resource=%s RowName=%s"),
			*LinkedResource->GetName(),
			*LinkedResource->GetResourceRowName().ToString()
		);
		return false;
	}

	if (Data.shape != EResourceShape::Ore)
	{
		LOG_SSR_W(
			TEXT("CanMine failed: Resource shape is not Ore. Resource=%s RowName=%s Shape=%s"),
			*LinkedResource->GetName(),
			*LinkedResource->GetResourceRowName().ToString(),
			*UEnum::GetValueAsString(Data.shape)
		);
		return false;
	}

	return true;
}

void AMinerMachine::StartMining()
{
	if (MineAmount <= 0 || MineInterval <= 0.f)
	{
		LOG_SSR_W(
			TEXT("Cannot start mining. Invalid mining settings. MineAmount=%d MineInterval=%.2f"),
			MineAmount,
			MineInterval
		);
		return;
	}

	if (!CanMine())
	{
		LOG_SSR_W(TEXT("Cannot start mining."))
		return;
	}

	GetWorldTimerManager().SetTimer(
		MineTimerHandle,
		this,
		&AMinerMachine::MineResource,
		MineInterval,
		true
	);

	LOG_SSR_W(TEXT("Mining started."));
	UE_LOG(LogTemp, Warning, TEXT("Mining started."));
}

void AMinerMachine::StopMining()
{
	GetWorldTimerManager().ClearTimer(MineTimerHandle);
	
	LOG_SSR_W(TEXT("Mining stopped."));
}

void AMinerMachine::MineResource()
{
	if (!CanMine())
	{
		StopMining();
		return;
	}
	
	// RowName이 아이템 ID 역할
	const FName ResourceID =
		LinkedResource->GetResourceRowName();

	if (ResourceID.IsNone())
	{
		LOG_SSR_W(TEXT("MineResource failed: Resource row name is None."));
		StopMining();
		return;
	}

	const int32 CurrentOutputCount = OutputBuffer.FindRef(ResourceID);

	if (CurrentOutputCount + MineAmount > MaxBufferPerItem)
	{
		LOG_SSR_W(
			TEXT("MineResource failed: Output buffer full. Item=%s Count=%d Max=%d"),
			*ResourceID.ToString(),
			CurrentOutputCount,
			MaxBufferPerItem
		);
		StopMining();
		return;
	}

	if (!LinkedResource->ConsumeResource(MineAmount))
	{
		LOG_SSR_W(TEXT("Resource depleted"));
		StopMining();
		return;
	}
	
	AddOutputItem(ResourceID, MineAmount);

	LOG_SSR_W(TEXT("Mined Resource: %s x %d"),
		*ResourceID.ToString(),
		MineAmount);
}



