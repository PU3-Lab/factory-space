// Fill out your copyright notice in the Description page of Project Settings.

#include "OJJ_ProtectionTower.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "OJJ_ProtectionSubsystem.h"
#include "Wanted_Factory.h"

// 보호 대상 판별용(읽기 전용 include — SSR 머신 코드는 수정하지 않음).
#include "MachineBase.h"
#include "DummyMachineBase.h"

AOJJ_ProtectionTower::AOJJ_ProtectionTower()
{
	PrimaryActorTick.bCanEverTick = false;

	// 루트 = StaticMesh(비주얼). 메시 에셋은 BP에서 지정.
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(MeshComponent);

	// 보호 범위 구체. QueryOnly + 머신 오버랩만, 나머지 Ignore.
	// 머신은 WorldStatic(설치형)일 수도, WorldDynamic(Dummy/이동형)일 수도 있어 둘 다 Overlap.
	ProtectionRange = CreateDefaultSubobject<USphereComponent>(TEXT("ProtectionRange"));
	ProtectionRange->SetupAttachment(MeshComponent);
	ProtectionRange->SetSphereRadius(ProtectionRadius);
	ProtectionRange->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ProtectionRange->SetCollisionObjectType(ECC_WorldDynamic);
	ProtectionRange->SetCollisionResponseToAllChannels(ECR_Ignore);
	ProtectionRange->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);
	ProtectionRange->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	ProtectionRange->SetGenerateOverlapEvents(true);
}

void AOJJ_ProtectionTower::BeginPlay()
{
	Super::BeginPlay();

	// EditAnywhere로 조정된 반경을 런타임에 반영.
	ProtectionRange->SetSphereRadius(ProtectionRadius);

	ProtectionRange->OnComponentBeginOverlap.AddDynamic(this, &AOJJ_ProtectionTower::OnRangeBeginOverlap);
	ProtectionRange->OnComponentEndOverlap.AddDynamic(this, &AOJJ_ProtectionTower::OnRangeEndOverlap);

	// BeginOverlap은 "신규 진입"만 발생하므로, 스폰 시점에 이미 범위 안에 있던 머신을 직접 보호.
	TArray<AActor*> OverlappingActors;
	ProtectionRange->GetOverlappingActors(OverlappingActors);

	UOJJ_ProtectionSubsystem* Subsystem = GetProtectionSubsystem();
	if (!Subsystem)
	{
		LOG_OJJ_W(TEXT("ProtectionSubsystem unavailable in BeginPlay"));
		return;
	}

	for (AActor* OtherActor : OverlappingActors)
	{
		if (IsProtectableMachine(OtherActor) && !ProtectedMachines.Contains(OtherActor))
		{
			Subsystem->AddProtection(OtherActor);
			ProtectedMachines.Add(OtherActor);
		}
	}
}

void AOJJ_ProtectionTower::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 철거/레벨 종료 시 이 타워가 건 보호를 전부 반납.
	if (UOJJ_ProtectionSubsystem* Subsystem = GetProtectionSubsystem())
	{
		for (const TWeakObjectPtr<AActor>& WeakMachine : ProtectedMachines)
		{
			if (AActor* Machine = WeakMachine.Get())
			{
				Subsystem->RemoveProtection(Machine);
			}
		}
	}
	ProtectedMachines.Empty();

	Super::EndPlay(EndPlayReason);
}

void AOJJ_ProtectionTower::OnRangeBeginOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	if (!IsProtectableMachine(OtherActor) || ProtectedMachines.Contains(OtherActor))
	{
		return;
	}

	if (UOJJ_ProtectionSubsystem* Subsystem = GetProtectionSubsystem())
	{
		Subsystem->AddProtection(OtherActor);
		ProtectedMachines.Add(OtherActor);
	}
}

void AOJJ_ProtectionTower::OnRangeEndOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
	if (!OtherActor || !ProtectedMachines.Contains(OtherActor))
	{
		return;
	}

	if (UOJJ_ProtectionSubsystem* Subsystem = GetProtectionSubsystem())
	{
		Subsystem->RemoveProtection(OtherActor);
	}
	ProtectedMachines.Remove(OtherActor);
}

UOJJ_ProtectionSubsystem* AOJJ_ProtectionTower::GetProtectionSubsystem() const
{
	if (const UWorld* World = GetWorld())
	{
		return World->GetSubsystem<UOJJ_ProtectionSubsystem>();
	}
	return nullptr;
}

bool AOJJ_ProtectionTower::IsProtectableMachine(AActor* OtherActor) const
{
	if (!IsValid(OtherActor) || OtherActor == this)
	{
		return false;
	}

	// 둘 중 하나라도 성공하면 보호 대상. (통합 전: 이벤트는 Dummy에만 적용되지만,
	// 정식 AMachineBase도 미리 잡아두어 통합 후 즉시 동작하도록 한다.)
	return OtherActor->IsA(AMachineBase::StaticClass())
		|| OtherActor->IsA(ADummyMachineBase::StaticClass());
}
