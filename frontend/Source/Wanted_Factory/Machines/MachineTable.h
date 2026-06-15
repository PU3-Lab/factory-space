// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "UObject/Object.h"
#include "MachineTable.generated.h"

USTRUCT(BlueprintType)
struct FMachineTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName MachineType;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Level = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName CostType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 CostQty = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Durability = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Power = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 InputPortCnt = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 InputBufCnt = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 OutputPortCnt = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 OutputBufCnt = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Xlen = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Ylen = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UTexture2D> ImgAsset;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UStaticMesh> StaticMeshAsset;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UMaterialInterface> MaterialAsset;
};
