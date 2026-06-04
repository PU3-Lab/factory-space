// Fill out your copyright notice in the Description page of Project Settings.

#include "OJJ_ProtectionSubsystem.h"

#include "GameFramework/Actor.h"
#include "Wanted_Factory.h"

// 문자열 상수 단일 정의 지점.
const FName UOJJ_ProtectionSubsystem::MagneticProtectedTag(TEXT("MagneticProtected"));

void UOJJ_ProtectionSubsystem::AddProtection(AActor* Machine)
{
	if (!IsValid(Machine))
	{
		return;
	}

	// 파괴된 머신 잔여 항목 정리(약참조 키 무효화 대응).
	PurgeStaleEntries();

	int32& Count = ProtectionCounts.FindOrAdd(Machine);
	++Count;

	// 0 -> 1 전이일 때만 태그 부여(중복 부여 방지).
	if (Count == 1)
	{
		if (!Machine->Tags.Contains(MagneticProtectedTag))
		{
			Machine->Tags.Add(MagneticProtectedTag);
		}
		LOG_OJJ(TEXT("MagneticProtected ON: %s"), *Machine->GetName());
	}
}

void UOJJ_ProtectionSubsystem::RemoveProtection(AActor* Machine)
{
	if (!Machine)
	{
		return;
	}

	const TWeakObjectPtr<AActor> Key(Machine);
	int32* CountPtr = ProtectionCounts.Find(Key);
	if (!CountPtr)
	{
		// Add 없이 호출되었거나 이미 정리됨 — 음수 방지.
		return;
	}

	--(*CountPtr);

	// 1 -> 0 전이일 때만 태그 제거 + 맵에서 제거.
	if (*CountPtr <= 0)
	{
		if (IsValid(Machine))
		{
			Machine->Tags.Remove(MagneticProtectedTag);
			LOG_OJJ(TEXT("MagneticProtected OFF: %s"), *Machine->GetName());
		}
		ProtectionCounts.Remove(Key);
	}
}

bool UOJJ_ProtectionSubsystem::IsProtected(const AActor* Machine) const
{
	// 태그가 단일 진실 소스(이찬 이벤트 적용부도 이 태그를 읽는다).
	return IsValid(Machine) && Machine->ActorHasTag(MagneticProtectedTag);
}

void UOJJ_ProtectionSubsystem::PurgeStaleEntries()
{
	for (auto It = ProtectionCounts.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}
