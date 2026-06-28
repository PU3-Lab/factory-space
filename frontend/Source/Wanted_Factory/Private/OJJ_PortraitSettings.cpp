// Fill out your copyright notice in the Description page of Project Settings.

#include "OJJ_PortraitSettings.h"

UOJJ_PortraitSettings::UOJJ_PortraitSettings()
{
	// 기본 화이트리스트(인게임 레벨). DefaultGame.ini에 값이 있으면 그쪽이 우선한다.
	AutoSpawnLevels = { FName(TEXT("L_Planet")), FName(TEXT("L_GridTest")) };
}
