# -*- coding: utf-8 -*-
# 송전탑 전력 범위 구용 노란 전기 플라즈마 머티리얼 M_PowerRange_Plasma 생성 (에디터 내부 Python).
#
# ⚠️ 재작성: 기존 스크립트가 Noise/Voronoi/Fresnel/Power 노드의 set_editor_property에서 예외 → 연결 미도달
#    (검정+불투명)했다. 어제 작동한 M_Hologram(OJJ_HologramMaterialFactory.cpp)의 검증된 노드 팔레트만 사용:
#    VectorParameter / ScalarParameter / Time / Sine / Multiply(const_b) / Add(const_a) / OneMinus / Saturate.
#    전기 갈라짐/치지직 = Time×Sine 깜빡임(홀로그램 FlickerFactor 방식). Fresnel 외곽은 try 가드(실패해도 본체 유지).
#
# 연결은 connect_material_expressions(from, out, to, in) + connect_material_property(expr, "", prop) — 포트레이트
# 머티리얼에서 작동 검증된 동일 API. 기존 에셋은 통째 삭제 후 재생성(노드 MarkAsGarbage 회피).

import unreal

MAT_FOLDER = "/Game/OJJ/Materials"
MAT_NAME = "M_PowerRange_Plasma"
MAT_PATH = "{}/{}".format(MAT_FOLDER, MAT_NAME)

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

# 기존 에셋 통째 삭제 후 재생성(노드 삭제가 아니라 에셋 삭제 — GC 루트 어설션 회피).
if eal.does_asset_exist(MAT_PATH):
    eal.delete_asset(MAT_PATH)
    unreal.log("[PowerRangeMat] 기존 에셋 삭제 후 재생성.")

atools = unreal.AssetToolsHelpers.get_asset_tools()
mat = atools.create_asset(MAT_NAME, MAT_FOLDER, unreal.Material, unreal.MaterialFactoryNew())
unreal.log("[PowerRangeMat] 생성: {}".format(MAT_PATH))

# 반투명 Unlit TwoSided
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("two_sided", True)

def expr(cls, x, y):
    return mel.create_material_expression(mat, cls, x, y)

# --- 파라미터 ---
plasma_color = expr(unreal.MaterialExpressionVectorParameter, -1100, -150)
plasma_color.set_editor_property("parameter_name", "PlasmaColor")
plasma_color.set_editor_property("default_value", unreal.LinearColor(1.0, 0.78, 0.12, 1.0))

opacity_param = expr(unreal.MaterialExpressionScalarParameter, -1100, 250)
opacity_param.set_editor_property("parameter_name", "Opacity")
opacity_param.set_editor_property("default_value", 0.4)

emis_boost = expr(unreal.MaterialExpressionScalarParameter, -1100, -40)
emis_boost.set_editor_property("parameter_name", "EmissiveBoost")
emis_boost.set_editor_property("default_value", 3.0)

flicker_amt = expr(unreal.MaterialExpressionScalarParameter, -1500, 480)
flicker_amt.set_editor_property("parameter_name", "FlickerAmount")
flicker_amt.set_editor_property("default_value", 0.35)

# --- 치지직: Flicker = 1 + FlickerAmount*(sin(17t)*sin(5.3t)) (홀로그램 검증 방식) ---
time_node = expr(unreal.MaterialExpressionTime, -1500, 300)

t1 = expr(unreal.MaterialExpressionMultiply, -1300, 300)
t1.set_editor_property("const_b", 17.0)
mel.connect_material_expressions(time_node, "", t1, "A")
sin_a = expr(unreal.MaterialExpressionSine, -1150, 300)
mel.connect_material_expressions(t1, "", sin_a, "")

t2 = expr(unreal.MaterialExpressionMultiply, -1300, 400)
t2.set_editor_property("const_b", 5.3)
mel.connect_material_expressions(time_node, "", t2, "A")
sin_b = expr(unreal.MaterialExpressionSine, -1150, 400)
mel.connect_material_expressions(t2, "", sin_b, "")

flick_sig = expr(unreal.MaterialExpressionMultiply, -1000, 350)
mel.connect_material_expressions(sin_a, "", flick_sig, "A")
mel.connect_material_expressions(sin_b, "", flick_sig, "B")

flick_amp = expr(unreal.MaterialExpressionMultiply, -850, 380)
mel.connect_material_expressions(flick_sig, "", flick_amp, "A")
mel.connect_material_expressions(flicker_amt, "", flick_amp, "B")

flicker = expr(unreal.MaterialExpressionAdd, -700, 350)
flicker.set_editor_property("const_a", 1.0)
mel.connect_material_expressions(flick_amp, "", flicker, "B")

# --- Fresnel 외곽 발광(선택 — 실패해도 본체 유지) ---
fresnel = None
try:
    fresnel = expr(unreal.MaterialExpressionFresnel, -1100, 100)
    fresnel.set_editor_property("exponent", 2.5)
    fresnel.set_editor_property("base_reflect_fraction", 0.06)
    unreal.log("[PowerRangeMat] Fresnel 노드 추가됨(외곽 발광).")
except Exception as e:
    fresnel = None
    unreal.log_warning("[PowerRangeMat] Fresnel 미지원 — 외곽 발광 생략(본체는 정상): {}".format(e))

# --- Emissive = PlasmaColor × EmissiveBoost × Flicker (+ Fresnel 외곽) ---
body_emis = expr(unreal.MaterialExpressionMultiply, -500, -100)
mel.connect_material_expressions(plasma_color, "", body_emis, "A")
mel.connect_material_expressions(emis_boost, "", body_emis, "B")

body_flick = expr(unreal.MaterialExpressionMultiply, -350, -60)
mel.connect_material_expressions(body_emis, "", body_flick, "A")
mel.connect_material_expressions(flicker, "", body_flick, "B")

emissive_out = body_flick
if fresnel:
    # 외곽도 노랑 발광: + PlasmaColor*Fresnel*Boost
    rim = expr(unreal.MaterialExpressionMultiply, -350, 120)
    mel.connect_material_expressions(plasma_color, "", rim, "A")
    mel.connect_material_expressions(fresnel, "", rim, "B")
    emissive_add = expr(unreal.MaterialExpressionAdd, -180, 0)
    mel.connect_material_expressions(body_flick, "", emissive_add, "A")
    mel.connect_material_expressions(rim, "", emissive_add, "B")
    emissive_out = emissive_add

# --- Opacity = saturate((Fresnel + floor) × Opacity), floor로 구 전체 가시성 ---
op_floor = expr(unreal.MaterialExpressionScalarParameter, -700, 600)
op_floor.set_editor_property("parameter_name", "OpacityFloor")
op_floor.set_editor_property("default_value", 0.35)

if fresnel:
    op_base = expr(unreal.MaterialExpressionAdd, -500, 400)
    mel.connect_material_expressions(fresnel, "", op_base, "A")
    mel.connect_material_expressions(op_floor, "", op_base, "B")
    op_src = op_base
else:
    op_src = op_floor

op_mul = expr(unreal.MaterialExpressionMultiply, -300, 350)
mel.connect_material_expressions(op_src, "", op_mul, "A")
mel.connect_material_expressions(opacity_param, "", op_mul, "B")
op_final = expr(unreal.MaterialExpressionSaturate, -150, 350)
mel.connect_material_expressions(op_mul, "", op_final, "")

# --- 최종 연결 ---
mel.connect_material_property(emissive_out, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(op_final, "", unreal.MaterialProperty.MP_OPACITY)

mel.layout_material_expressions(mat)
mel.recompile_material(mat)
eal.save_asset(MAT_PATH)
unreal.log("[PowerRangeMat] 완료 — Emissive/Opacity 연결됨. Blend=Translucent, Unlit. PIE에서 송전탑 호버로 확인(검정 X).")
