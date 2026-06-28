# -*- coding: utf-8 -*-
# 포트레이트 UI 보정 머티리얼 M_Portrait_UI 생성 (에디터 내부 Python으로 실행)
#  RT_RobotPortrait(SCS_SceneColorHDR)의 알파는 "역불투명도"(로봇=0, 배경=1)라 그대로 쓰면 뒤집힌다.
#  → UI 도메인 Translucent 머티리얼에서 Opacity = 1 - RT.Alpha 로 교정(로봇=불투명, 배경=투명).
#
# 안전수칙: 노드 "삭제"는 하지 않는다(MarkAsGarbage 어설션 크래시 회피) — 신규 생성 시에만 노드 추가.
#           에디터 내부 실행이라 헤드리스 저장 충돌 없음.

import unreal

RT_PATH = "/Game/OJJ/Character/Robot/RT_RobotPortrait"
MAT_FOLDER = "/Game/OJJ/UI"
MAT_NAME = "M_Portrait_UI"
MAT_PATH = "{}/{}".format(MAT_FOLDER, MAT_NAME)

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

rt = eal.load_asset(RT_PATH)
if rt is None:
    unreal.log_error("[PortraitMat] RT_RobotPortrait를 못 찾음 — 먼저 RT가 있어야 합니다.")
    raise SystemExit

newly_created = not eal.does_asset_exist(MAT_PATH)
if newly_created:
    atools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = atools.create_asset(MAT_NAME, MAT_FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    unreal.log("[PortraitMat] 머티리얼 신규 생성: {}".format(MAT_PATH))
else:
    mat = eal.load_asset(MAT_PATH)
    unreal.log("[PortraitMat] 기존 머티리얼 재사용(노드는 건드리지 않음): {}".format(MAT_PATH))

# UI 도메인 + 반투명 블렌드
mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

if newly_created:
    # TextureSample(RT) → RGB는 Emissive(UI 최종색), A는 1-A 거쳐 Opacity
    tex = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -400, 0)
    tex.set_editor_property("texture", rt)

    one_minus = mel.create_material_expression(mat, unreal.MaterialExpressionOneMinus, -150, 160)

    # 연결: RGB → Emissive Color (UI 도메인의 화면 색)
    mel.connect_material_property(tex, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    # 연결: A → OneMinus → Opacity
    mel.connect_material_expressions(tex, "A", one_minus, "")
    mel.connect_material_property(one_minus, "", unreal.MaterialProperty.MP_OPACITY)

    unreal.log("[PortraitMat] 노드 구성 완료 (TextureSample → Emissive / 1-Alpha → Opacity)")

mel.recompile_material(mat)
eal.save_asset(MAT_PATH)
unreal.log("[PortraitMat] 완료 — WBP_PortraitTest의 Image Brush.Image를 RT 대신 M_Portrait_UI 로 교체 후 PIE 확인.")
