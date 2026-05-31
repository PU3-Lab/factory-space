// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "OJJ_GameMode.generated.h"

/**
 * OJJ TPS 플레이용 GameMode.
 * DefaultPawn = AOJJ_Player. 기본 APlayerController 사용 (입력은 Pawn에서 처리).
 *
 * 글로벌 기본값(DefaultEngine.ini)은 건드리지 않고, L_GridTest 레벨의
 * World Settings > GameMode Override에 지정해 팀 공용 기본값과 분리한다.
 */
UCLASS()
class WANTED_FACTORY_API AOJJ_GameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AOJJ_GameMode();
};
