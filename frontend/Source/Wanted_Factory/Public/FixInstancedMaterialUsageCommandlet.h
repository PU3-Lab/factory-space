#pragma once

#include "Commandlets/Commandlet.h"
#include "FixInstancedMaterialUsageCommandlet.generated.h"

UCLASS()
class WANTED_FACTORY_API UFixInstancedMaterialUsageCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UFixInstancedMaterialUsageCommandlet();

	virtual int32 Main(const FString& Params) override;
};
