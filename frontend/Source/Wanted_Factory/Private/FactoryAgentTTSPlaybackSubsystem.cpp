// Fill out your copyright notice in the Description page of Project Settings.

#include "FactoryAgentTTSPlaybackSubsystem.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "Wanted_Factory.h"

#include "FactoryAgentClientSubsystem.h"
#include "Engine/GameInstance.h"

void UFactoryAgentTTSPlaybackSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ServerOriginUrl = TEXT("http://127.0.0.1:18000");
}

void UFactoryAgentTTSPlaybackSubsystem::Deinitialize()
{
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

			int32 ProtoIdx = WsUrl.Find(TEXT("://"));
			if (ProtoIdx != INDEX_NONE)
			{
				int32 PathIdx;
				if (WsUrl.FindChar(TEXT('/'), PathIdx) && PathIdx > ProtoIdx + 2)
				{
					ActiveOrigin = WsUrl.Left(PathIdx);
				}
				else
				{
					ActiveOrigin = WsUrl;
				}
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
	UE_LOG(LogWanted_Factory, Log, TEXT("StopCurrent TTS requested. (MP3 playback not supported yet)"));
}

bool UFactoryAgentTTSPlaybackSubsystem::IsPlaybackAvailable() const
{
	return false;
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

	FString RequestUrl = Request->GetURL();
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
		UE_LOG(LogWanted_Factory, Warning, TEXT("Warning: 런타임 MP3 재생 디코더 플러그인이 아직 활성화되지 않았습니다. 재생을 생략합니다."));
	}
	else
	{
		UE_LOG(LogWanted_Factory, Warning, TEXT("TTS 오디오 파일 로컬 디스크 저장 실패: %s"), *SavePath);
	}
}
