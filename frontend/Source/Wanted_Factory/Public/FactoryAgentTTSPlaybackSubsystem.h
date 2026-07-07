// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/IHttpRequest.h"
#include "FactoryAgentTTSPlaybackSubsystem.generated.h"

class UMediaPlayer;
class UMediaSoundComponent;

/**
 * UFactoryAgentTTSPlaybackSubsystem
 * 
 * 초보자용 설명:
 *     이 서브시스템은 백엔드 서버에서 생성한 AI 에이전트 음성 파일(MP3)의 다운로드와 로컬 저장을 담당합니다.
 *     에이전트 응답에 포함된 정적 URL을 인자로 받아 HTTP 요청으로 다운로드한 뒤, 로컬 세이브 폴더(Saved/AgentTTS)에 저장합니다.
 *     현재 Unreal 엔진에 런타임 MP3 재생 디코더 플러그인이 탑재되어 있지 않기 때문에, 오디오 재생 대신
 *     다운로드 완료 후 오디오 저장 경로를 로그로 출력하는 단계까지만 수행합니다.
 *     향후 MP3 디코더가 도입되면 UGameplayStatics::PlaySound2D 등으로 실제 사운드를 송출할 수 있습니다.
 */
UCLASS()
class WANTED_FACTORY_API UFactoryAgentTTSPlaybackSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * 지정된 에이전트 음성 URL로부터 오디오 파일을 다운로드하고 로컬 디스크 저장 작업을 시작합니다.
	 * 상대 경로(/tts/...)가 전달되면 기본 서버 주소(http://127.0.0.1:18000)를 접두사로 조인하여 다운로드합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Factory Agent|TTS")
	void PlayFromUrl(const FString& AudioUrl);

	/**
	 * 현재 재생 중인 TTS 음성을 중지합니다. (MVP에서는 오디오 디코더가 없어 관련 로그만 남깁니다.)
	 */
	UFUNCTION(BlueprintCallable, Category = "Factory Agent|TTS")
	void StopCurrent();

	/**
	 * 현재 런타임 오디오 디코딩/재생이 가능한 상태인지 여부를 반환합니다.
	 * MVP 단계에서는 MP3 재생 플러그인이 활성화되어 있지 않으므로 false를 리턴합니다.
	 */
	UFUNCTION(BlueprintPure, Category = "Factory Agent|TTS")
	bool IsPlaybackAvailable() const;

private:
	void HandleDownloadComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	bool EnsurePlaybackObjects();
	bool PlaySavedAudioFile(const FString& SavePath);

	FString ServerOriginUrl;

	UPROPERTY(Transient)
	TObjectPtr<UMediaPlayer> MediaPlayer;

	UPROPERTY(Transient)
	TObjectPtr<UMediaSoundComponent> MediaSoundComponent;
};
