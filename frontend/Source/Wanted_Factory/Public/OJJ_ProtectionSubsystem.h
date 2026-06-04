// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OJJ_ProtectionSubsystem.generated.h"

/**
 * 자기폭풍(MagneticStorm) 보호 상태 관리 서브시스템 (OJJ 소유).
 *
 * AOJJ_ProtectionTower들이 범위 진입/이탈 시 AddProtection/RemoveProtection을 호출한다.
 * 머신별로 "현재 보호 중인 타워 수"를 카운트하며, 카운트가 0->1로 올라갈 때만 머신에
 * "MagneticProtected" Actor Tag를 부여하고, 1->0으로 내려갈 때만 제거한다.
 *
 * ⚠️ 중첩 카운트 이유: 타워 2개가 같은 머신을 보호할 때, 그 중 하나를 철거해도 다른
 *    타워의 보호가 유지되어야 한다. 단순 Tag 추가/제거로는 표현 불가하므로 카운트가 필요.
 *
 * 이벤트 적용부(이찬, PlanetEventManagerSubsystem)는 이 태그를 읽어 효율 저하를 스킵할
 * 예정이다. 본 서브시스템은 태그 "관리"만 담당하며 이벤트 코드는 건드리지 않는다.
 */
UCLASS()
class WANTED_FACTORY_API UOJJ_ProtectionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// 자기폭풍 보호 표시 태그. 문자열 리터럴은 이 한 곳에서만 정의한다(.cpp).
	static const FName MagneticProtectedTag;

	// 타워 범위 진입 시 호출. 카운트 ++; 0->1이면 머신에 태그 부여.
	void AddProtection(AActor* Machine);

	// 타워 범위 이탈 / 타워 철거 시 호출. 카운트 --; 1->0이면 태그 제거 + 맵에서 제거.
	// 음수로 내려가지 않도록 가드한다.
	void RemoveProtection(AActor* Machine);

	// 현재 보호 중인지 질의(디버그/검증용). 태그를 단일 진실 소스로 사용.
	UFUNCTION(BlueprintPure, Category = "OJJ|Protection")
	bool IsProtected(const AActor* Machine) const;

private:
	// 머신별 보호 타워 중첩 카운트. 약참조 키로 파괴된 머신을 자연 무효화한다.
	TMap<TWeakObjectPtr<AActor>, int32> ProtectionCounts;

	// 파괴되어 무효가 된 머신 항목 정리.
	void PurgeStaleEntries();
};
