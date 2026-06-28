// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OJJ_HologramMaterialFactory.generated.h"

class UMaterial;

/**
 * [#3 점진 건설] M_Hologram_BuildUp 머티리얼을 코드로 생성하는 에디터 전용 유틸리티(노드 수작업 대체).
 * docs/03_architecture/M_Hologram_BuildUp_guide.md 그래프를 UMaterialEditingLibrary로 구성한다.
 *
 * 사용법(에디터 안에서만): Editor Utility Blueprint/Widget을 하나 만들어 GenerateHologramBuildUpMaterial 노드를
 * 호출하거나, 콘솔/EUW 버튼으로 실행. 생성 후 Content Browser에서 /Game/OJJ/Materials/M_Hologram_BuildUp을
 * 열어 그래프/렌더 확인 후 Ctrl+S로 저장한다(코드가 강제 저장하지 않음 — 헤드리스 저장 손상 회피).
 *
 * ⚠️ 에디터 전용(WITH_EDITOR). 런타임/패키지 빌드에는 포함되지 않는다.
 */
UCLASS()
class WANTED_FACTORY_API UOJJ_HologramMaterialFactory : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	// 홀로그램 빌드업 머티리얼 생성.
	//  bPathMode=false → M_Hologram_BuildUp (머신/파운데이션: 월드 Z 마스크, 아래서위 차오름).
	//  bPathMode=true  → M_Hologram_BuildUp_Path (컨베이어/파이프: 월드위치를 경로축 투영 마스크, 시작→끝 차오름.
	//                    PathStart/PathDir/PathLength 파라미터를 C++가 주입. per-instance 커스텀데이터 미사용 →
	//                    파이프 액체 커스텀데이터 무접촉).
	//  bOverwriteExisting=true면 기존 머티리얼 노드만 지우고 in-place 재구성(패키지 삭제 없음). 실패 시 null.
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "OJJ|Hologram")
	static UMaterial* GenerateHologramBuildUpMaterial(bool bOverwriteExisting = false, bool bPathMode = false);
#endif
};
