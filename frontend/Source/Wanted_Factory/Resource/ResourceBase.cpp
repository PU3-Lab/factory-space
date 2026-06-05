// Fill out your copyright notice in the Description page of Project Settings.


#include "ResourceBase.h"

#include "Wanted_Factory.h"
#include "MachineBase.h"
#include "OJJ_Grid.h"
#include "Kismet/GameplayStatics.h"


// Sets default values
AResourceBase::AResourceBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
}

// Called when the game starts or when spawned
void AResourceBase::BeginPlay()
{
	Super::BeginPlay();

	Amount = FMath::Clamp(Amount, 0, MaxAmount);

	RegisterToGrid();
}

void AResourceBase::RegisterToGrid()
{
	// 그리드 점유는 서버 권위에서만 기록(OJJ_RegisterActorCells 내부도 HasAuthority ensure).
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AOJJ_Grid* Grid = Cast<AOJJ_Grid>(UGameplayStatics::GetActorOfClass(World, AOJJ_Grid::StaticClass()));
	if (!Grid)
	{
		LOG_SSR_W(TEXT("RegisterToGrid skipped: no AOJJ_Grid in world. Resource=%s"), *GetName());
		return;
	}

	// 액터 BeginPlay 순서는 비보장이지만, 그리드 등록은 그리드의 BeginPlay 산출물에 의존하지 않는다 —
	// WorldToGrid/IsValidGridCell은 CellSize/GridSize(에디터·생성자 시점 확정 UPROPERTY)와 액터 위치만 사용.
	// 따라서 광맥 BeginPlay가 그리드 BeginPlay보다 먼저 돌아도 안전(액터 자체는 이미 존재).
	const FIntPoint Cell = Grid->WorldToGrid(GetActorLocation());

	// XY를 셀 중심으로 스냅(점유 셀과 시각 정합). Z는 메시 높이 보존.
	const FVector CellCenter = Grid->GridToWorld(Cell);
	SetActorLocation(FVector(CellCenter.X, CellCenter.Y, GetActorLocation().Z));

	TArray<FIntPoint> Cells;
	Cells.Add(Cell);
	if (!Grid->OJJ_RegisterActorCells(this, Cells))
	{
		// 셀 충돌(다른 액터 점유) / off-grid — 등록 실패. 자원은 선점 대상에서 빠지므로 경고만.
		LOG_SSR_W(
			TEXT("RegisterToGrid failed (cell occupied or off-grid): Resource=%s Cell=(%d,%d)"),
			*GetName(), Cell.X, Cell.Y);
	}
}

bool AResourceBase::ConsumeResource(int32 ConsumeAmount)
{
	if (ConsumeAmount <= 0)
	{
		return false;
	}

	// 무한 자원이라면 수량을 줄이지 않고 성공 처리
	if (bIsInfinite)
	{
		return true;
	}

	if (Amount < ConsumeAmount)
	{
		return false;
	}

	Amount -= ConsumeAmount;
	return true;
}

void AResourceBase::AddResource(int32 AddAmount)
{
	if (AddAmount <= 0)
	{
		return;
	}
	Amount = FMath::Clamp(Amount + AddAmount, 0, MaxAmount);

}

bool AResourceBase::IsEmpty() const
{
	return !bIsInfinite && Amount == 0;
}

bool AResourceBase::GetResourceData(FResourceData& OutResourceData) const
{
	if (!ResourceData.DataTable)
	{
		LOG_SSR_W(TEXT("GetResourceData failed: DataTable is null. Resource=%s"), *GetName());
		return false;
	}

	if (ResourceData.RowName.IsNone())
	{
		LOG_SSR_W(
			TEXT("GetResourceData failed: RowName is None. Resource=%s DataTable=%s"),
			*GetName(),
			*ResourceData.DataTable->GetName()
		);
		return false;
	}
	
	const FResourceData* FoundData = 
		ResourceData.GetRow<FResourceData>(TEXT("GetResourceData"));
	
	if (!FoundData)
	{
		LOG_SSR_W(
			TEXT("GetResourceData failed: Row not found. Resource=%s DataTable=%s RowName=%s"),
			*GetName(),
			*ResourceData.DataTable->GetName(),
			*ResourceData.RowName.ToString()
		);
		return false;
	}
	
	OutResourceData = *FoundData;
	return true;
}

FName AResourceBase::GetResourceRowName() const
{
	return ResourceData.RowName;
}

bool AResourceBase::IsClaimed() const
{
	// weak ptr이 유효하면 선점 중. 선점 머신이 소멸하면 자동 false.
	return ClaimedBy.IsValid();
}

bool AResourceBase::Claim(AMachineBase* Claimant)
{
	if (!Claimant)
	{
		return false;
	}

	// 다른 유효 머신이 선점 중이면 거부. 동일 머신 재선점은 멱등 성공.
	if (ClaimedBy.IsValid() && ClaimedBy.Get() != Claimant)
	{
		LOG_SSR_W(
			TEXT("Claim rejected: already claimed. Resource=%s Owner=%s Requester=%s"),
			*GetName(), *ClaimedBy->GetName(), *Claimant->GetName());
		return false;
	}

	ClaimedBy = Claimant;
	return true;
}

void AResourceBase::Release(AMachineBase* Claimant)
{
	// Claimant 지정 시 소유자 일치할 때만 해제(남의 선점 보호). null이면 무조건 해제.
	if (Claimant && ClaimedBy.IsValid() && ClaimedBy.Get() != Claimant)
	{
		return;
	}
	ClaimedBy = nullptr;
}

bool AResourceBase::HasShape(EResourceShape Shape) const
{
	FResourceData Data;
	return GetResourceData(Data) && Data.shape == Shape;
}

bool AResourceBase::HasForm(FName Form) const
{
	FResourceData Data;
	return GetResourceData(Data) && Data.form == Form;
}


