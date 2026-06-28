// Fill out your copyright notice in the Description page of Project Settings.

#include "OJJ_HologramMaterialFactory.h"

#if WITH_EDITOR

#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionAbs.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionFrac.h"
#include "Materials/MaterialExpressionSine.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	void Conn(FExpressionInput& In, UMaterialExpression* Expr, int32 OutIndex = 0)
	{
		In.Expression = Expr;
		In.OutputIndex = OutIndex;
	}

	template <typename T>
	T* MakeExpr(UMaterial* Mat, int32 X, int32 Y)
	{
		return Cast<T>(UMaterialEditingLibrary::CreateMaterialExpression(Mat, T::StaticClass(), X, Y));
	}
}

UMaterial* UOJJ_HologramMaterialFactory::GenerateHologramBuildUpMaterial(bool bOverwriteExisting, bool bPathMode)
{
	const FString AssetName = bPathMode ? TEXT("M_Hologram_BuildUp_Path") : TEXT("M_Hologram_BuildUp");
	const FString PackageName = TEXT("/Game/OJJ/Materials/") + AssetName;
	const FString FullPath = PackageName + TEXT(".") + AssetName;

	bool bIsNew = false;
	UMaterial* Mat = LoadObject<UMaterial>(nullptr, *FullPath);
	if (Mat)
	{
		if (!bOverwriteExisting)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Hologram] %s 이미 존재 — 중단(bOverwriteExisting=true로 재생성)."), *PackageName);
			return nullptr;
		}
		UMaterialEditingLibrary::DeleteAllMaterialExpressions(Mat);
		UE_LOG(LogTemp, Display, TEXT("[Hologram] %s 노드 제거 후 재구성(overwrite)."), *PackageName);
	}
	else
	{
		UPackage* Package = CreatePackage(*PackageName);
		if (!Package) { UE_LOG(LogTemp, Error, TEXT("[Hologram] 패키지 생성 실패: %s"), *PackageName); return nullptr; }
		Mat = NewObject<UMaterial>(Package, *AssetName, RF_Public | RF_Standalone);
		if (!Mat) { UE_LOG(LogTemp, Error, TEXT("[Hologram] UMaterial 생성 실패")); return nullptr; }
		bIsNew = true;
	}

	Mat->BlendMode = BLEND_Translucent;
	Mat->SetShadingModel(MSM_Unlit);
	Mat->TwoSided = true;

	// --- 공통 파라미터 ---
	UMaterialExpressionScalarParameter* Progress = MakeExpr<UMaterialExpressionScalarParameter>(Mat, -1900, 0);
	Progress->ParameterName = TEXT("Progress"); Progress->DefaultValue = 0.0f;
	UMaterialExpressionScalarParameter* ScanWidth = MakeExpr<UMaterialExpressionScalarParameter>(Mat, -1900, 260);
	ScanWidth->ParameterName = TEXT("ScanlineWidth"); ScanWidth->DefaultValue = 0.04f;
	UMaterialExpressionScalarParameter* EmisBoost = MakeExpr<UMaterialExpressionScalarParameter>(Mat, -1900, 340);
	EmisBoost->ParameterName = TEXT("EmissiveBoost"); EmisBoost->DefaultValue = 8.0f;
	UMaterialExpressionVectorParameter* HoloColor = MakeExpr<UMaterialExpressionVectorParameter>(Mat, -1900, 420);
	HoloColor->ParameterName = TEXT("HologramColor"); HoloColor->DefaultValue = FLinearColor(0.05f, 0.3f, 1.0f, 1.0f);
	UMaterialExpressionVectorParameter* ScanColor = MakeExpr<UMaterialExpressionVectorParameter>(Mat, -1900, 520);
	ScanColor->ParameterName = TEXT("ScanlineColor"); ScanColor->DefaultValue = FLinearColor(0.91f, 0.565f, 0.165f, 1.0f);
	UMaterialExpressionScalarParameter* HoloOpacity = MakeExpr<UMaterialExpressionScalarParameter>(Mat, -1900, 600);
	HoloOpacity->ParameterName = TEXT("HologramOpacity"); HoloOpacity->DefaultValue = 0.5f;
	UMaterialExpressionScalarParameter* ScanDensity = MakeExpr<UMaterialExpressionScalarParameter>(Mat, -1900, 680);
	ScanDensity->ParameterName = TEXT("ScanDensity"); ScanDensity->DefaultValue = 30.0f;
	UMaterialExpressionScalarParameter* ScanSpeed = MakeExpr<UMaterialExpressionScalarParameter>(Mat, -1900, 760);
	ScanSpeed->ParameterName = TEXT("ScanSpeed"); ScanSpeed->DefaultValue = 1.0f;
	UMaterialExpressionScalarParameter* ScanIntensity = MakeExpr<UMaterialExpressionScalarParameter>(Mat, -1900, 840);
	ScanIntensity->ParameterName = TEXT("ScanIntensity"); ScanIntensity->DefaultValue = 0.6f;
	UMaterialExpressionScalarParameter* FlickerAmt = MakeExpr<UMaterialExpressionScalarParameter>(Mat, -1900, 920);
	FlickerAmt->ParameterName = TEXT("ScanFlicker"); FlickerAmt->DefaultValue = 0.25f;

	UMaterialExpressionWorldPosition* WorldPos = MakeExpr<UMaterialExpressionWorldPosition>(Mat, -1600, -260);

	// --- 마스크 좌표 Norm(0~1) + 주사선 좌표 StripeCoord (모드별) ---
	UMaterialExpression* Norm = nullptr;
	UMaterialExpression* StripeCoord = nullptr;
	if (!bPathMode)
	{
		// 머신: 월드 Z 마스크. NormZ = saturate((WorldPos.Z - MinZ)/(MaxZ - MinZ)).
		UMaterialExpressionScalarParameter* MinZ = MakeExpr<UMaterialExpressionScalarParameter>(Mat, -1900, 100);
		MinZ->ParameterName = TEXT("MinZ"); MinZ->DefaultValue = 0.0f;
		UMaterialExpressionScalarParameter* MaxZ = MakeExpr<UMaterialExpressionScalarParameter>(Mat, -1900, 180);
		MaxZ->ParameterName = TEXT("MaxZ"); MaxZ->DefaultValue = 100.0f;
		UMaterialExpressionComponentMask* WPZ = MakeExpr<UMaterialExpressionComponentMask>(Mat, -1400, -260);
		WPZ->R = false; WPZ->G = false; WPZ->B = true; WPZ->A = false;
		Conn(WPZ->Input, WorldPos);
		UMaterialExpressionSubtract* SubNum = MakeExpr<UMaterialExpressionSubtract>(Mat, -1200, -200);
		Conn(SubNum->A, WPZ); Conn(SubNum->B, MinZ);
		UMaterialExpressionSubtract* SubDen = MakeExpr<UMaterialExpressionSubtract>(Mat, -1200, -40);
		Conn(SubDen->A, MaxZ); Conn(SubDen->B, MinZ);
		UMaterialExpressionDivide* DivN = MakeExpr<UMaterialExpressionDivide>(Mat, -1000, -120);
		Conn(DivN->A, SubNum); Conn(DivN->B, SubDen);
		UMaterialExpressionSaturate* Sat = MakeExpr<UMaterialExpressionSaturate>(Mat, -820, -120);
		Conn(Sat->Input, DivN);
		Norm = Sat;
		StripeCoord = WPZ; // 월드 Z(세로 줄무늬)
	}
	else
	{
		// 컨베이어/파이프: 경로축 투영. NormP = saturate(dot(WorldPos - PathStart, PathDir)/PathLength).
		UMaterialExpressionVectorParameter* PathStart = MakeExpr<UMaterialExpressionVectorParameter>(Mat, -1900, 100);
		PathStart->ParameterName = TEXT("PathStart"); PathStart->DefaultValue = FLinearColor(0, 0, 0, 0);
		UMaterialExpressionVectorParameter* PathDir = MakeExpr<UMaterialExpressionVectorParameter>(Mat, -1900, 180);
		PathDir->ParameterName = TEXT("PathDir"); PathDir->DefaultValue = FLinearColor(1, 0, 0, 0);
		UMaterialExpressionScalarParameter* PathLen = MakeExpr<UMaterialExpressionScalarParameter>(Mat, -1900, -80);
		PathLen->ParameterName = TEXT("PathLength"); PathLen->DefaultValue = 100.0f;
		// 벡터 파라미터(float4)를 RGB(float3)로 — WorldPosition(float3)과 차원 정합.
		UMaterialExpressionComponentMask* StartRGB = MakeExpr<UMaterialExpressionComponentMask>(Mat, -1600, 100);
		StartRGB->R = true; StartRGB->G = true; StartRGB->B = true; StartRGB->A = false;
		Conn(StartRGB->Input, PathStart);
		UMaterialExpressionComponentMask* DirRGB = MakeExpr<UMaterialExpressionComponentMask>(Mat, -1600, 180);
		DirRGB->R = true; DirRGB->G = true; DirRGB->B = true; DirRGB->A = false;
		Conn(DirRGB->Input, PathDir);
		UMaterialExpressionSubtract* SubV = MakeExpr<UMaterialExpressionSubtract>(Mat, -1400, -120);
		Conn(SubV->A, WorldPos); Conn(SubV->B, StartRGB);
		UMaterialExpressionDotProduct* Dot = MakeExpr<UMaterialExpressionDotProduct>(Mat, -1200, -120);
		Conn(Dot->A, SubV); Conn(Dot->B, DirRGB);
		UMaterialExpressionDivide* DivP = MakeExpr<UMaterialExpressionDivide>(Mat, -1000, -120);
		Conn(DivP->A, Dot); Conn(DivP->B, PathLen);
		UMaterialExpressionSaturate* Sat = MakeExpr<UMaterialExpressionSaturate>(Mat, -820, -120);
		Conn(Sat->Input, DivP);
		Norm = Sat;
		StripeCoord = Sat; // 진행도(경로 방향 줄무늬)
	}

	// --- 경계 글로우: ScanGlow = saturate(1 - abs(Norm - Progress)/ScanlineWidth) ---
	UMaterialExpressionSubtract* SubNP = MakeExpr<UMaterialExpressionSubtract>(Mat, -620, 80);
	Conn(SubNP->A, Norm); Conn(SubNP->B, Progress);
	UMaterialExpressionAbs* AbsDist = MakeExpr<UMaterialExpressionAbs>(Mat, -460, 80);
	Conn(AbsDist->Input, SubNP);
	UMaterialExpressionDivide* DivScan = MakeExpr<UMaterialExpressionDivide>(Mat, -300, 80);
	Conn(DivScan->A, AbsDist); Conn(DivScan->B, ScanWidth);
	UMaterialExpressionOneMinus* OneMinusScan = MakeExpr<UMaterialExpressionOneMinus>(Mat, -140, 80);
	Conn(OneMinusScan->Input, DivScan);
	UMaterialExpressionSaturate* ScanGlow = MakeExpr<UMaterialExpressionSaturate>(Mat, 20, 80);
	Conn(ScanGlow->Input, OneMinusScan);

	// --- 가로 주사선(흐름): frac(StripeCoord*ScanDensity - Time*ScanSpeed) ---
	UMaterialExpressionTime* Time = MakeExpr<UMaterialExpressionTime>(Mat, -1200, 360);
	UMaterialExpressionMultiply* TimeSpeed = MakeExpr<UMaterialExpressionMultiply>(Mat, -1000, 360);
	Conn(TimeSpeed->A, Time); Conn(TimeSpeed->B, ScanSpeed);
	UMaterialExpressionMultiply* ZDensity = MakeExpr<UMaterialExpressionMultiply>(Mat, -1000, 260);
	Conn(ZDensity->A, StripeCoord); Conn(ZDensity->B, ScanDensity);
	UMaterialExpressionSubtract* ScanPhase = MakeExpr<UMaterialExpressionSubtract>(Mat, -820, 300);
	Conn(ScanPhase->A, ZDensity); Conn(ScanPhase->B, TimeSpeed);
	UMaterialExpressionFrac* Scanline = MakeExpr<UMaterialExpressionFrac>(Mat, -660, 300);
	Conn(Scanline->Input, ScanPhase);
	UMaterialExpressionMultiply* StripeDark = MakeExpr<UMaterialExpressionMultiply>(Mat, -500, 300);
	Conn(StripeDark->A, Scanline); Conn(StripeDark->B, ScanIntensity);
	UMaterialExpressionOneMinus* StripeFactor = MakeExpr<UMaterialExpressionOneMinus>(Mat, -340, 300);
	Conn(StripeFactor->Input, StripeDark);

	// --- 지지직: FlickerFactor = 1 + ScanFlicker*(sin(17t)*sin(5.3t)) ---
	UMaterialExpressionMultiply* T1 = MakeExpr<UMaterialExpressionMultiply>(Mat, -1000, 480);
	Conn(T1->A, Time); T1->ConstB = 17.0f;
	UMaterialExpressionSine* SinA = MakeExpr<UMaterialExpressionSine>(Mat, -840, 480);
	Conn(SinA->Input, T1);
	UMaterialExpressionMultiply* T2 = MakeExpr<UMaterialExpressionMultiply>(Mat, -1000, 580);
	Conn(T2->A, Time); T2->ConstB = 5.3f;
	UMaterialExpressionSine* SinB = MakeExpr<UMaterialExpressionSine>(Mat, -840, 580);
	Conn(SinB->Input, T2);
	UMaterialExpressionMultiply* FlickSig = MakeExpr<UMaterialExpressionMultiply>(Mat, -680, 520);
	Conn(FlickSig->A, SinA); Conn(FlickSig->B, SinB);
	UMaterialExpressionMultiply* FlickAmp = MakeExpr<UMaterialExpressionMultiply>(Mat, -520, 520);
	Conn(FlickAmp->A, FlickSig); Conn(FlickAmp->B, FlickerAmt);
	UMaterialExpressionAdd* FlickerFactor = MakeExpr<UMaterialExpressionAdd>(Mat, -360, 520);
	FlickerFactor->ConstA = 1.0f; Conn(FlickerFactor->B, FlickAmp);

	// --- 본체 Emissive × 주사선 × 지지직 ---
	UMaterialExpressionMultiply* BodyEmis = MakeExpr<UMaterialExpressionMultiply>(Mat, -120, 440);
	Conn(BodyEmis->A, HoloColor); BodyEmis->ConstB = 1.5f;
	UMaterialExpressionMultiply* BodyScan = MakeExpr<UMaterialExpressionMultiply>(Mat, 60, 420);
	Conn(BodyScan->A, BodyEmis); Conn(BodyScan->B, StripeFactor);
	UMaterialExpressionMultiply* BodyFinal = MakeExpr<UMaterialExpressionMultiply>(Mat, 240, 440);
	Conn(BodyFinal->A, BodyScan); Conn(BodyFinal->B, FlickerFactor);

	UMaterialExpressionMultiply* ScanEmis = MakeExpr<UMaterialExpressionMultiply>(Mat, 240, 600);
	Conn(ScanEmis->A, ScanColor); Conn(ScanEmis->B, EmisBoost);

	UMaterialExpressionLinearInterpolate* Emissive = MakeExpr<UMaterialExpressionLinearInterpolate>(Mat, 480, 500);
	Conn(Emissive->A, BodyFinal); Conn(Emissive->B, ScanEmis); Conn(Emissive->Alpha, ScanGlow);

	// --- Opacity = saturate(saturate((Norm-Progress)*100)*HologramOpacity + ScanGlow) ---
	UMaterialExpressionMultiply* AboveSharp = MakeExpr<UMaterialExpressionMultiply>(Mat, 240, -120);
	Conn(AboveSharp->A, SubNP); AboveSharp->ConstB = 100.0f;
	UMaterialExpressionSaturate* AboveMask = MakeExpr<UMaterialExpressionSaturate>(Mat, 400, -120);
	Conn(AboveMask->Input, AboveSharp);
	UMaterialExpressionMultiply* OpacityBody = MakeExpr<UMaterialExpressionMultiply>(Mat, 560, -120);
	Conn(OpacityBody->A, AboveMask); Conn(OpacityBody->B, HoloOpacity);
	UMaterialExpressionAdd* OpacitySum = MakeExpr<UMaterialExpressionAdd>(Mat, 720, -60);
	Conn(OpacitySum->A, OpacityBody); Conn(OpacitySum->B, ScanGlow);
	UMaterialExpressionSaturate* OpacityFinal = MakeExpr<UMaterialExpressionSaturate>(Mat, 880, -60);
	Conn(OpacityFinal->Input, OpacitySum);

	UMaterialEditingLibrary::ConnectMaterialProperty(Emissive, TEXT(""), MP_EmissiveColor);
	UMaterialEditingLibrary::ConnectMaterialProperty(OpacityFinal, TEXT(""), MP_Opacity);

	UMaterialEditingLibrary::LayoutMaterialExpressions(Mat);
	UMaterialEditingLibrary::RecompileMaterial(Mat);

	if (bIsNew)
	{
		FAssetRegistryModule::AssetCreated(Mat);
	}
	Mat->MarkPackageDirty();

	UE_LOG(LogTemp, Display,
		TEXT("[Hologram] %s %s (%s). 에디터서 확인 후 Ctrl+S → BuildController 슬롯 지정."),
		*PackageName, bIsNew ? TEXT("생성") : TEXT("재구성"),
		bPathMode ? TEXT("경로 길이 마스크") : TEXT("Z 마스크"));
	return Mat;
}

#endif // WITH_EDITOR
