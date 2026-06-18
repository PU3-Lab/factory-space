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
	(void)Params;
	UE_LOG(
		LogTemp,
		Error,
		TEXT("[FixInstancedMaterialUsage] Disabled. This commandlet directly rewrites material assets and caused invalid material saves. Restore the affected materials first and use an editor-driven resave workflow for future fixes."));
	return 1;
}
