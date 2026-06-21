// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OJJ_CharacterSelectionSubsystem.h" // EOJJ_CharacterType
#include "OJJ_CharacterAppearanceData.generated.h"

class USkeletalMesh;
class UAnimInstance;

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
