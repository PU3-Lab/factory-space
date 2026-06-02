// Fill out your copyright notice in the Description page of Project Settings.


#include "ResourceBase.h"


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
		return false;
	}
	
	const FResourceData* FoundData = 
		ResourceData.GetRow<FResourceData>(TEXT("GetResourceData"));
	
	if (!FoundData)
	{
		return false;
	}
	
	OutResourceData = *FoundData;
	return true;
}


