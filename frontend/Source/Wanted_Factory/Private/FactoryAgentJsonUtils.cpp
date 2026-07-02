#include "FactoryAgentJsonUtils.h"

#include "Dom/JsonObject.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace FactoryAgentJsonUtils
{
	bool ParseJsonObject(const FString& Json, TSharedPtr<FJsonObject>& OutObject)
	{
		if (Json.TrimStartAndEnd().IsEmpty())
		{
			OutObject = MakeShared<FJsonObject>();
			return true;
		}

		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
	}

	FString WriteJsonObject(const TSharedPtr<FJsonObject>& JsonObject)
	{
		if (!JsonObject.IsValid())
		{
			return TEXT("{}");
		}

		FString Output;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);
		FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
		return Output;
	}

	FString WritePrettyJsonObject(const TSharedPtr<FJsonObject>& JsonObject)
	{
		if (!JsonObject.IsValid())
		{
			return TEXT("{}");
		}

		FString Output;
		const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Output);
		FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
		return Output;
	}

	FString GetStringField(const TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName)
	{
		FString Value;
		if (JsonObject.IsValid())
		{
			JsonObject->TryGetStringField(FieldName, Value);
		}
		return Value;
	}

	int32 GetIntegerField(const TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName, int32 DefaultValue)
	{
		double Value = DefaultValue;
		if (JsonObject.IsValid())
		{
			JsonObject->TryGetNumberField(FieldName, Value);
		}
		return FMath::FloorToInt(Value);
	}

	TSharedPtr<FJsonObject> GetObjectField(const TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName)
	{
		if (!JsonObject.IsValid())
		{
			return nullptr;
		}

		const TSharedPtr<FJsonObject>* ObjectField = nullptr;
		if (!JsonObject->TryGetObjectField(FieldName, ObjectField) || ObjectField == nullptr)
		{
			return nullptr;
		}

		return *ObjectField;
	}
}
