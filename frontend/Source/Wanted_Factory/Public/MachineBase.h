// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Recipe/RecipeTable.h"
#include "MachineBase.generated.h"

class UTextRenderComponent;

// 효율 modifier 키 모음 (요인별). 새 효율 요인은 여기에 한 줄 추가 — 머신/이벤트 매니저 등
// 양쪽 파일의 문자열 중복을 방지하는 단일 정의 지점. 정의는 MachineBase.cpp.
namespace EfficiencyKeys
{
	WANTED_FACTORY_API extern const FName MagneticStorm; // 자기폭풍 (행성 이벤트)
	WANTED_FACTORY_API extern const FName Power;         // 전력 부족 (전력 시스템 대비, 예약)
}

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMachineDurabilityChanged, float, CurrentDurability, float, MaxDurability);

UENUM(BlueprintType)
enum class EMachineState : uint8
{
	Idle, // 기본
	Working, // 작동중
	NoPower, // 전력 없음
	Blocked, // 입,출력 버퍼가 꽉차서 더 생산할수 없을때
	Disabled // On/Off
};

// 포트 타입 : 입력 포트, 출력 포트
UENUM(BlueprintType)
enum class EPortType : uint8
{
	Input,
	Output
};

// 기계가 가진 포트 정보
USTRUCT(BlueprintType)
struct FMachinePortData
{
	GENERATED_BODY()
	
	// 포트 번호
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Machine | Ports")
	int32 PortIndex = 0;

	// 입력 / 출력 구분
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Machine | Ports")
	EPortType PortType = EPortType::Input;

	// 이 포트가 받을/보낼 아이템 제한용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Machine | Ports")
	FName AcceptedItemID;

	// 현재 연결되어 있는지
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine | Ports")
	bool bIsConnected = false;
};

// 출력 포트 하나가 어느 기계의 입력 포트와 연결되어 있는지 저장
USTRUCT(BlueprintType)
struct FMachinePortConnection
{
	GENERATED_BODY()

	// 내 출력 포트 번호
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Machine | Ports")
	int32 FromOutputPortIndex = 0;

	// 연결할 대상 기계
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Machine | Ports")
	AMachineBase* TargetMachine = nullptr;

	// 대상 기계의 입력 포트 번호
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Machine | Ports")
	int32 TargetInputPortIndex = 0;
};

UCLASS()
class WANTED_FACTORY_API AMachineBase : public AActor
{
	GENERATED_BODY()

public:
	AMachineBase();
	// --- UI 연동용 Getter 함수 ---
	EMachineState GetMachineState() const { return MachineState; }
	const TMap<FName, int32>& GetInputInventory() const { return InputInventory; }
	const TMap<FName, int32>& GetOutputBuffer() const { return OutputBuffer; }
	const FRecipeTable& GetCurrentRecipe() const { return CurrentRecipe; }
	int32 GetMaxInput() const { return MaxInputPerItem; }
	int32 GetMaxOutput() const { return MaxBufferPerItem; }
	FTimerHandle GetProcessTimer() const { return ProcessTimer; }
	float GetProcessTime() const { return ProcessTime; }
	FName GetMachineType() const { return MachineType; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine | Debug")
	UTextRenderComponent* DebugBufferText;

	// 머신 스테이트(상태)

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine | State")
	EMachineState MachineState = EMachineState::Idle;

	// 타이머
	FTimerHandle ProcessTimer;

	// // 현재 입력 아이템, 수량
	// UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine | Inventory")
	// FName CurrentInputItem;
	//
	// UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine | Inventory")
	// int32 CurrentInputCount = 0;

	// 입력 인벤토리
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine | Inventory")
	TMap<FName, int32> InputInventory;
	
	// 입력 인벤토리 최대 수량
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Machine | Inventory", meta = (ClampMin = "1"))
	int32 MaxInputPerItem = 50;

	// 나중에 창고/컨베이어로 빠져나간 최종 출력 저장소로 사용 가능
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine | Inventory")
	TMap<FName, int32> OutputInventory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine | Debug")
	bool bShowDebugBufferText = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine | Debug")
	FVector DebugTextOffset = FVector(0.0f, 0.0f, 180.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine | Debug", meta = (ClampMin = "1.0"))
	float DebugTextWorldSize = 24.0f;

	// 현재 사용 중인 레시피
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine | Inventory")
	FRecipeTable CurrentRecipe;
	
	// 기계 내부 출력 버퍼
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FName, int32> OutputBuffer;
	
	// 버퍼 최대량
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxBufferPerItem = 50;
	
	// 포트 부분
	// 입력 포트 목록 : 기계마다 개수 다르게 설정 가능
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Machine | Ports")
	TArray<FMachinePortData> InputPorts;

	// 출력 포트 목록 : 기계마다 개수 다르게 설정 가능
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Machine | Ports")
	TArray<FMachinePortData> OutputPorts;

	// 출력 포트별 연결 정보
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine | Ports")
	TArray<FMachinePortConnection> OutputConnections;
	
	// 내구도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine | Durability", meta=(ClampMin = "1"))
	float MaxDurability = 1000.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Machine | Durability")
	float CurrentDurability = 1000.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine | Durability")
	bool bDisableWhenBroken = true;
	
	// 전력
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine | Power")
	float PowerConsumption = 10.f;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Machine | Power")
	float CurrentProvidedPower = 0.f;

	// 요인별(키별) 효율 배율. 최종 효율 = 모든 값의 곱. 런타임 전용(Transient).
	// 예: MagneticStorm 0.6, Power 0.5 → 최종 0.3. 키는 EfficiencyKeys로 관리하며,
	// 같은 키를 두 호출자가 쓰면 서로 덮어쓰므로 키 소유를 분리할 것.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Machine | Planet Event")
	TMap<FName, float> EfficiencyModifiers;

public:
	// 기능들
	UPROPERTY(BlueprintAssignable, Category = "Machine | Durability")
	FOnMachineDurabilityChanged OnDurabilityChanged;

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintPure, Category = "Machine Settings")
	FVector2D GetMachineSize() const { return GridSize; }

	UFUNCTION(BlueprintPure, Category = "Machine Settings")
	int32 GetInputPortCount() const { return InputPortCount; }

	UFUNCTION(BlueprintPure, Category = "Machine Settings")
	int32 GetOutputPortCount() const { return OutputPortCount; }

	// 배치 Z(바닥 안착) 계산 등에서 메시 로컬 바운즈/월드스케일을 읽기 위한 접근자.
	UStaticMeshComponent* GetMeshComponent() const { return MeshComponent; }

	// 설치 가능 여부
	UFUNCTION(BlueprintCallable)
	virtual bool CanPlace();

	// 임시 테스트 용 : 아이템 투입
	UFUNCTION(BlueprintCallable, Category = "Machine | Inventory")
	virtual bool AddItem(FName ItemID, int32 Count);
	
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
	
	// 입력 버퍼에 들어갈수 있는지
	UFUNCTION(BlueprintCallable, Category = "Machine | Inventory")
	bool CanAddInputItem(FName ItemID, int32 Count) const;
	
	// 재료가 충분히 있나?
	bool HasEnoughIngredients(const FRecipeTable& Recipe) const;
	// 재료 소모하고 결과물 뽑는다
	void ConsumeIngredients(const FRecipeTable& Recipe);
	// 결과물 추가합니다
	virtual void AddOutputItem(FName ItemID, int32 Count);
	
	// 출력 버퍼가 꽉차 있으면 생산 금지
	virtual bool CanAddToOutputBuffer(const FRecipeTable& Recipe) const;
	
	UFUNCTION(BlueprintCallable, Category = "Machine | Buffer")
	bool TakeOutputItem(FName ItemID, int32 Count);

	UFUNCTION(BlueprintPure, Category = "Machine | Conveyor")
	bool PeekFirstOutputItem(FName& OutItemID) const;

	UFUNCTION(BlueprintCallable, Category = "Machine | Conveyor")
	bool TryTakeFirstOutputItem(FName& OutItemID);

	UFUNCTION(BlueprintPure, Category = "Machine | Conveyor")
	virtual bool CanReceiveConveyorItem(FName ItemID, int32 Count = 1) const;

	UFUNCTION(BlueprintCallable, Category = "Machine | Conveyor")
	virtual bool ReceiveConveyorItem(FName ItemID, int32 Count = 1);

	UFUNCTION(BlueprintCallable, Category = "Machine | Debug")
	void UpdateDebugBufferText();
	
	UFUNCTION(BlueprintCallable, Category = "Machine | Transfer")
	bool TransferOutputToMachine(AMachineBase* TargetMachine, FName ItemID, int32 Count);
	
	
	// 포트 부분
	
	// 내 출력 포트와 상대 입력 포트를 연결
	UFUNCTION(BlueprintCallable, Category = "Machine | Ports")
	bool ConnectOutputToMachine(int32 OutputPortIndex, AMachineBase* TargetMachine, int32 TargetInputPortIndex);

	// 특정 출력 포트로 아이템 전송
	UFUNCTION(BlueprintCallable, Category = "Machine | Ports")
	bool TransferOutputByPort(int32 OutputPortIndex, FName ItemID, int32 Count);
	
	UFUNCTION(BlueprintCallable, Category = "Machine | Debug")
	void DebugInventory();
	
	// 내구도 함수
	UFUNCTION(BlueprintPure, Category = "Machine | Durability")
	bool isBroken() const;
	
	UFUNCTION(BlueprintCallable, Category = "Machine | Durability")
	void DamageDurability(float DamageAmount);
	
	UFUNCTION(BlueprintCallable, Category = "Machine | Durability")
	void RepairDurability(float RepairAmount);

	UFUNCTION(BlueprintCallable, Category = "Machine | Durability")
	void ApplyDurabilityDamage(float DamageAmount);

	UFUNCTION(BlueprintPure, Category = "Machine | Durability")
	float GetMaxDurability() const { return MaxDurability; }

	UFUNCTION(BlueprintPure, Category = "Machine | Durability")
	float GetCurrentDurability() const { return CurrentDurability; }
	
	// 파워 (전력)
	UFUNCTION(BlueprintCallable, Category = "Machine | Power")
	void SetProvidedPower(float NewPower);
	
	UFUNCTION(BlueprintPure, Category = "Machine | Power")
	bool HasEnoughPower() const;

	UFUNCTION(BlueprintPure, Category = "Machine | Power")
	bool NeedsPower() const { return bNeedPower; }

	UFUNCTION(BlueprintPure, Category = "Machine | Power")
	float GetPowerConsumption() const { return PowerConsumption; }

	UFUNCTION(BlueprintPure, Category = "Machine | Power")
	float GetCurrentProvidedPower() const { return CurrentProvidedPower; }
	
	UFUNCTION(BlueprintCallable, Category = "Machine | State")
	void RefreshMachineState();

	// 요인별 효율 배율 설정. Value는 0.01~100.0 클램프, NaN/Inf·빈 키는 무시. 같은 키 재설정 시 덮어씀.
	UFUNCTION(BlueprintCallable, Category = "Machine | Planet Event")
	void SetEfficiencyModifier(FName Key, float Value);

	// 요인별 효율 배율 해제(해당 요인 영향 제거).
	UFUNCTION(BlueprintCallable, Category = "Machine | Planet Event")
	void ClearEfficiencyModifier(FName Key);

	// 최종 효율 = 모든 modifier의 곱. 비어 있으면 1.0, 하한 0.01.
	UFUNCTION(BlueprintPure, Category = "Machine | Planet Event")
	float GetFinalEfficiency() const;

	// [DEPRECATED] 효율 modifier 시스템으로 대체됨. 내부적으로 MagneticStorm 키에 위임한다.
	UFUNCTION(BlueprintCallable, Category = "Machine | Planet Event", meta = (DeprecatedFunction, DeprecationMessage = "SetEfficiencyModifier(키, 값)을 사용하세요."))
	void SetPlanetProductionEfficiency(float NewEfficiency);

	// [DEPRECATED] GetFinalEfficiency()를 사용하세요. (스테일 방지 위해 GetFinalEfficiency 위임)
	UFUNCTION(BlueprintPure, Category = "Machine | Planet Event", meta = (DeprecatedFunction, DeprecationMessage = "GetFinalEfficiency()를 사용하세요."))
	float GetPlanetProductionEfficiency() const { return GetFinalEfficiency(); }

	UFUNCTION(BlueprintPure, Category = "Machine | Planet Event")
	float GetEffectiveProcessTime(float BaseProcessTime) const;
	
	
};
