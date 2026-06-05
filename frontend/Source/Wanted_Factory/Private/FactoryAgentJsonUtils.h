#pragma once

#include "CoreMinimal.h"

class FJsonObject;

namespace FactoryAgentJsonUtils
{
	bool ParseJsonObject(const FString& Json, TSharedPtr<FJsonObject>& OutObject);
	FString WriteJsonObject(const TSharedPtr<FJsonObject>& JsonObject);
	FString GetStringField(const TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName);
	int32 GetIntegerField(const TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName, int32 DefaultValue);
	TSharedPtr<FJsonObject> GetObjectField(const TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName);
}
