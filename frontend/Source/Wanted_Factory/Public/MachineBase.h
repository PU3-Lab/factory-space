// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Machines/MachineTable.h"
#include "Recipe/RecipeTable.h"
#include "MachineBase.generated.h"

class UTextRenderComponent;
class AOJJ_Grid;
class UStaticMesh;

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
	virtual void ApplyMachineData(const FMachineTableRow& MachineData);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	bool ApplyMachineDataFromSubsystem();

	// 그리드 세팅

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine | Grid", meta = (ClampMin = "1"))
	FIntPoint GridSize = FIntPoint(1,1);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine | Grid")
	FIntPoint GridPosision;

	// 메시 스케일 미세조정 배율. 바운즈 정규화(footprint 자동 정합) 결과에 곱해진다.
	// 기본 (1,1,1) = 정규화 그대로. 셀을 살짝 넘치거나 줄이는 머신별 연출 보정용.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine | Mesh")
	FVector MeshScaleMultiplier = FVector(1.0f, 1.0f, 1.0f);

	// 셀 한 칸의 월드 크기(uu). ★ AOJJ_Grid::CellSize(=100)와 반드시 동기화 ★ — 메시 바운즈를
	// footprint(GridSize × 이 값)에 정규화하는 데 사용. core(MachineBase)가 OJJ_Grid 헤더에
	// 역의존하지 않도록 상수로 둔다(양쪽 주석으로 동기화 명시).
	static constexpr float MeshFitCellWorld = 100.0f;

	// 입력 포트 개수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine | Ports", meta = (ClampMin = "0"))
	int32 InputPortCount;

	// 출력 포트 개수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine | Ports", meta = (ClampMin = "0"))
	int32 OutputPortCount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine | Inventory", meta = (ClampMin = "0"))
	int32 InputBufferCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine | Inventory", meta = (ClampMin = "0"))
	int32 OutputBufferCount = 1;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine | Durability")
	FName RepairCostItemID;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine | Durability", meta = (ClampMin = "0"))
	int32 RepairBaseCostQty = 0;
	
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

	UFUNCTION(BlueprintPure, Category = "Machine Settings")
	int32 GetInputBufferCount() const { return InputBufferCount; }

	UFUNCTION(BlueprintPure, Category = "Machine Settings")
	int32 GetOutputBufferCount() const { return OutputBufferCount; }

	// 배치 Z(바닥 안착) 계산 등에서 메시 로컬 바운즈/월드스케일을 읽기 위한 접근자.
	UStaticMeshComponent* GetMeshComponent() const { return MeshComponent; }

	// 고스트 프리뷰(#187)가 머신 메시별 미세조정 배율을 액터 스폰 없이 재현하기 위한 접근자.
	FVector GetMeshScaleMultiplier() const { return MeshScaleMultiplier; }

	// 메시 바운즈를 footprint(GridSize × MeshFitCellWorld)에 정규화하는 단일 진실원(#187).
	// FitMeshToGrid / OnConstruction / 고스트 프리뷰가 모두 이 식을 공유 — 수치 동작 동일(무회귀).
	// 폴백: Mesh 널/바운즈 0이면 Scale=(GridSize.X,GridSize.Y,1). 최종 결과에 MeshScaleMultiplier를 곱한다.
	static FVector OJJ_ComputeMeshFitScale(const UStaticMesh* Mesh, FIntPoint GridSize, FVector MeshScaleMultiplier);

	// 설치 가능 여부
	UFUNCTION(BlueprintCallable)
	virtual bool CanPlace();

	// === 머신별 추가 배치 제약 / 배치·제거 훅 (기본 무제약) ===
	// 그리드가 머신 종류를 모르게 하기 위해 머신이 오버라이드(채굴기=인접 광맥, 펌프=인접 수원 등).
	// CanPlaceMachine이 footprint 점유 검사에 더해 이 함수를 AND로 호출 → 호버 색/배치 판정 동일 경로.
	//
	// ⚠️ CDO-safe 요구: 호버 미리보기는 머신을 스폰하기 전에 CDO(GetDefaultObject)로 판정한다
	//    (AOJJ_BuildController가 CDO로 풋프린트·CanPlaceMachine 호출). 따라서 구현은 인자 Grid/Origin/
	//    RotationSteps와 CDO-safe getter(GetMachineSize 등)만 사용하고, this의 GetWorld()/GetActorLocation()/
	//    인스턴스 멤버 상태에 의존하지 말 것. (CDO는 월드/트랜스폼이 없다.)
	virtual bool CanPlaceAdditional(const AOJJ_Grid* Grid, FIntPoint Origin, int32 RotationSteps) const { return true; }

	// #182: 물(WaterArea∩분류 water) 셀 위 직접 배치 허용 여부. 기본 false(육상 전용) — 펌프만 override true.
	// CanPlaceMachine의 게이트 A(분류 IsCellConstructible)·B(WaterArea 점유) 면제 스위치이자
	// GetMachinePlacementLocation의 수면 Z 안착 스위치. 교집합(IsCellWater AND GetWaterSurfaceZAtCell)은
	// 그리드 측이 셀별로 보장하므로, 머신은 "물 위에 설 수 있는가"만 선언한다(CDO-safe — 인스턴스 상태 미사용).
	virtual bool CanStandOnWater() const { return false; }
	virtual bool OJJ_RequiresOccupancyOnlyRegistration() const { return false; }

	// 그리드 등록 성공 직후(배치 확정) 호출 — 자원 선점 등. 실제 인스턴스에서만 호출(CDO 아님).
	virtual void OnPlacedOnGrid(AOJJ_Grid* Grid, FIntPoint Origin, int32 RotationSteps) {}

	// 그리드에서 제거되기 직전 호출 — 자원 선점 해제 등. 실제 인스턴스에서만 호출.
	virtual void OnRemovedFromGrid();

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
	virtual bool PeekFirstOutputItem(FName& OutItemID) const;

	UFUNCTION(BlueprintCallable, Category = "Machine | Conveyor")
	virtual bool TryTakeFirstOutputItem(FName& OutItemID);

	UFUNCTION(BlueprintPure, Category = "Machine | Conveyor")
	virtual bool CanReceiveConveyorItem(FName ItemID, int32 Count = 1) const;

	UFUNCTION(BlueprintCallable, Category = "Machine | Conveyor")
	virtual bool ReceiveConveyorItem(FName ItemID, int32 Count = 1);

	UFUNCTION(BlueprintCallable, Category = "Machine | Debug")
	void UpdateDebugBufferText();

	void UpdateDebugTextFacingPlayer();
	
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

	UFUNCTION(BlueprintPure, Category = "Machine | Durability")
	int32 GetMaxRepairCostQty() const;

	UFUNCTION(BlueprintPure, Category = "Machine | Durability")
	int32 GetRepairCostQtyForCurrentDurability() const;

	UFUNCTION(BlueprintPure, Category = "Machine | Durability")
	FName GetRepairCostItemID() const { return RepairCostItemID; }

	UFUNCTION(BlueprintCallable, Category = "Machine | Durability")
	bool RepairUsingWarehouse();

	UFUNCTION(BlueprintCallable, Category = "Machine | Durability")
	void ApplyDurabilityDamage(float DamageAmount);

	UFUNCTION(BlueprintPure, Category = "Machine | Durability")
	float GetMaxDurability() const { return MaxDurability; }

	UFUNCTION(BlueprintPure, Category = "Machine | Durability")
	float GetCurrentDurability() const { return CurrentDurability; }
	bool RefundBufferedItemsToWarehouse();
	void GetSaveState(
		TMap<FName, int32>& OutInputInventory,
		TMap<FName, int32>& OutOutputBuffer,
		float& OutCurrentDurability) const;
	void ApplySaveState(
		const TMap<FName, int32>& InInputInventory,
		const TMap<FName, int32>& InOutputBuffer,
		float InCurrentDurability);
	
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

protected:
	virtual bool ShouldRefundBuffersToWarehouseOnRemoval() const { return true; }
};
