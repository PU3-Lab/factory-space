// Fill out your copyright notice in the Description page of Project Settings.

#include "FactoryAgentTTSPlaybackSubsystem.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "MediaPlayer.h"
#include "MediaSoundComponent.h"
#include "Wanted_Factory.h"

#include "FactoryAgentClientSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void UFactoryAgentTTSPlaybackSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ServerOriginUrl = TEXT("http://127.0.0.1:18000");
}

void UFactoryAgentTTSPlaybackSubsystem::Deinitialize()
{
	StopCurrent();

	if (MediaSoundComponent)
	{
		MediaSoundComponent->DestroyComponent();
		MediaSoundComponent = nullptr;
	}

	MediaPlayer = nullptr;
	Super::Deinitialize();
}

void UFactoryAgentTTSPlaybackSubsystem::PlayFromUrl(const FString& AudioUrl)
{
	if (AudioUrl.IsEmpty())
	{
		return;
	}

	FString ActiveOrigin = ServerOriginUrl;
	if (GetGameInstance())
	{
		UFactoryAgentClientSubsystem* AgentClient = GetGameInstance()->GetSubsystem<UFactoryAgentClientSubsystem>();
		if (AgentClient && !AgentClient->DefaultWebSocketUrl.IsEmpty())
		{
			FString WsUrl = AgentClient->DefaultWebSocketUrl;
			if (WsUrl.StartsWith(TEXT("ws://")))
			{
				WsUrl.ReplaceInline(TEXT("ws://"), TEXT("http://"));
			}
			else if (WsUrl.StartsWith(TEXT("wss://")))
			{
				WsUrl.ReplaceInline(TEXT("wss://"), TEXT("https://"));
			}

			const int32 ProtoIdx = WsUrl.Find(TEXT("://"));
			if (ProtoIdx != INDEX_NONE)
			{
				const int32 HostStartIdx = ProtoIdx + 3;
				const int32 PathIdx = WsUrl.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, HostStartIdx);
				ActiveOrigin = PathIdx != INDEX_NONE ? WsUrl.Left(PathIdx) : WsUrl;
			}
		}
	}

	FString FullUrl = AudioUrl;
	if (AudioUrl.StartsWith(TEXT("/")))
	{
		FullUrl = ActiveOrigin + AudioUrl;
	}
	else if (!AudioUrl.StartsWith(TEXT("http://")) && !AudioUrl.StartsWith(TEXT("https://")))
	{
		FullUrl = ActiveOrigin + TEXT("/") + AudioUrl;
	}

	UE_LOG(LogWanted_Factory, Log, TEXT("TTS PlayFromUrl: %s"), *FullUrl);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UFactoryAgentTTSPlaybackSubsystem::HandleDownloadComplete);
	Request->SetURL(FullUrl);
	Request->SetVerb(TEXT("GET"));
	Request->ProcessRequest();
}

void UFactoryAgentTTSPlaybackSubsystem::StopCurrent()
{
	if (MediaPlayer)
	{
		MediaPlayer->Close();
	}

	if (MediaSoundComponent && MediaSoundComponent->IsPlaying())
	{
		MediaSoundComponent->Stop();
	}
}

bool UFactoryAgentTTSPlaybackSubsystem::IsPlaybackAvailable() const
{
	return true;
}

void UFactoryAgentTTSPlaybackSubsystem::HandleDownloadComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogWanted_Factory, Warning, TEXT("TTS 오디오 다운로드 실패. (네트워크 오류)"));
		return;
	}

	const int32 ResponseCode = Response->GetResponseCode();
	if (ResponseCode != 200)
	{
		UE_LOG(LogWanted_Factory, Warning, TEXT("TTS 오디오 다운로드 실패. HTTP 응답 코드: %d"), ResponseCode);
		return;
	}

	const TArray<uint8>& AudioBytes = Response->GetContent();
	if (AudioBytes.Num() == 0)
	{
		UE_LOG(LogWanted_Factory, Warning, TEXT("TTS 오디오 다운로드 완료되었으나 빈 파일입니다."));
		return;
	}

	const FString RequestUrl = Request->GetURL();
	const int32 QueryIndex = RequestUrl.Find(TEXT("?"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	const FString CleanUrl = QueryIndex == INDEX_NONE ? RequestUrl : RequestUrl.Left(QueryIndex);
	FString FileName = FPaths::GetCleanFilename(CleanUrl);
	if (FileName.IsEmpty())
	{
		FileName = TEXT("tts_temp.mp3");
	}

	const FString SaveDirectory = FPaths::ProjectSavedDir() / TEXT("AgentTTS");
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*SaveDirectory))
	{
		PlatformFile.CreateDirectory(*SaveDirectory);
	}

	const FString SavePath = SaveDirectory / FileName;
	if (FFileHelper::SaveArrayToFile(AudioBytes, *SavePath))
	{
		UE_LOG(LogWanted_Factory, Log, TEXT("TTS 오디오 로컬 디스크 저장 성공: %s"), *SavePath);
		if (!PlaySavedAudioFile(SavePath))
		{
			UE_LOG(LogWanted_Factory, Warning, TEXT("TTS 오디오 재생 시작에 실패했습니다: %s"), *SavePath);
		}
	}
	else
	{
		UE_LOG(LogWanted_Factory, Warning, TEXT("TTS 오디오 파일 로컬 디스크 저장 실패: %s"), *SavePath);
	}
}

bool UFactoryAgentTTSPlaybackSubsystem::EnsurePlaybackObjects()
{
	if (!MediaPlayer)
	{
		MediaPlayer = NewObject<UMediaPlayer>(this);
	}

	if (MediaSoundComponent)
	{
		return MediaPlayer != nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			World = GI->GetWorld();
		}
	}
	if (!World)
	{
		return false;
	}

	AWorldSettings* WorldSettings = World->GetWorldSettings();
	if (!WorldSettings)
	{
		return false;
	}

	MediaSoundComponent = NewObject<UMediaSoundComponent>(WorldSettings);
	if (!MediaSoundComponent)
	{
		return false;
	}

	MediaSoundComponent->SetMediaPlayer(MediaPlayer);
	MediaSoundComponent->bAllowSpatialization = false;
	MediaSoundComponent->bIsUISound = true;
	MediaSoundComponent->RegisterComponentWithWorld(World);
	return true;
}

bool UFactoryAgentTTSPlaybackSubsystem::PlaySavedAudioFile(const FString& SavePath)
{
	if (!EnsurePlaybackObjects() || !MediaPlayer || !MediaSoundComponent)
	{
		return false;
	}

	FString NormalizedPath = SavePath;
	FPaths::MakeStandardFilename(NormalizedPath);

	MediaPlayer->Close();
	MediaSoundComponent->Stop();

	if (!MediaPlayer->OpenFile(NormalizedPath))
	{
		UE_LOG(LogWanted_Factory, Warning, TEXT("TTS MediaPlayer OpenFile 실패: %s"), *NormalizedPath);
		return false;
	}

	MediaSoundComponent->Activate(true);
	return MediaPlayer->Play();
}
