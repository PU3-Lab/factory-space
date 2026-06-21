// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ResourceData.h"
#include "Engine/DataTable.h"
#include "ResourceBase.generated.h"

class AMachineBase;

UCLASS()
class WANTED_FACTORY_API AResourceBase : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AResourceBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UStaticMeshComponent* Mesh;

	// 레거시 호환용. 실제 런타임 조회는 ResourceID + 기본 리소스 테이블을 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resource")
	FDataTableRowHandle ResourceData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
	FName ResourceID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
	int32 Amount = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
	int32 MaxAmount = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
	bool bIsInfinite = false;

	// true면 광물 메쉬 bounds가 덮는 그리드 셀 전체를 자원 점유 셀로 등록한다.
	// 채굴기는 이 점유 영역의 바깥 4방향 인접 셀 어디에서나 설치 가능해진다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource|Grid")
	bool bUseMeshBoundsForGridRegistration = true;

	// 이 자원을 선점한 머신(채굴기/펌프). weak — 머신이 비정상 소멸해도 자동 무효화돼
	// 선점이 자연 해제된다(IsClaimed가 IsValid로 판정). 명시 Release(머신 제거/EndPlay)도 병행.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Resource|Claim")
	TWeakObjectPtr<AMachineBase> ClaimedBy;

public:
	UFUNCTION(BlueprintCallable)
	virtual bool ConsumeResource(int32 ConsumeAmount);

	UFUNCTION(BlueprintCallable)
	virtual void AddResource(int32 AddAmount);

	UFUNCTION(BlueprintCallable)
	bool IsEmpty() const;
	
	UFUNCTION(BlueprintCallable, Category="Resource")
	bool GetResourceData(FResourceData& OutResourceData) const;
	
	UFUNCTION(BlueprintCallable)
	FName GetResourceRowName() const;

	// === 선점(Claim) 시스템 — 1자원 1머신 보장 ===

	// 유효한 머신이 선점 중인가. weak ptr이라 선점 머신이 소멸하면 자동 false(스테일 해제).
	UFUNCTION(BlueprintPure, Category = "Resource|Claim")
	bool IsClaimed() const;

	// Claimant가 이 자원을 선점. 이미 다른 유효 머신이 선점 중이면 false.
	// 동일 머신 재선점은 true(멱등). 서버 권위에서만 의미.
	UFUNCTION(BlueprintCallable, Category = "Resource|Claim")
	bool Claim(AMachineBase* Claimant);

	// 선점 해제. ClaimedBy가 Claimant와 같을 때만 해제(남의 선점 해제 방지). Claimant null이면 무조건 해제.
	UFUNCTION(BlueprintCallable, Category = "Resource|Claim")
	void Release(AMachineBase* Claimant);

	// === 타입 질의 (배치 제약 판정용) — DataTable shape/form 조회 래퍼 ===

	// ResourceData.shape == Shape 인가 (채굴기 Ore 판정 등). DataTable 무효면 false.
	UFUNCTION(BlueprintPure, Category = "Resource")
	bool HasShape(EResourceShape Shape) const;

	// ResourceData.form == Form 인가 (펌프 liquid 판정 등). DataTable 무효면 false.
	UFUNCTION(BlueprintPure, Category = "Resource")
	bool HasForm(FName Form) const;

protected:
	// 맵 사전 배치 대응: BeginPlay에서 자기 셀을 그리드 점유에 등록(+ XY를 셀 중심 스냅).
	// 단일 그리드 가정(SP) — 멀티 그리드는 OJJ_Grid.h KNOWN LIMITATION 참조.
	// 기본 구현은 단일 셀 등록(광맥). 다중 셀 영역(AWaterArea 등)은 override로 확장 — 본 경로/광맥 동작 무변경.
	virtual void RegisterToGrid();

	// 레거시 핸들에서 ResourceID를 보정하고, 기본 리소스 테이블을 코드에서 로드한다.
	void NormalizeResourceDataHandle();

	UDataTable* GetDefaultResourceTable() const;
};
