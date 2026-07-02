#include "MaterialGenerationRegistrySubsystem.h"

#include "FactorySaveSubsystem.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	FLinearColor ParseRuntimeVisualColorText(const FString& ColorText)
	{
		FString NormalizedColor = ColorText.TrimStartAndEnd();
		NormalizedColor.RemoveFromStart(TEXT("("));
		NormalizedColor.RemoveFromEnd(TEXT(")"));

		FLinearColor ParsedColor = FLinearColor::White;
		if (ParsedColor.InitFromString(NormalizedColor))
		{
			return ParsedColor;
		}

		return FLinearColor::White;
	}

	FString NormalizeRelativeAssetKey(const FString& AssetKey)
	{
		FString Normalized = AssetKey.TrimStartAndEnd();
		Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));
		Normalized.RemoveFromStart(TEXT("./"));
		Normalized.RemoveFromStart(TEXT("/"));
		return Normalized;
	}

	void AddRecipeInput(
		TArray<FFactoryMaterialRequestInput>& Inputs,
		const FName ItemId,
		const int32 Quantity)
	{
		if (ItemId.IsNone() || Quantity <= 0)
		{
			return;
		}

		FFactoryMaterialRequestInput& Input = Inputs.AddDefaulted_GetRef();
		Input.ItemId = ItemId;
		Input.Quantity = Quantity;
	}

	void FillRecipeInputs(
		FFactoryDynamicRecipeRecord& RecipeRecord,
		const TArray<FFactoryMaterialRequestInput>& Inputs)
	{
		if (Inputs.Num() > 0)
		{
			RecipeRecord.InputItem1 = Inputs[0].ItemId;
			RecipeRecord.InputQty1 = Inputs[0].Quantity;
		}

		if (Inputs.Num() > 1)
		{
			RecipeRecord.InputItem2 = Inputs[1].ItemId;
			RecipeRecord.InputQty2 = Inputs[1].Quantity;
		}

		if (Inputs.Num() > 2)
		{
			RecipeRecord.InputItem3 = Inputs[2].ItemId;
			RecipeRecord.InputQty3 = Inputs[2].Quantity;
		}
	}
}

void UMaterialGenerationRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency(UFactoryAgentClientSubsystem::StaticClass());

	if (UFactoryAgentClientSubsystem* AgentClient = GetGameInstance()->GetSubsystem<UFactoryAgentClientSubsystem>())
	{
		AgentClient->OnMaterialGenerationResponseReceived.AddDynamic(
			this,
			&UMaterialGenerationRegistrySubsystem::HandleMaterialGenerationResponse);
	}
}

void UMaterialGenerationRegistrySubsystem::Deinitialize()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFactoryAgentClientSubsystem* AgentClient = GameInstance->GetSubsystem<UFactoryAgentClientSubsystem>())
		{
			AgentClient->OnMaterialGenerationResponseReceived.RemoveDynamic(
				this,
				&UMaterialGenerationRegistrySubsystem::HandleMaterialGenerationResponse);
		}
	}

	Super::Deinitialize();
}

void UMaterialGenerationRegistrySubsystem::ExportSaveData(
	TArray<FFactoryDynamicMaterialRecord>& OutMaterials,
	TArray<FFactoryDynamicRecipeRecord>& OutRecipes) const
{
	OutMaterials = DynamicMaterials;
	OutRecipes = DynamicRecipes;
}

void UMaterialGenerationRegistrySubsystem::ImportSaveData(
	const TArray<FFactoryDynamicMaterialRecord>& InMaterials,
	const TArray<FFactoryDynamicRecipeRecord>& InRecipes)
{
	DynamicMaterials = InMaterials;
	DynamicRecipes = InRecipes;
	ThumbnailTextureCache.Reset();
}

bool UMaterialGenerationRegistrySubsystem::FindDynamicMaterialRecord(
	FName MaterialId,
	FFactoryDynamicMaterialRecord& OutRecord) const
{
	if (MaterialId.IsNone())
	{
		return false;
	}

	const FFactoryDynamicMaterialRecord* FoundRecord = DynamicMaterials.FindByPredicate(
		[MaterialId](const FFactoryDynamicMaterialRecord& ExistingRecord)
		{
			return ExistingRecord.MaterialId == MaterialId;
		});
	if (!FoundRecord)
	{
		return false;
	}

	OutRecord = *FoundRecord;
	return true;
}

FText UMaterialGenerationRegistrySubsystem::GetMaterialDisplayText(FName MaterialId) const
{
	FFactoryDynamicMaterialRecord MaterialRecord;
	if (!FindDynamicMaterialRecord(MaterialId, MaterialRecord))
	{
		return FText::GetEmpty();
	}

	if (!MaterialRecord.DisplayName.TrimStartAndEnd().IsEmpty())
	{
		return FText::FromString(MaterialRecord.DisplayName);
	}

	if (!MaterialRecord.Name.TrimStartAndEnd().IsEmpty())
	{
		return FText::FromString(MaterialRecord.Name);
	}

	if (!MaterialRecord.RowName.TrimStartAndEnd().IsEmpty())
	{
		return FText::FromString(MaterialRecord.RowName);
	}

	return FText::FromName(MaterialId);
}

UTexture2D* UMaterialGenerationRegistrySubsystem::GetMaterialThumbnailTexture(FName MaterialId)
{
	if (MaterialId.IsNone())
	{
		return nullptr;
	}

	if (TObjectPtr<UTexture2D>* CachedTexture = ThumbnailTextureCache.Find(MaterialId))
	{
		return CachedTexture->Get();
	}

	FFactoryDynamicMaterialRecord MaterialRecord;
	if (!FindDynamicMaterialRecord(MaterialId, MaterialRecord))
	{
		return nullptr;
	}

	UTexture2D* LoadedTexture = LoadTextureFromMaterialRecord(MaterialRecord);
	if (!LoadedTexture)
	{
		LoadedTexture = CreateGeneratedThumbnailTexture(MaterialId, MaterialRecord);
	}
	ThumbnailTextureCache.Add(MaterialId, LoadedTexture);
	return LoadedTexture;
}

bool UMaterialGenerationRegistrySubsystem::FindFirstRuntimeRecipe(
	const FName MachineType,
	const TMap<FName, int32>& InputInventory,
	FRecipeTable& OutRecipe) const
{
	for (const FFactoryDynamicRecipeRecord& RecipeRecord : DynamicRecipes)
	{
		if (RecipeRecord.MachineType != MachineType)
		{
			continue;
		}

		const auto HasRequiredQuantity = [&InputInventory](const FName ItemId, const int32 Quantity)
		{
			if (ItemId.IsNone() || Quantity <= 0)
			{
				return true;
			}

			return InputInventory.FindRef(ItemId) >= Quantity;
		};

		if (!HasRequiredQuantity(RecipeRecord.InputItem1, RecipeRecord.InputQty1) ||
			!HasRequiredQuantity(RecipeRecord.InputItem2, RecipeRecord.InputQty2) ||
			!HasRequiredQuantity(RecipeRecord.InputItem3, RecipeRecord.InputQty3))
		{
			continue;
		}

		OutRecipe = FRecipeTable();
		OutRecipe.MachineType = RecipeRecord.MachineType;
		OutRecipe.InputItem1 = RecipeRecord.InputItem1;
		OutRecipe.InputQty1 = RecipeRecord.InputQty1;
		OutRecipe.InputItem2 = RecipeRecord.InputItem2;
		OutRecipe.InputQty2 = RecipeRecord.InputQty2;
		OutRecipe.InputItem3 = RecipeRecord.InputItem3;
		OutRecipe.InputQty3 = RecipeRecord.InputQty3;
		OutRecipe.OutputItem1 = RecipeRecord.OutputItem1;
		OutRecipe.OutputQty1 = RecipeRecord.OutputQty1;
		OutRecipe.OutputItem2 = RecipeRecord.OutputItem2;
		OutRecipe.OutputQty2 = RecipeRecord.OutputQty2;
		OutRecipe.CraftingTime = RecipeRecord.CraftingTime;
		return true;
	}

	return false;
}

void UMaterialGenerationRegistrySubsystem::HandleMaterialGenerationResponse(
	const FFactoryMaterialGenerationResponse& Response)
{
	UFactoryAgentClientSubsystem* AgentClient = GetGameInstance()->GetSubsystem<UFactoryAgentClientSubsystem>();
	if (!AgentClient)
	{
		return;
	}

	FFactoryPendingMaterialGenerationRequest PendingRequest;
	if (!AgentClient->ConsumePendingMaterialGenerationRequest(Response.RequestId, PendingRequest))
	{
		return;
	}

	RegisterDynamicRecipe(PendingRequest, Response);
	RegisterDynamicMaterial(Response);

	if (UFactorySaveSubsystem* SaveSubsystem = GetGameInstance()->GetSubsystem<UFactorySaveSubsystem>())
	{
		SaveSubsystem->SaveCurrentGame();
	}
}

void UMaterialGenerationRegistrySubsystem::RegisterDynamicMaterial(
	const FFactoryMaterialGenerationResponse& Response)
{
	if (Response.MaterialId.IsEmpty())
	{
		return;
	}

	FFactoryDynamicMaterialRecord MaterialRecord;
	MaterialRecord.MaterialId = FName(Response.MaterialId);
	MaterialRecord.Name = Response.Name;
	MaterialRecord.State = Response.State;
	MaterialRecord.RowName = Response.RowName;
	MaterialRecord.Form = Response.Form;
	MaterialRecord.Substance = Response.Substance;
	MaterialRecord.MaterialType = Response.MaterialType;
	MaterialRecord.Shape = Response.Shape;
	MaterialRecord.DisplayName = Response.DisplayName;
	MaterialRecord.VisualColor = Response.VisualColor;
	MaterialRecord.VisualAssetKey = Response.VisualAssetKey;
	MaterialRecord.TextureAssetKey = Response.TextureAssetKey;
	MaterialRecord.ThumbnailAssetKey = Response.ThumbnailAssetKey;
	MaterialRecord.FallbackIcon = Response.FallbackIcon;
	MaterialRecord.Message = Response.Message;

	const int32 ExistingIndex = DynamicMaterials.IndexOfByPredicate(
		[&MaterialRecord](const FFactoryDynamicMaterialRecord& ExistingRecord)
		{
			return ExistingRecord.MaterialId == MaterialRecord.MaterialId;
		});

	if (ExistingIndex != INDEX_NONE)
	{
		DynamicMaterials[ExistingIndex] = MaterialRecord;
		return;
	}

	DynamicMaterials.Add(MaterialRecord);
	ThumbnailTextureCache.Remove(MaterialRecord.MaterialId);
}

void UMaterialGenerationRegistrySubsystem::RegisterDynamicRecipe(
	const FFactoryPendingMaterialGenerationRequest& Request,
	const FFactoryMaterialGenerationResponse& Response)
{
	FFactoryDynamicRecipeRecord RecipeRecord;
	RecipeRecord.MachineType = TEXT("Synthesizer");
	RecipeRecord.RecipeKey = BuildRecipeKey(RecipeRecord.MachineType, Request.Inputs);
	RecipeRecord.CraftingTime = 1.0f;

	TArray<FFactoryMaterialRequestInput> SortedInputs = Request.Inputs;
	SortedInputs.Sort([](const FFactoryMaterialRequestInput& Left, const FFactoryMaterialRequestInput& Right)
	{
		if (Left.ItemId != Right.ItemId)
		{
			return Left.ItemId.LexicalLess(Right.ItemId);
		}

		return Left.Quantity < Right.Quantity;
	});
	FillRecipeInputs(RecipeRecord, SortedInputs);

	if (Response.ResultType.Equals(TEXT("existing_recipe"), ESearchCase::IgnoreCase))
	{
		if (Response.Outputs.Num() > 0)
		{
			RecipeRecord.OutputItem1 = Response.Outputs[0].ItemId;
			RecipeRecord.OutputQty1 = Response.Outputs[0].Quantity;
		}
		if (Response.Outputs.Num() > 1)
		{
			RecipeRecord.OutputItem2 = Response.Outputs[1].ItemId;
			RecipeRecord.OutputQty2 = Response.Outputs[1].Quantity;
		}
	}
	else if (!Response.MaterialId.IsEmpty())
	{
		RecipeRecord.OutputItem1 = FName(Response.MaterialId);
		RecipeRecord.OutputQty1 = 1;
	}

	if (RecipeRecord.OutputItem1.IsNone() && RecipeRecord.OutputItem2.IsNone())
	{
		return;
	}

	const int32 ExistingIndex = DynamicRecipes.IndexOfByPredicate(
		[&RecipeRecord](const FFactoryDynamicRecipeRecord& ExistingRecord)
		{
			return ExistingRecord.RecipeKey == RecipeRecord.RecipeKey;
		});

	if (ExistingIndex != INDEX_NONE)
	{
		DynamicRecipes[ExistingIndex] = RecipeRecord;
		return;
	}

	DynamicRecipes.Add(RecipeRecord);
}

FString UMaterialGenerationRegistrySubsystem::BuildRecipeKey(
	const FName MachineType,
	const TArray<FFactoryMaterialRequestInput>& Inputs)
{
	TArray<FFactoryMaterialRequestInput> SortedInputs = Inputs;
	SortedInputs.Sort([](const FFactoryMaterialRequestInput& Left, const FFactoryMaterialRequestInput& Right)
	{
		if (Left.ItemId != Right.ItemId)
		{
			return Left.ItemId.LexicalLess(Right.ItemId);
		}

		return Left.Quantity < Right.Quantity;
	});

	TArray<FString> Parts;
	Parts.Reserve(SortedInputs.Num() + 1);
	Parts.Add(MachineType.ToString());

	for (const FFactoryMaterialRequestInput& Input : SortedInputs)
	{
		if (Input.ItemId.IsNone() || Input.Quantity <= 0)
		{
			continue;
		}

		Parts.Add(FString::Printf(TEXT("%s:%d"), *Input.ItemId.ToString(), Input.Quantity));
	}

	return FString::Join(Parts, TEXT("|"));
}

UTexture2D* UMaterialGenerationRegistrySubsystem::LoadTextureFromMaterialRecord(
	const FFactoryDynamicMaterialRecord& MaterialRecord)
{
	TArray<FString> AssetKeys;
	AssetKeys.Add(MaterialRecord.ThumbnailAssetKey);
	AssetKeys.Add(MaterialRecord.TextureAssetKey);
	AssetKeys.Add(MaterialRecord.VisualAssetKey);
	AssetKeys.Add(MaterialRecord.FallbackIcon);

	const FString ProjectRootDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), TEXT("../"));
	const FString BackendMaterialsDir = FPaths::Combine(ProjectRootDir, TEXT("backend"), TEXT("var"), TEXT("materials"));

	for (const FString& RawAssetKey : AssetKeys)
	{
		const FString AssetKey = RawAssetKey.TrimStartAndEnd();
		if (AssetKey.IsEmpty())
		{
			continue;
		}

		if (AssetKey.StartsWith(TEXT("/Game/")) || AssetKey.StartsWith(TEXT("/Engine/")))
		{
			if (UTexture2D* AssetTexture = LoadObject<UTexture2D>(nullptr, *AssetKey))
			{
				return AssetTexture;
			}
		}

		const FString RelativeAssetKey = NormalizeRelativeAssetKey(AssetKey);
		TArray<FString> CandidatePaths;
		CandidatePaths.Add(AssetKey);
		CandidatePaths.Add(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), RelativeAssetKey));
		CandidatePaths.Add(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir(), RelativeAssetKey));
		CandidatePaths.Add(FPaths::Combine(BackendMaterialsDir, RelativeAssetKey));

		for (const FString& CandidatePath : CandidatePaths)
		{
			if (FPaths::FileExists(CandidatePath))
			{
				return FImageUtils::ImportFileAsTexture2D(CandidatePath);
			}
		}
	}

	return nullptr;
}

FLinearColor UMaterialGenerationRegistrySubsystem::ResolveMaterialPreviewColor(
	const FFactoryDynamicMaterialRecord& MaterialRecord) const
{
	return ParseRuntimeVisualColorText(MaterialRecord.VisualColor);
}

UTexture2D* UMaterialGenerationRegistrySubsystem::CreateGeneratedThumbnailTexture(
	FName MaterialId,
	const FFactoryDynamicMaterialRecord& MaterialRecord)
{
	UTexture2D* Texture = UTexture2D::CreateTransient(64, 64, PF_B8G8R8A8);
	if (!Texture || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.Num() == 0)
	{
		return nullptr;
	}

	const FLinearColor PreviewColor = ResolveMaterialPreviewColor(MaterialRecord);
	const FColor FillColor = PreviewColor.ToFColor(true);
	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	if (!Data)
	{
		Mip.BulkData.Unlock();
		return nullptr;
	}

	FMemory::Memset(Data, 0, 64 * 64 * sizeof(FColor));
	FColor* Pixels = static_cast<FColor*>(Data);
	for (int32 Index = 0; Index < 64 * 64; ++Index)
	{
		Pixels[Index] = FillColor;
	}

	Mip.BulkData.Unlock();
	Texture->SRGB = true;
	Texture->NeverStream = true;
	Texture->UpdateResource();
	Texture->Rename(*FString::Printf(TEXT("MGThumb_%s"), *MaterialId.ToString()), this);
	return Texture;
}
