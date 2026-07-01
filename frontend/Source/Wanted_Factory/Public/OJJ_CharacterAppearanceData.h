// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OJJ_CharacterSelectionSubsystem.h" // EOJJ_CharacterType
#include "OJJ_CharacterAppearanceData.generated.h"

class USkeletalMesh;
class UAnimInstance;
class UAnimSequenceBase;
class UAnimMontage;

// [게임진입] 한 캐릭터 외형 = 스켈레탈 메시 + 애님 BP. 스켈레톤이 같으면 메시만 바꿔도 되지만 다를 수 있어
// ABP까지 세팅(코드가 둘 다 커버). 비우면 해당 항목은 스왑 스킵(BP 기본 메시/ABP 유지).
USTRUCT(BlueprintType)
struct FOJJ_CharacterAppearance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	TObjectPtr<USkeletalMesh> SkeletalMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	TSubclassOf<UAnimInstance> AnimClass = nullptr;

	// [게임진입] 캐릭터별 인트로 기상 몽타주. 비우면 BP_OJJ_Player 기본(Man) 유지.
	// Man 전용 MT_WakeUp을 Woman_Skeleton에서 재생하면 누우므로 캐릭터별로 교체.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character")
	TObjectPtr<UAnimMontage> GetUpMontage = nullptr;

	// [게임진입] 캐릭터별 점프 시퀀스(DefaultSlot 동적 재생). 비우면 BP_OJJ_Player 기본(Man) 유지.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character")
	TObjectPtr<UAnimSequenceBase> JumpAnim = nullptr;

	// [루트모션 올라서기] 캐릭터별 사다리 마무리(올라서기) 몽타주(DefaultSlot, 루트모션). 비우면 BP_OJJ_Player
	// 기본(Man, AM_Man_Ladder_Finish) 유지 — Man 몽타주를 Woman_Skeleton에서 재생하면 스켈레톤 mismatch로
	// 재생 실패→step-off 폴백(Finish 없음). Woman은 AM_Woman_Ladder_Finish를 할당해 캐릭터별 올라서기 루트모션 사용.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character")
	TObjectPtr<UAnimMontage> LadderFinishMontage = nullptr;

	// [게임진입] 캐릭터별 메시 RelativeLocation.Z 보정 사용 여부. true일 때만 ApplySelected가 Z를 덮는다.
	// false(기본)면 BP_OJJ_Player 기본 Z 유지 — Man처럼 BP 보정이 맞는 캐릭터는 미설정으로 두면 됨(회귀 0).
	// 0이 "미설정"인지 "의도된 0"인지 float만으론 구분 불가하므로 명시적 bool로 게이트(codex 리뷰 2026-06-25).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character")
	bool bOverrideMeshRelativeZ = false;

	// [게임진입] bOverrideMeshRelativeZ=true인 캐릭터의 메시 RelativeLocation.Z. 메시 피벗(발바닥 원점)이
	// 캐릭터마다 달라 단일 BP 값으론 발이 뜬다(Woman). X/Y는 BP 기본 유지, Z만 이 값으로 덮는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character", meta = (EditCondition = "bOverrideMeshRelativeZ"))
	float MeshRelativeZ = 0.f;

	// [수영] 이 캐릭터가 수영 부유 오프셋을 오버라이드할지. 캡슐은 남/여 동일하나 메시 키(머리 위치)가 달라
	// 같은 오프셋이면 작은 캐릭터(Woman)는 머리가 덜 나온다 → 캐릭터별 값 필요. false면 AOJJ_Player 기본값(Man) 유지.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	bool bOverrideSwimOffsets = false;

	// [수영] bOverrideSwimOffsets=true인 캐릭터의 수면 기준 캡슐중심 Z 오프셋. idle=머리 물 밖(클수록 더 뜸),
	// move=이동 시 잠김. AOJJ_Player.SwimIdleOffsetZ/SwimMoveOffsetZ를 ApplySelectedCharacterAppearance가 이 값으로 덮는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character", meta = (EditCondition = "bOverrideSwimOffsets"))
	float SwimIdleOffsetZ = -40.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character", meta = (EditCondition = "bOverrideSwimOffsets"))
	float SwimMoveOffsetZ = 0.f;
};

/**
 * [게임진입 1단계] 캐릭터 종류(EOJJ_CharacterType) → 외형(메시+ABP) 매핑 DataAsset.
 * JJ가 에디터에서 DA 인스턴스 생성 + Man(실제 메시)·Woman(WIP, 일단 플레이스홀더/빈 슬롯) 채움.
 * AOJJ_Player가 이 DA를 참조해 선택값에 맞춰 메시/ABP를 스왑한다(별도 BP_Woman 없이 단일 pawn 스왑).
 */
UCLASS(BlueprintType)
class WANTED_FACTORY_API UOJJ_CharacterAppearanceData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	TMap<EOJJ_CharacterType, FOJJ_CharacterAppearance> Appearances;
};
