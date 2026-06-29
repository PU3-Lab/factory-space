// Fill out your copyright notice in the Description page of Project Settings.

#include "OJJ_LandscapeMaterialFactory.h"

#if WITH_EDITOR

#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionNoise.h"
#include "Materials/MaterialExpressionDeriveNormalZ.h"
#include "Materials/MaterialExpressionDesaturation.h"
#include "Materials/MaterialExpressionLandscapeLayerBlend.h"
#include "Materials/MaterialExpressionLandscapeLayerCoords.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeEditTypes.h" // ELandscapeTargetLayerBlendMethod
#include "Engine/Texture.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h" // EPropertyChangeType (SetLayerUsageDebugColor)

namespace
{
	// 텍스처 경로(.Object 형식 포함).
	const TCHAR* kGroundB   = TEXT("/Game/OJJ/Textures/T_wkjncjv_2K_B.T_wkjncjv_2K_B");
	const TCHAR* kGroundN   = TEXT("/Game/OJJ/Textures/T_wkjncjv_2K_N.T_wkjncjv_2K_N");
	const TCHAR* kGroundORM = TEXT("/Game/OJJ/Textures/T_wkjncjv_2K_ORM.T_wkjncjv_2K_ORM");

	// Gravel = Military_Trenches_Ground_Dirt_Rough_02 (yd0lacrs). ⚠️ 접미사 소문자 2k. B/N/ORM 합본(同패킹). _H는 미사용.
	const TCHAR* kGravelB   = TEXT("/Game/Fab/Megascans/Surfaces/Military_Trenches_Ground_Dirt_Rough_02_yd0lacrs/Medium/yd0lacrs_tier_2/Textures/T_yd0lacrs_2k_B.T_yd0lacrs_2k_B");
	const TCHAR* kGravelN   = TEXT("/Game/Fab/Megascans/Surfaces/Military_Trenches_Ground_Dirt_Rough_02_yd0lacrs/Medium/yd0lacrs_tier_2/Textures/T_yd0lacrs_2k_N.T_yd0lacrs_2k_N");
	const TCHAR* kGravelORM = TEXT("/Game/Fab/Megascans/Surfaces/Military_Trenches_Ground_Dirt_Rough_02_yd0lacrs/Medium/yd0lacrs_tier_2/Textures/T_yd0lacrs_2k_ORM.T_yd0lacrs_2k_ORM");

	// ⚠️ 유니티(jumbo) 빌드 충돌 회피 — OJJ_HologramMaterialFactory.cpp의 동명 익명 헬퍼(Conn/MakeExpr)와
	// 같은 TU로 묶이면 재정의 에러(C2084/C2995). LS 접두사로 고유화.
	void LSConn(FExpressionInput& In, UMaterialExpression* Expr, int32 OutIndex = 0)
	{
		In.Expression = Expr;
		In.OutputIndex = OutIndex;
	}

	template <typename T>
	T* LSMakeExpr(UMaterial* Mat, int32 X, int32 Y)
	{
		return Cast<T>(UMaterialEditingLibrary::CreateMaterialExpression(Mat, T::StaticClass(), X, Y));
	}

	UTexture* LoadTex(const TCHAR* Path)
	{
		UTexture* Tex = LoadObject<UTexture>(nullptr, Path);
		if (!Tex)
		{
			UE_LOG(LogTemp, Error, TEXT("[LandscapeMat] 텍스처 로드 실패: %s"), Path);
		}
		return Tex;
	}

	UMaterialExpressionTextureSample* Sample(UMaterial* Mat, UTexture* Tex, EMaterialSamplerType SType,
		UMaterialExpression* UV, int32 X, int32 Y)
	{
		UMaterialExpressionTextureSample* S = LSMakeExpr<UMaterialExpressionTextureSample>(Mat, X, Y);
		S->Texture = Tex;
		S->SamplerType = SType;
		if (UV) { LSConn(S->Coordinates, UV); }
		return S;
	}

	// 멀티 스케일 샘플: 같은 텍스처를 UV1/UV2(다른 크기)로 2번 샘플 → Lerp(0.5).
	// 비정수배 스케일로 두 무늬가 어긋나 격자 반복이 시각적으로 사라진다. 매크로 베리에이션(밝기)과 보완.
	UMaterialExpression* SampleMultiScale(UMaterial* Mat, UTexture* Tex, EMaterialSamplerType SType,
		UMaterialExpression* UV1, UMaterialExpression* UV2, int32 X, int32 Y)
	{
		UMaterialExpressionTextureSample* S1 = Sample(Mat, Tex, SType, UV1, X, Y);
		UMaterialExpressionTextureSample* S2 = Sample(Mat, Tex, SType, UV2, X, Y + 130);
		UMaterialExpressionLinearInterpolate* L = LSMakeExpr<UMaterialExpressionLinearInterpolate>(Mat, X + 220, Y + 60);
		LSConn(L->A, S1); LSConn(L->B, S2); L->ConstAlpha = 0.5f;
		return L;
	}

	UMaterialExpressionComponentMask* Mask(UMaterial* Mat, UMaterialExpression* In,
		bool bR, bool bG, bool bB, int32 X, int32 Y)
	{
		UMaterialExpressionComponentMask* M = LSMakeExpr<UMaterialExpressionComponentMask>(Mat, X, Y);
		M->R = bR; M->G = bG; M->B = bB; M->A = false;
		LSConn(M->Input, In);
		return M;
	}

	// LandscapeLayerBlend에 WeightBlend 레이어 한 칸 추가.
	void AddBlendLayer(UMaterialExpressionLandscapeLayerBlend* Blend, FName LayerName,
		UMaterialExpression* Input, float PreviewWeight)
	{
		FLayerBlendInput L;
		L.LayerName = LayerName;
		L.BlendType = LB_WeightBlend;
		L.PreviewWeight = PreviewWeight;
		LSConn(L.LayerInput, Input);
		Blend->Layers.Add(L);
	}
}

ULandscapeLayerInfoObject* UOJJ_LandscapeMaterialFactory::EnsureLayerInfo(
	const FString& AssetName, FName LayerName, const FLinearColor& DebugColor)
{
	const FString PackageName = TEXT("/Game/OJJ/Landscape/") + AssetName;
	const FString FullPath = PackageName + TEXT(".") + AssetName;

	if (ULandscapeLayerInfoObject* Existing = LoadObject<ULandscapeLayerInfoObject>(nullptr, *FullPath))
	{
		// 기존 에셋이 옛 None(=No Weight Blending)으로 만들어졌을 수 있으니, 재사용 시에도 가중 블렌딩으로 교정.
		if (Existing->GetBlendMethod() != ELandscapeTargetLayerBlendMethod::FinalWeightBlending)
		{
			Existing->SetBlendMethod(ELandscapeTargetLayerBlendMethod::FinalWeightBlending, /*bInModify*/ false);
			Existing->MarkPackageDirty();
			UE_LOG(LogTemp, Display, TEXT("[LandscapeMat] LayerInfo 재사용 — BlendMethod를 FinalWeightBlending으로 교정: %s"), *PackageName);
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("[LandscapeMat] LayerInfo 이미 존재 — 재사용: %s"), *PackageName);
		}
		return Existing;
	}

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		UE_LOG(LogTemp, Error, TEXT("[LandscapeMat] LayerInfo 패키지 생성 실패: %s"), *PackageName);
		return nullptr;
	}

	ULandscapeLayerInfoObject* LayerInfo =
		NewObject<ULandscapeLayerInfoObject>(Package, *AssetName, RF_Public | RF_Standalone);
	if (!LayerInfo)
	{
		UE_LOG(LogTemp, Error, TEXT("[LandscapeMat] LayerInfo 생성 실패: %s"), *AssetName);
		return nullptr;
	}

	// 5.7부터 직접 멤버 대입은 deprecated → setter 사용(신규 생성이라 bInModify=false).
	LayerInfo->SetLayerName(LayerName, /*bInModify*/ false);
	LayerInfo->SetLayerUsageDebugColor(DebugColor, /*bInModify*/ false, EPropertyChangeType::ValueSet);
	// ⚠️ BlendMethod 기본값은 None(="No Weight Blending") — 가중 블렌딩이 안 됨.
	//    LandscapeLayerBlend(WeightBlend)와 맞물리려면 FinalWeightBlending(레거시 가중)으로 설정.
	LayerInfo->SetBlendMethod(ELandscapeTargetLayerBlendMethod::FinalWeightBlending, /*bInModify*/ false);

	FAssetRegistryModule::AssetCreated(LayerInfo);
	LayerInfo->MarkPackageDirty();
	UE_LOG(LogTemp, Display, TEXT("[LandscapeMat] LayerInfo 생성: %s (LayerName=%s)"), *PackageName, *LayerName.ToString());
	return LayerInfo;
}

UMaterial* UOJJ_LandscapeMaterialFactory::GenerateFactoryGroundMaterial(bool bOverwriteExisting, bool bCreateLayerInfo)
{
	const FString AssetName = TEXT("M_Landscape_FactoryGround");
	const FString PackageName = TEXT("/Game/OJJ/Materials/") + AssetName;
	const FString FullPath = PackageName + TEXT(".") + AssetName;

	// --- 텍스처 6개 선로드(하나라도 실패하면 중단) ---
	UTexture* GroundB = LoadTex(kGroundB);
	UTexture* GroundN = LoadTex(kGroundN);
	UTexture* GroundORM = LoadTex(kGroundORM);
	UTexture* GravelB = LoadTex(kGravelB);
	UTexture* GravelN = LoadTex(kGravelN);
	UTexture* GravelORM = LoadTex(kGravelORM);
	if (!GroundB || !GroundN || !GroundORM || !GravelB || !GravelN || !GravelORM)
	{
		UE_LOG(LogTemp, Error, TEXT("[LandscapeMat] 텍스처 로드 실패로 중단. 경로/임포트 확인."));
		return nullptr;
	}

	// --- 머티리얼 패키지 ---
	bool bIsNew = false;
	UMaterial* Mat = LoadObject<UMaterial>(nullptr, *FullPath);
	if (Mat)
	{
		if (!bOverwriteExisting)
		{
			UE_LOG(LogTemp, Warning, TEXT("[LandscapeMat] %s 이미 존재 — 중단(bOverwriteExisting=true로 재생성)."), *PackageName);
			return nullptr;
		}
		UMaterialEditingLibrary::DeleteAllMaterialExpressions(Mat);
		UE_LOG(LogTemp, Display, TEXT("[LandscapeMat] %s 노드 제거 후 재구성(overwrite)."), *PackageName);
	}
	else
	{
		UPackage* Package = CreatePackage(*PackageName);
		if (!Package) { UE_LOG(LogTemp, Error, TEXT("[LandscapeMat] 패키지 생성 실패: %s"), *PackageName); return nullptr; }
		Mat = NewObject<UMaterial>(Package, *AssetName, RF_Public | RF_Standalone);
		if (!Mat) { UE_LOG(LogTemp, Error, TEXT("[LandscapeMat] UMaterial 생성 실패")); return nullptr; }
		bIsNew = true;
	}

	Mat->SetShadingModel(MSM_DefaultLit);

	// --- 타일링: LandscapeLayerCoords × per-layer ScalarParameter ---
	UMaterialExpressionLandscapeLayerCoords* Coords = LSMakeExpr<UMaterialExpressionLandscapeLayerCoords>(Mat, -2200, 0);
	Coords->MappingScale = 1.0f;

	UMaterialExpressionScalarParameter* GroundTiling = LSMakeExpr<UMaterialExpressionScalarParameter>(Mat, -2200, 160);
	GroundTiling->ParameterName = TEXT("GroundTiling"); GroundTiling->DefaultValue = 1.0f;
	UMaterialExpressionScalarParameter* GravelTiling = LSMakeExpr<UMaterialExpressionScalarParameter>(Mat, -2200, 240);
	GravelTiling->ParameterName = TEXT("GravelTiling"); GravelTiling->DefaultValue = 1.0f;

	// UV1(기본) = Coords × Tiling.  UV2(디테일) = UV1 × DetailScaleRatio(비정수배).
	UMaterialExpressionScalarParameter* DetailRatio = LSMakeExpr<UMaterialExpressionScalarParameter>(Mat, -2200, 320);
	DetailRatio->ParameterName = TEXT("DetailScaleRatio"); DetailRatio->DefaultValue = 0.37f;

	UMaterialExpressionMultiply* GroundUV1 = LSMakeExpr<UMaterialExpressionMultiply>(Mat, -2000, 80);
	LSConn(GroundUV1->A, Coords); LSConn(GroundUV1->B, GroundTiling);
	UMaterialExpressionMultiply* GroundUV2 = LSMakeExpr<UMaterialExpressionMultiply>(Mat, -2000, 160);
	LSConn(GroundUV2->A, GroundUV1); LSConn(GroundUV2->B, DetailRatio);

	UMaterialExpressionMultiply* GravelUV1 = LSMakeExpr<UMaterialExpressionMultiply>(Mat, -2000, 360);
	LSConn(GravelUV1->A, Coords); LSConn(GravelUV1->B, GravelTiling);
	UMaterialExpressionMultiply* GravelUV2 = LSMakeExpr<UMaterialExpressionMultiply>(Mat, -2000, 440);
	LSConn(GravelUV2->A, GravelUV1); LSConn(GravelUV2->B, DetailRatio);

	// --- Ground 레이어 샘플 ---
	//  BaseColor/ORM = 멀티 스케일(타일 반복 깨기, 평균해도 무해).
	//  ⚠️ Normal = 단일 스케일(UV1)! 서로 다른 스케일 노멀을 Lerp 평균하면 xy 섭동이 상쇄돼 평탄화됨 → 입체감 소실.
	//     노멀은 벡터라 BaseColor와 블렌드 방식이 다름. 단일 샘플로 native 강도 유지.
	UMaterialExpression* GroundBaseTex = SampleMultiScale(Mat, GroundB, SAMPLERTYPE_Color, GroundUV1, GroundUV2, -1700, -260);
	UMaterialExpression* GroundNormTex = Sample(Mat, GroundN, SAMPLERTYPE_Normal, GroundUV1, -1700, 0);
	UMaterialExpression* GroundOrmTex = SampleMultiScale(Mat, GroundORM, SAMPLERTYPE_Masks, GroundUV1, GroundUV2, -1700, 260);

	// --- Gravel 레이어 샘플 (Normal은 위와 동일 이유로 단일 스케일) ---
	UMaterialExpression* GravelBaseTex = SampleMultiScale(Mat, GravelB, SAMPLERTYPE_Color, GravelUV1, GravelUV2, -1700, 560);
	UMaterialExpression* GravelNormTex = Sample(Mat, GravelN, SAMPLERTYPE_Normal, GravelUV1, -1700, 820);
	UMaterialExpression* GravelOrmTex = SampleMultiScale(Mat, GravelORM, SAMPLERTYPE_Masks, GravelUV1, GravelUV2, -1700, 1080);

	// --- Gravel 틴트: Gravel BaseColor × GravelTint (LayerBlend 진입 전, Gravel에만 적용).
	//     기본 무채색(0.6 회색, R=G=B) — 더트 톤 차분하게. 인스턴스서 (1,1,1) 중립이나 다른 회색으로 조정. ---
	// 채도 제거(선택): GravelDesaturation 0=원본색, 1=완전 회색. 갈색 텍스처를 무채색으로(틴트 곱만으론 hue 유지됨).
	UMaterialExpressionScalarParameter* GravelDesatAmt = LSMakeExpr<UMaterialExpressionScalarParameter>(Mat, -1700, 360);
	GravelDesatAmt->ParameterName = TEXT("GravelDesaturation"); GravelDesatAmt->DefaultValue = 0.0f;
	UMaterialExpressionDesaturation* GravelDesat = LSMakeExpr<UMaterialExpressionDesaturation>(Mat, -1550, 440);
	LSConn(GravelDesat->Input, GravelBaseTex); LSConn(GravelDesat->Fraction, GravelDesatAmt);

	UMaterialExpressionVectorParameter* GravelTint = LSMakeExpr<UMaterialExpressionVectorParameter>(Mat, -1550, 600);
	GravelTint->ParameterName = TEXT("GravelTint");
	GravelTint->DefaultValue = FLinearColor(0.6f, 0.6f, 0.6f, 1.0f);
	UMaterialExpressionMultiply* GravelBaseTinted = LSMakeExpr<UMaterialExpressionMultiply>(Mat, -1380, 480);
	LSConn(GravelBaseTinted->A, GravelDesat); LSConn(GravelBaseTinted->B, GravelTint);

	// --- 3개 LandscapeLayerBlend (BaseColor / Normal / ORM) ---
	UMaterialExpressionLandscapeLayerBlend* BlendBase = LSMakeExpr<UMaterialExpressionLandscapeLayerBlend>(Mat, -1300, -200);
	AddBlendLayer(BlendBase, TEXT("Ground"), GroundBaseTex, 1.0f);
	AddBlendLayer(BlendBase, TEXT("Gravel"), GravelBaseTinted, 0.0f);

	UMaterialExpressionLandscapeLayerBlend* BlendNorm = LSMakeExpr<UMaterialExpressionLandscapeLayerBlend>(Mat, -1300, 100);
	AddBlendLayer(BlendNorm, TEXT("Ground"), GroundNormTex, 1.0f);
	AddBlendLayer(BlendNorm, TEXT("Gravel"), GravelNormTex, 0.0f);

	UMaterialExpressionLandscapeLayerBlend* BlendOrm = LSMakeExpr<UMaterialExpressionLandscapeLayerBlend>(Mat, -1300, 400);
	AddBlendLayer(BlendOrm, TEXT("Ground"), GroundOrmTex, 1.0f);
	AddBlendLayer(BlendOrm, TEXT("Gravel"), GravelOrmTex, 0.0f);

	// --- 매크로 베리에이션: 큰 스케일 Noise를 [1-Amt, 1+Amt]로 리맵 → BaseColor에 곱 ---
	UMaterialExpressionNoise* MacroNoise = LSMakeExpr<UMaterialExpressionNoise>(Mat, -1300, -480);
	MacroNoise->Scale = 0.003f;     // 작은 Scale = 큰 패치(저주파). 월드 위치 기반(타일과 무관) → 반복 깨기.
	MacroNoise->OutputMin = 0.0f;
	MacroNoise->OutputMax = 1.0f;
	MacroNoise->Levels = 2;

	UMaterialExpressionScalarParameter* MacroAmt = LSMakeExpr<UMaterialExpressionScalarParameter>(Mat, -1300, -360);
	MacroAmt->ParameterName = TEXT("MacroVariationAmount"); MacroAmt->DefaultValue = 0.4f;

	UMaterialExpressionOneMinus* MacroLow = LSMakeExpr<UMaterialExpressionOneMinus>(Mat, -1100, -360);  // 1 - Amt
	LSConn(MacroLow->Input, MacroAmt);
	UMaterialExpressionAdd* MacroHigh = LSMakeExpr<UMaterialExpressionAdd>(Mat, -1100, -280);           // 1 + Amt
	MacroHigh->ConstA = 1.0f; LSConn(MacroHigh->B, MacroAmt);

	UMaterialExpressionLinearInterpolate* MacroFactor = LSMakeExpr<UMaterialExpressionLinearInterpolate>(Mat, -900, -360);
	LSConn(MacroFactor->A, MacroLow); LSConn(MacroFactor->B, MacroHigh); LSConn(MacroFactor->Alpha, MacroNoise);

	UMaterialExpressionMultiply* BaseColorFinal = LSMakeExpr<UMaterialExpressionMultiply>(Mat, -700, -200);
	LSConn(BaseColorFinal->A, BlendBase); LSConn(BaseColorFinal->B, MacroFactor);

	// --- ORM 분리 (R:AO, G:Roughness, B:Metallic) ---
	UMaterialExpressionComponentMask* AO = Mask(Mat, BlendOrm, true, false, false, -1000, 360);
	UMaterialExpressionComponentMask* Rough = Mask(Mat, BlendOrm, false, true, false, -1000, 440);
	UMaterialExpressionComponentMask* Metal = Mask(Mat, BlendOrm, false, false, true, -1000, 520);

	// --- Normal 강도: 블렌드된 노멀의 xy를 NormalIntensity로 증폭 → DeriveNormalZ로 Z 재계산(정규화).
	//     타일링을 크게 해도 요철이 살아있게. 기본 2.0(강조). 인스턴스서 1.0=원본~더 크게 조정. ---
	UMaterialExpressionScalarParameter* NormalIntensity = LSMakeExpr<UMaterialExpressionScalarParameter>(Mat, -1000, 120);
	NormalIntensity->ParameterName = TEXT("NormalIntensity"); NormalIntensity->DefaultValue = 2.0f;
	UMaterialExpressionComponentMask* NormXY = Mask(Mat, BlendNorm, true, true, false, -820, 120);
	UMaterialExpressionMultiply* NormXYAmp = LSMakeExpr<UMaterialExpressionMultiply>(Mat, -640, 120);
	LSConn(NormXYAmp->A, NormXY); LSConn(NormXYAmp->B, NormalIntensity);
	UMaterialExpressionDeriveNormalZ* NormFinal = LSMakeExpr<UMaterialExpressionDeriveNormalZ>(Mat, -460, 120);
	LSConn(NormFinal->InXY, NormXYAmp);

	// --- 머티리얼 출력 연결 ---
	UMaterialEditingLibrary::ConnectMaterialProperty(BaseColorFinal, TEXT(""), MP_BaseColor);
	UMaterialEditingLibrary::ConnectMaterialProperty(NormFinal, TEXT(""), MP_Normal);
	UMaterialEditingLibrary::ConnectMaterialProperty(Rough, TEXT(""), MP_Roughness);
	UMaterialEditingLibrary::ConnectMaterialProperty(Metal, TEXT(""), MP_Metallic);
	UMaterialEditingLibrary::ConnectMaterialProperty(AO, TEXT(""), MP_AmbientOcclusion);

	UMaterialEditingLibrary::LayoutMaterialExpressions(Mat);
	UMaterialEditingLibrary::RecompileMaterial(Mat);

	if (bIsNew)
	{
		FAssetRegistryModule::AssetCreated(Mat);
	}
	Mat->MarkPackageDirty();

	// --- LayerInfo (선택) ---
	if (bCreateLayerInfo)
	{
		EnsureLayerInfo(TEXT("LI_FactoryGround"), TEXT("Ground"), FLinearColor(0.55f, 0.45f, 0.30f));
		EnsureLayerInfo(TEXT("LI_FactoryGravel"), TEXT("Gravel"), FLinearColor(0.40f, 0.35f, 0.30f));
	}

	UE_LOG(LogTemp, Display,
		TEXT("[LandscapeMat] %s %s. 레이어 'Ground'(wkjncjv)/'Gravel'(vd4pbdt). ")
		TEXT("에디터서 확인 후 Ctrl+S → Landscape 머티리얼 슬롯 지정 + Paint 모드서 LayerInfo 할당."),
		*PackageName, bIsNew ? TEXT("생성") : TEXT("재구성"));
	return Mat;
}

#endif // WITH_EDITOR
