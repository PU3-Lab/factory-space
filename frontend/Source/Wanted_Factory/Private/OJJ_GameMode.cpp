// Fill out your copyright notice in the Description page of Project Settings.


#include "OJJ_GameMode.h"

#include "OJJ_Player.h"

AOJJ_GameMode::AOJJ_GameMode()
{
	DefaultPawnClass = AOJJ_Player::StaticClass();
	// PlayerControllerClass는 엔진 기본(APlayerController) 유지 — 입력은 Pawn에서 바인딩.
}
