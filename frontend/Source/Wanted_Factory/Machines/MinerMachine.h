// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MachineBase.h"
#include "Resource/ResourceBase.h"
#include "MinerMachine.generated.h"

UCLASS()
class WANTED_FACTORY_API AMinerMachine : public AMachineBase
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AMinerMachine();

protected:
	virtual void BeginPlay() override;
	
	// 채굴 대상 자원
   UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining")
	AResourceBase* LinkedResource;

	// 한 번 채굴할 때 얻는 수량
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining")
	int32 MineAmount = 1;

	// 채굴 주기
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining")
	float MineInterval = 2.0f;

	FTimerHandle MineTimerHandle;

public:
	// Grid/Placement 쪽에서 설치 성공 후 호출
	UFUNCTION(BlueprintCallable, Category="Mining")
	void SetLinkedResource(AResourceBase* NewResource);

	// 채굴 가능한 상태인지 확인
	UFUNCTION(BlueprintCallable, Category="Mining")
	bool CanMine() const;

	// 실제 채굴 실행
	UFUNCTION(BlueprintCallable, Category="Mining")
	void MineResource();

	// 채굴 시작
	UFUNCTION(BlueprintCallable, Category="Mining")
	void StartMining();

	// 채굴 정지
	UFUNCTION(BlueprintCallable, Category="Mining")
	void StopMining();
};
