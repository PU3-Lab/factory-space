// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI_MainHUD.generated.h"

UCLASS()
class WANTED_FACTORY_API UUI_MainHUD : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	// 재난 일수
	//UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_DisasterDay;

	// 인게임 시간
	//UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_InGameTime;

	// 날씨
	//UPROPERTY(meta = (BindWidget)) class UImage* IMG_WeatherIcon;
	//UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_WeatherStatus;

	// 퀘스트
	//UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_QuestTitle;
	//UPROPERTY(meta = (BindWidget)) class UTextBlock* TXT_QuestProgress;
};

