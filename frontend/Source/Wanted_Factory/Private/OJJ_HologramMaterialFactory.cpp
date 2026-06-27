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
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionAbs.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionIf.h"
#include "Materials/MaterialExpressionConstant.h"
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

UMaterial* UOJJ_HologramMaterialFactory::GenerateHologramBuildUpMaterial(bool bOverwriteExisting)
{
	const FString PackageName = TEXT("/Game/OJJ/Materials/M_Hologram_BuildUp");
	const FString AssetName = TEXT("M_Hologram_BuildUp");
	const FString FullPath = PackageName + TEXT(".") + AssetName;

	bool bIsNew = false;
	UMaterial* Mat = LoadObject<UMaterial>(nullptr, *FullPath);
	if (Mat)
	{
		if (!bOverwriteExisting)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Hologram] %s 이미 존재 — 중단. 재생성하려면 bOverwriteExisting=true."), *PackageName);
			return nullptr;
		}
		UMaterialEditingLibrary::DeleteAllMaterialExpressions(Mat);
		UE_LOG(LogTemp, Display, TEXT("[Hologram] %s 기존 노드 제거 후 재구성(overwrite)."), *PackageName);
	}
	else
	{
		UPackage* Package = CreatePackage(*PackageName);
		if (!Package)
		{
			UE_LOG(LogTemp, Error, TEXT("[Hologram] 패키지 생성 실패: %s"), *PackageName);
			return nullptr;
		}
		Mat = NewObject<UMaterial>(Package, *AssetName, RF_Public | RF_Standalone);
		if (!Mat)
		{
			UE_LOG(LogTemp, Error, TEXT("[Hologram] UMaterial 생성 실패"));
			return nullptr;
		}
		bIsNew = true;
	}

	// 머티리얼 도메인: Translucent + Unlit + TwoSided (반투명 — 위는 반투명 홀로그램, 아래 완전 투명).
	Mat->BlendMode = BLEND_Translucent;
	Mat->SetShadingModel(MSM_Unlit);
	Mat->TwoSided = true;

	// --- 파라미터 노드 ---
	UMaterialExpressionScalarParameter* Progress = MakeExpr<UMaterialExpressionScalarParameter>(Mat, -1900, 0);
	Progress->ParameterName = TEXT("Progress"); Progress->DefaultValue = 0.0f;
	UMaterialExpressionScalarParameter* MinZ = MakeExpr<UMaterialExpressionScalarParameter>(Mat, -1900, 100);
	MinZ->ParameterName = TEXT("MinZ"); MinZ->DefaultValue = 0.0f;
	UMaterialExpressionScalarParameter* MaxZ = MakeExpr<UMaterialExpressionScalarParameter>(Mat, -1900, 180);
	MaxZ->ParameterName = TEXT("MaxZ"); MaxZ->DefaultValue = 100.0f;
	UMaterialExpressionScalarParameter* ScanWidth = MakeExpr<UMaterialExpressionScalarParameter>(Mat, -1900, 260);
	ScanWidth->ParameterName = TEXT("ScanlineWidth"); ScanWidth->DefaultValue = 0.04f;
	UMaterialExpressionScalarParameter* EmisBoost = MakeExpr<UMaterialExpressionScalarParameter>(Mat, -1900, 340);
	EmisBoost->ParameterName = TEXT("EmissiveBoost"); EmisBoost->DefaultValue = 8.0f;
	// 진한 파란색(하늘색/시안 아님).
	UMaterialExpressionVectorParameter* HoloColor = MakeExpr<UMaterialExpressionVectorParameter>(Mat, -1900, 420);
	HoloColor->ParameterName = TEXT("HologramColor"); HoloColor->DefaultValue = FLinearColor(0.05f, 0.3f, 1.0f, 1.0f);
	UMaterialExpressionVectorParameter* ScanColor = MakeExpr<UMaterialExpressionVectorParameter>(Mat, -1900, 520);
	ScanColor->ParameterName = TEXT("ScanlineColor"); ScanColor->DefaultValue = FLinearColor(0.91f, 0.565f, 0.165f, 1.0f);
	// 반투명 정도(위 본체 알파).
	UMaterialExpressionScalarParameter* HoloOpacity = MakeExpr<UMaterialExpressionScalarParameter>(Mat, -1900, 600);
	HoloOpacity->ParameterName = TEXT("HologramOpacity"); HoloOpacity->DefaultValue = 0.5f;
	// 가로 주사선 튜닝(더 뚜렷/강하게).
	UMaterialExpressionScalarParameter* ScanDensity = MakeExpr<UMaterialExpressionScalarParameter>(Mat, -1900, 680);
	ScanDensity->ParameterName = TEXT("ScanDensity"); ScanDensity->DefaultValue = 30.0f;
	UMaterialExpressionScalarParameter* ScanSpeed = MakeExpr<UMaterialExpressionScalarParameter>(Mat, -1900, 760);
	ScanSpeed->ParameterName = TEXT("ScanSpeed"); ScanSpeed->DefaultValue = 1.0f;
	UMaterialExpressionScalarParameter* ScanIntensity = MakeExpr<UMaterialExpressionScalarParameter>(Mat, -1900, 840);
	ScanIntensity->ParameterName = TEXT("ScanIntensity"); ScanIntensity->DefaultValue = 0.6f;
	UMaterialExpressionScalarParameter* FlickerAmt = MakeExpr<UMaterialExpressionScalarParameter>(Mat, -1900, 920);
	FlickerAmt->ParameterName = TEXT("ScanFlicker"); FlickerAmt->DefaultValue = 0.25f;

	// --- WorldPos.Z ---
	UMaterialExpressionWorldPosition* WorldPos = MakeExpr<UMaterialExpressionWorldPosition>(Mat, -1600, -200);
	UMaterialExpressionComponentMask* WorldPosZ = MakeExpr<UMaterialExpressionComponentMask>(Mat, -1400, -200);
	WorldPosZ->R = false; WorldPosZ->G = false; WorldPosZ->B = true; WorldPosZ->A = false;
	Conn(WorldPosZ->Input, WorldPos);

	// --- NormZ = saturate((WorldPosZ - MinZ)/(MaxZ - MinZ)) ---
	UMaterialExpressionSubtract* SubNum = MakeExpr<UMaterialExpressionSubtract>(Mat, -1200, -160);
	Conn(SubNum->A, WorldPosZ); Conn(SubNum->B, MinZ);
	UMaterialExpressionSubtract* SubDen = MakeExpr<UMaterialExpressionSubtract>(Mat, -1200, 0);
	Conn(SubDen->A, MaxZ); Conn(SubDen->B, MinZ);
	UMaterialExpressionDivide* DivNorm = MakeExpr<UMaterialExpressionDivide>(Mat, -1000, -80);
	Conn(DivNorm->A, SubNum); Conn(DivNorm->B, SubDen);
	UMaterialExpressionSaturate* NormZ = MakeExpr<UMaterialExpressionSaturate>(Mat, -820, -80);
	Conn(NormZ->Input, DivNorm);

	// --- 경계 글로우: ScanGlow = saturate(1 - abs(NormZ - Progress)/ScanlineWidth) ---
	UMaterialExpressionSubtract* SubNP = MakeExpr<UMaterialExpressionSubtract>(Mat, -620, 80);
	Conn(SubNP->A, NormZ); Conn(SubNP->B, Progress);
	UMaterialExpressionAbs* AbsDist = MakeExpr<UMaterialExpressionAbs>(Mat, -460, 80);
	Conn(AbsDist->Input, SubNP);
	UMaterialExpressionDivide* DivScan = MakeExpr<UMaterialExpressionDivide>(Mat, -300, 80);
	Conn(DivScan->A, AbsDist); Conn(DivScan->B, ScanWidth);
	UMaterialExpressionOneMinus* OneMinusScan = MakeExpr<UMaterialExpressionOneMinus>(Mat, -140, 80);
	Conn(OneMinusScan->Input, DivScan);
	UMaterialExpressionSaturate* ScanGlow = MakeExpr<UMaterialExpressionSaturate>(Mat, 20, 80);
	Conn(ScanGlow->Input, OneMinusScan);

	// --- 가로 주사선(흐름): Scanline = frac(WorldPosZ*ScanDensity - Time*ScanSpeed) ---
	UMaterialExpressionTime* Time = MakeExpr<UMaterialExpressionTime>(Mat, -1200, 360);
	UMaterialExpressionMultiply* TimeSpeed = MakeExpr<UMaterialExpressionMultiply>(Mat, -1000, 360);
	Conn(TimeSpeed->A, Time); Conn(TimeSpeed->B, ScanSpeed);
	UMaterialExpressionMultiply* ZDensity = MakeExpr<UMaterialExpressionMultiply>(Mat, -1000, 260);
	Conn(ZDensity->A, WorldPosZ); Conn(ZDensity->B, ScanDensity);
	UMaterialExpressionSubtract* ScanPhase = MakeExpr<UMaterialExpressionSubtract>(Mat, -820, 300);
	Conn(ScanPhase->A, ZDensity); Conn(ScanPhase->B, TimeSpeed);
	UMaterialExpressionFrac* Scanline = MakeExpr<UMaterialExpressionFrac>(Mat, -660, 300);
	Conn(Scanline->Input, ScanPhase);
	// StripeFactor = 1 - ScanIntensity*Scanline (줄 부분 어두워짐 — ScanIntensity↑로 더 뚜렷).
	UMaterialExpressionMultiply* StripeDark = MakeExpr<UMaterialExpressionMultiply>(Mat, -500, 300);
	Conn(StripeDark->A, Scanline); Conn(StripeDark->B, ScanIntensity);
	UMaterialExpressionOneMinus* StripeFactor = MakeExpr<UMaterialExpressionOneMinus>(Mat, -340, 300);
	Conn(StripeFactor->Input, StripeDark);

	// --- 지지직(노이즈): 여러 sin 곱으로 불규칙 깜빡. FlickerFactor = 1 + ScanFlicker * (sin(17t)*sin(5.3t)) ---
	UMaterialExpressionMultiply* T1 = MakeExpr<UMaterialExpressionMultiply>(Mat, -1000, 480);
	Conn(T1->A, Time); T1->ConstB = 17.0f;
	UMaterialExpressionSine* SinA = MakeExpr<UMaterialExpressionSine>(Mat, -840, 480);
	Conn(SinA->Input, T1);
	UMaterialExpressionMultiply* T2 = MakeExpr<UMaterialExpressionMultiply>(Mat, -1000, 580);
	Conn(T2->A, Time); T2->ConstB = 5.3f;
	UMaterialExpressionSine* SinB = MakeExpr<UMaterialExpressionSine>(Mat, -840, 580);
	Conn(SinB->Input, T2);
	UMaterialExpressionMultiply* FlickSig = MakeExpr<UMaterialExpressionMultiply>(Mat, -680, 520); // -1..1 불규칙
	Conn(FlickSig->A, SinA); Conn(FlickSig->B, SinB);
	UMaterialExpressionMultiply* FlickAmp = MakeExpr<UMaterialExpressionMultiply>(Mat, -520, 520);
	Conn(FlickAmp->A, FlickSig); Conn(FlickAmp->B, FlickerAmt);
	UMaterialExpressionAdd* FlickerFactor = MakeExpr<UMaterialExpressionAdd>(Mat, -360, 520);
	FlickerFactor->ConstA = 1.0f; Conn(FlickerFactor->B, FlickAmp);

	// --- 본체 Emissive(파랑) × 주사선 × 지지직 ---
	UMaterialExpressionMultiply* BodyEmis = MakeExpr<UMaterialExpressionMultiply>(Mat, -120, 440); // HoloColor*1.5
	Conn(BodyEmis->A, HoloColor); BodyEmis->ConstB = 1.5f;
	UMaterialExpressionMultiply* BodyScan = MakeExpr<UMaterialExpressionMultiply>(Mat, 60, 420);
	Conn(BodyScan->A, BodyEmis); Conn(BodyScan->B, StripeFactor);
	UMaterialExpressionMultiply* BodyFinal = MakeExpr<UMaterialExpressionMultiply>(Mat, 240, 440);
	Conn(BodyFinal->A, BodyScan); Conn(BodyFinal->B, FlickerFactor);

	// --- 경계 Emissive(주황) = ScanColor*EmissiveBoost ---
	UMaterialExpressionMultiply* ScanEmis = MakeExpr<UMaterialExpressionMultiply>(Mat, 240, 600);
	Conn(ScanEmis->A, ScanColor); Conn(ScanEmis->B, EmisBoost);

	// --- Emissive = lerp(BodyFinal, ScanEmis, ScanGlow) ---
	UMaterialExpressionLinearInterpolate* Emissive = MakeExpr<UMaterialExpressionLinearInterpolate>(Mat, 480, 500);
	Conn(Emissive->A, BodyFinal); Conn(Emissive->B, ScanEmis); Conn(Emissive->Alpha, ScanGlow);

	// --- Opacity(반투명) — If 노드 대신 연속 수식 step(ScanGlow와 동일 구조로 픽셀별 게이트 보장) ---
	//     AboveMask = saturate((NormZ - Progress) * 100)  // 경계 위=1, 아래=0 (얇은 소프트 엣지)
	//     Opacity   = saturate(AboveMask*HologramOpacity + ScanGlow)  // 위=반투명, 아래=투명, 경계=주황 크리스프
	//     ⚠️ SubNP(NormZ-Progress)는 경계(주황)가 쓰는 바로 그 노드 — 재사용해 동일 마스크 보장(If 배선 버그 회피).
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

	// --- 머티리얼 출력 연결 ---
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
		TEXT("[Hologram] %s %s (Translucent/파랑/주사선·지지직 강화). 에디터서 확인 후 Ctrl+S → BP_BuildController.HologramBuildUpMaterial 지정."),
		*PackageName, bIsNew ? TEXT("생성 완료") : TEXT("재구성 완료"));
	return Mat;
}

#endif // WITH_EDITOR
