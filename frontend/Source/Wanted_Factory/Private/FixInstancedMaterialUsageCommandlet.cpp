#include "FixInstancedMaterialUsageCommandlet.h"

#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Misc/PackageName.h"
#include "Resource/ResourceData.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
constexpr TCHAR DefaultMaterialPath[] = TEXT("/Game/Assets/Ore/copper/M_copper_ore.M_copper_ore");
constexpr TCHAR DefaultResourceTablePath[] = TEXT("/Game/DataTable/DT_ResourceData.DT_ResourceData");

bool SaveAssetPackage(UObject* Asset)
{
	if (!Asset)
	{
		return false;
	}

	UPackage* Package = Asset->GetOutermost();
	if (!Package)
	{
		return false;
	}

	const FString PackageName = Package->GetName();
	const FString PackageFilename = FPackageName::LongPackageNameToFilename(
		PackageName,
		FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	return UPackage::SavePackage(Package, Asset, *PackageFilename, SaveArgs);
}

bool FixMaterialUsageAndSave(UMaterial* Material)
{
	if (!Material)
	{
		return false;
	}

	bool bNeedsRecompile = false;
	const bool bUsageSet = Material->SetMaterialUsage(bNeedsRecompile, MATUSAGE_InstancedStaticMeshes);
	if (!bUsageSet)
	{
		return false;
	}

	Material->Modify();
	Material->MarkPackageDirty();
	return SaveAssetPackage(Material);
}
}

UFixInstancedMaterialUsageCommandlet::UFixInstancedMaterialUsageCommandlet()
{
	IsClient = false;
	IsEditor = true;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 UFixInstancedMaterialUsageCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	ParseCommandLine(*Params, Tokens, Switches, ParamVals);

	bool bHadFailure = false;

	if (Tokens.Num() == 0)
	{
		UDataTable* ResourceTable = LoadObject<UDataTable>(nullptr, DefaultResourceTablePath);
		if (!ResourceTable)
		{
			UE_LOG(LogTemp, Error, TEXT("[FixInstancedMaterialUsage] Failed to load resource table: %s"), DefaultResourceTablePath);
			return 1;
		}

		TSet<FString> SavedMaterialPaths;
		for (const TPair<FName, uint8*>& RowPair : ResourceTable->GetRowMap())
		{
			const FResourceData* Resource = reinterpret_cast<const FResourceData*>(RowPair.Value);
			if (!Resource || Resource->StaticMeshAsset.IsNull())
			{
				continue;
			}

			UStaticMesh* StaticMesh = Resource->StaticMeshAsset.IsValid()
				? Resource->StaticMeshAsset.Get()
				: Resource->StaticMeshAsset.LoadSynchronous();
			if (!StaticMesh)
			{
				UE_LOG(LogTemp, Warning, TEXT("[FixInstancedMaterialUsage] Failed to load static mesh for row: %s"), *RowPair.Key.ToString());
				bHadFailure = true;
				continue;
			}

			for (const FStaticMaterial& StaticMaterial : StaticMesh->GetStaticMaterials())
			{
				UMaterialInterface* MaterialInterface = StaticMaterial.MaterialInterface;
				UMaterial* Material = MaterialInterface ? MaterialInterface->GetMaterial() : nullptr;
				if (!Material)
				{
					continue;
				}

				const FString MaterialPath = Material->GetPathName();
				if (SavedMaterialPaths.Contains(MaterialPath))
				{
					continue;
				}

				if (!FixMaterialUsageAndSave(Material))
				{
					UE_LOG(LogTemp, Error, TEXT("[FixInstancedMaterialUsage] Failed to save material package: %s"), *MaterialPath);
					bHadFailure = true;
					continue;
				}

				SavedMaterialPaths.Add(MaterialPath);
				UE_LOG(LogTemp, Display, TEXT("[FixInstancedMaterialUsage] Saved %s"), *MaterialPath);
			}
		}

		return bHadFailure ? 1 : 0;
	}

	for (const FString& MaterialPath : Tokens)
	{
		UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
		if (!Material)
		{
			UE_LOG(LogTemp, Error, TEXT("[FixInstancedMaterialUsage] Failed to load material: %s"), *MaterialPath);
			bHadFailure = true;
			continue;
		}

		if (!FixMaterialUsageAndSave(Material))
		{
			UE_LOG(LogTemp, Error, TEXT("[FixInstancedMaterialUsage] Failed to save material package: %s"), *MaterialPath);
			bHadFailure = true;
			continue;
		}

		UE_LOG(LogTemp, Display, TEXT("[FixInstancedMaterialUsage] Saved %s"), *MaterialPath);
	}

	return bHadFailure ? 1 : 0;
}
