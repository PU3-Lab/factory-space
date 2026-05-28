// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Recipe/RecipeTable.h"
#include "MachineBase.generated.h"

UENUM(BlueprintType)
enum class EMachineState : uint8
{
	Idle, // 기본
	Working, // 작동중
	NoPower, // 전력 없음
	Blocked, // 입,출력 버퍼가 꽉차서 더 생산할수 없을때
	Disabled // On/Off
};

UCLASS()
class WANTED_FACTORY_API AMachineBase : public AActor
{
	GENERATED_BODY()

public:
	AMachineBase();

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	// 그리드 세팅

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine | Grid", meta = (ClampMin = "1"))
	FIntPoint GridSize = FIntPoint(1,1);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine | Grid")
	FIntPoint GridPosision;

	// 입력 포트 개수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine | Ports", meta = (ClampMin = "0"))
	int32 InputPortCount;

	// 출력 포트 개수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine | Ports", meta = (ClampMin = "0"))
	int32 OutputPortCount;

	// 생산시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine | Settings")
	float ProcessTime = 3.f;

	// 기계 타입
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine | Settings")
	FName MachineType;

	// 전력 필요 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine | Settings")
	bool bNeedPower = false;


	// 컴포넌트

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine | Components")
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine | Components")
	UStaticMeshComponent* MeshComponent;

	// 머신 스테이트(상태)

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine | State")
	EMachineState MachineState = EMachineState::Idle;

	// 타이머
	FTimerHandle ProcessTimer;

	// 현재 입력 아이템, 수량
	UPROPERTY(visibleAnywhere, BlueprintReadOnly, Category = "Machine | Inventory")
	FName CurrentInputItem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine | Inventory")
	int32 CurrentInputCount = 0;

	// 입력 인벤토리
	// UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine | Inventory")
	// TMap<FName, TArray<FName>> InputToRecipeMap;

	// 출력 인벤토리
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine | Inventory")
	TMap<FName, int32> OutputInventory;

	// 현재 사용 중인 레시피
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine | Inventory")
	FRecipeTable CurrentRecipe;

public:
	// 기능들
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintPure, Category = "Machine Settings")
	FVector2D GetMachineSize() const { return GridSize; }

	UFUNCTION(BlueprintPure, Category = "Machine Settings")
	int32 GetInputPortCount() const { return InputPortCount; }

	UFUNCTION(BlueprintPure, Category = "Machine Settings")
	int32 GetOutputPortCount() const { return OutputPortCount; }

	// 설치 가능 여부
	UFUNCTION(BlueprintCallable)
	virtual bool CanPlace();

	// 임시 테스트 용 : 아이템 투입
	UFUNCTION(BlueprintCallable, Category = "Machine | Inventory")
	virtual void AddItem(FName ItemID, int32 Count);

	// 자동 레시피 탐색
	UFUNCTION(BlueprintCallable, Category = "Machine | Process")
	virtual void TryStartProcess();

	// 가공 시작
	UFUNCTION(BlueprintCallable)
	virtual void StartProcess();

	// 가공 완료
	UFUNCTION(BlueprintCallable)
	virtual void FinishProcess();

	// 실제 가공 내용 (자식에서 Override)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void ProcessItem();

	virtual void ProcessItem_Implementation();

	// 기계 정지
	UFUNCTION(BlueprintCallable)
	virtual void StopProcess();
};
