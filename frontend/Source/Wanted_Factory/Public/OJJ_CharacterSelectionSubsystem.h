// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OJJ_CharacterSelectionSubsystem.generated.h"

// [게임진입] 선택 가능한 캐릭터 외형 종류 — 외형만 다르고 스폰 로직은 동일(메시/ABP 스왑). Woman은 WIP.
UENUM(BlueprintType)
enum class EOJJ_CharacterType : uint8
{
	Man   UMETA(DisplayName = "Man"),
	Woman UMETA(DisplayName = "Woman"),
};

/**
 * [게임진입 1단계] 캐릭터 선택값 보존 — GameInstanceSubsystem이라 레벨 트래블(메뉴→인게임)을 넘어 유지된다.
 * 시작화면 캐릭터선택(2단계 위젯)이 SetSelectedCharacter로 기록, L_Planet 진입 시 AOJJ_Player가
 * GetSelectedCharacter를 읽어 메시/ABP를 스왑한다. 기존 7개 GameInstanceSubsystem 패턴과 동일.
 */
UCLASS()
class WANTED_FACTORY_API UOJJ_CharacterSelectionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Character Selection")
	void SetSelectedCharacter(EOJJ_CharacterType InType) { SelectedCharacter = InType; }

	UFUNCTION(BlueprintPure, Category = "Character Selection")
	EOJJ_CharacterType GetSelectedCharacter() const { return SelectedCharacter; }

private:
	// 선택된 캐릭터(기본 Man). 메뉴 미경유 직부팅(현재 L_Planet 직행)에서도 Man으로 정상 동작.
	UPROPERTY(VisibleAnywhere, Category = "Character Selection")
	EOJJ_CharacterType SelectedCharacter = EOJJ_CharacterType::Man;
};
