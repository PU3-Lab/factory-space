// Fill out your copyright notice in the Description page of Project Settings.


#include "ResourceBase.h"

#include "Wanted_Factory.h"
#include "MachineBase.h"
#include "OJJ_Grid.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UI/ResourceNameplateWidget.h"

namespace
{
	const TCHAR DefaultResourceTablePath[] = TEXT("/Game/DataTable/DT_ResourceData.DT_ResourceData");

	bool IsInfiniteOreResource(const AResourceBase* Resource)
	{
		if (!Resource)
		{
			return false;
		}

		if (Resource->HasShape(EResourceShape::Ore))
		{
			return true;
		}

		return Resource->GetResourceRowName().ToString().EndsWith(TEXT("_ore"));
	}
}


// Sets default values
AResourceBase::AResourceBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);

	NameplateWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("ResourceNameplateWidget"));
	NameplateWidget->SetupAttachment(Root);
	NameplateWidget->SetAbsolute(false, true, true);
	NameplateWidget->SetWidgetSpace(EWidgetSpace::World);
	NameplateWidget->SetWidgetClass(UResourceNameplateWidget::StaticClass());
	NameplateWidget->SetDrawSize(FVector2D(500.0f, 100.0f));
	NameplateWidget->SetPivot(FVector2D(0.5f, 0.5f));
	NameplateWidget->SetTwoSided(true);
	NameplateWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NameplateWidget->SetVisibility(false);
	NameplateWidget->SetHiddenInGame(true);
}

// Called when the game starts or when spawned
void AResourceBase::BeginPlay()
{
	Super::BeginPlay();

	NormalizeResourceDataHandle();

	if (IsInfiniteOreResource(this))
	{
		bIsInfinite = true;
	}

	Amount = FMath::Clamp(Amount, 0, MaxAmount);

	RegisterToGrid();

	if (NameplateWidget)
	{
		FResourceData NameplateData;
		FString DisplayName = GetResourceRowName().ToString();
		if (GetResourceData(NameplateData) && !NameplateData.DisplayName.IsEmpty())
		{
			DisplayName = NameplateData.DisplayName;
		}

		FBox ActorBounds(ForceInit);
		if (Root)
		{
			ActorBounds += Root->Bounds.GetBox();
		}
		if (Mesh)
		{
			ActorBounds += Mesh->Bounds.GetBox();
		}
		const FVector BoundsTop = ActorBounds.IsValid
			? FVector(ActorBounds.GetCenter().X, ActorBounds.GetCenter().Y, ActorBounds.Max.Z)
			: GetActorLocation();
		NameplateWidget->SetWorldLocation(BoundsTop + NameplateOffset);
		NameplateWidget->SetWorldScale3D(FVector(NameplateWorldSize / 100.0f));
		NameplateWidget->InitWidget();
		if (UResourceNameplateWidget* Nameplate =
			Cast<UResourceNameplateWidget>(NameplateWidget->GetWidget()))
		{
			Nameplate->SetResourceName(FText::FromString(DisplayName));
		}
		NameplateWidget->SetVisibility(false);
		NameplateWidget->SetHiddenInGame(true);
	}
}

void AResourceBase::SetNameplateVisible(bool bVisible, const FVector& ViewerLocation)
{
	if (!NameplateWidget)
	{
		return;
	}

	NameplateWidget->SetVisibility(bVisible);
	NameplateWidget->SetHiddenInGame(!bVisible);
	if (bVisible)
	{
		NameplateWidget->SetWorldScale3D(FVector(NameplateWorldSize / 100.0f));
		const FVector ToViewer = ViewerLocation - NameplateWidget->GetComponentLocation();
		if (!ToViewer.IsNearlyZero())
		{
			NameplateWidget->SetWorldRotation(ToViewer.Rotation());
		}
	}
}

bool AResourceBase::IsOreResource() const
{
	return HasShape(EResourceShape::Ore) ||
		GetResourceRowName().ToString().EndsWith(TEXT("_ore"));
}

void AResourceBase::NormalizeResourceDataHandle()
{
	if (ResourceID.IsNone() && !ResourceData.RowName.IsNone())
	{
		ResourceID = ResourceData.RowName;
	}
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
	if (bUseMeshBoundsForGridRegistration && Mesh)
	{
		const FBoxSphereBounds Bounds = Mesh->CalcBounds(Mesh->GetComponentTransform());
		const float CellSize = FMath::Abs((Grid->GridToWorld(FIntPoint(1, 0)) - Grid->GridToWorld(FIntPoint(0, 0))).X);
		const float EdgeEpsilon = FMath::Min(1.0f, CellSize * 0.01f);
		const FVector MinWorld(Bounds.Origin.X - Bounds.BoxExtent.X + EdgeEpsilon, Bounds.Origin.Y - Bounds.BoxExtent.Y + EdgeEpsilon, GetActorLocation().Z);
		const FVector MaxWorld(Bounds.Origin.X + Bounds.BoxExtent.X - EdgeEpsilon, Bounds.Origin.Y + Bounds.BoxExtent.Y - EdgeEpsilon, GetActorLocation().Z);
		const FIntPoint MinCell = Grid->WorldToGrid(MinWorld);
		const FIntPoint MaxCell = Grid->WorldToGrid(MaxWorld);

		const int32 MinX = FMath::Min(MinCell.X, MaxCell.X);
		const int32 MaxX = FMath::Max(MinCell.X, MaxCell.X);
		const int32 MinY = FMath::Min(MinCell.Y, MaxCell.Y);
		const int32 MaxY = FMath::Max(MinCell.Y, MaxCell.Y);
		Cells.Reserve((MaxX - MinX + 1) * (MaxY - MinY + 1));
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			for (int32 Y = MinY; Y <= MaxY; ++Y)
			{
				Cells.Add(FIntPoint(X, Y));
			}
		}
	}

	if (Cells.Num() == 0)
	{
		Cells.Add(Cell);
	}

	if (!Grid->OJJ_RegisterActorCells(this, Cells))
	{
		// 셀 충돌(다른 액터 점유) / off-grid — 등록 실패. 자원은 선점 대상에서 빠지므로 경고만.
		LOG_SSR_W(
			TEXT("RegisterToGrid failed (cell occupied or off-grid): Resource=%s OriginCell=(%d,%d) CellCount=%d"),
			*GetName(), Cell.X, Cell.Y, Cells.Num());
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
	UDataTable* ResourceTable = GetDefaultResourceTable();
	if (!ResourceTable)
	{
		LOG_SSR_W(TEXT("GetResourceData failed: DataTable is null. Resource=%s"), *GetName());
		return false;
	}

	const FName RowName = GetResourceRowName();
	if (RowName.IsNone())
	{
		LOG_SSR_W(
			TEXT("GetResourceData failed: RowName is None. Resource=%s DataTable=%s"),
			*GetName(),
			*ResourceTable->GetName()
		);
		return false;
	}
	
	const FResourceData* FoundData = ResourceTable->FindRow<FResourceData>(RowName, TEXT("GetResourceData"));
	
	if (!FoundData)
	{
		LOG_SSR_W(
			TEXT("GetResourceData failed: Row not found. Resource=%s DataTable=%s RowName=%s"),
			*GetName(),
			*ResourceTable->GetName(),
			*RowName.ToString()
		);
		return false;
	}
	
	OutResourceData = *FoundData;
	return true;
}

FName AResourceBase::GetResourceRowName() const
{
	return !ResourceID.IsNone() ? ResourceID : ResourceData.RowName;
}

UDataTable* AResourceBase::GetDefaultResourceTable() const
{
	return LoadObject<UDataTable>(nullptr, DefaultResourceTablePath);
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


