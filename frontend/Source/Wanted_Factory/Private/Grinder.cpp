// Fill out your copyright notice in the Description page of Project Settings.


#include "Grinder.h"

#include "Wanted_Factory.h"


// Sets default values
AGrinder::AGrinder()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	// GridSize = FVector2D(2, 2);
}

// Called when the game starts or when spawned
void AGrinder::BeginPlay()
{
	Super::BeginPlay();

}

// Called every frame
void AGrinder::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AGrinder::ProcessItem_Implementation()
{
	Super::ProcessItem_Implementation();
	LOG_SSR_W(TEXT("그라인더 실행중"));
}
