"""
M_River 머티리얼 생성 스크립트 (UE5.7 PythonScriptPlugin).
실행: UnrealEditor-Cmd.exe "<uproject>" -run=pythonscript -script="<이 파일>"

WaterArea.cpp 와의 파라미터 이름 계약: VectorParameter "FlowVelocity" (정확 일치).
흐르는 강: UV = TexCoord*Tiling + ComponentMask(Time*FlowVelocity).RG  ->  Normal 텍스처 샘플.
"""
import unreal

MAT_PATH = "/Game/OJJ/Materials"
MAT_NAME = "M_River"
FULL_PATH = MAT_PATH + "/" + MAT_NAME
NORMAL_TEX = "/Engine/EngineMaterials/RandomNormal2"

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary


def log(msg):
    unreal.log("[M_River] " + str(msg))


def main():
    # 기존 자산 있으면 지우고 재생성(멱등)
    if EAL.does_asset_exist(FULL_PATH):
        log("기존 M_River 삭제 후 재생성")
        EAL.delete_asset(FULL_PATH)

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = asset_tools.create_asset(MAT_NAME, MAT_PATH, unreal.Material, unreal.MaterialFactoryNew())
    if mat is None:
        log("ERROR: create_asset 실패")
        return False

    # 반투명 + Default Lit (Shading Model 기본 유지)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    try:
        mat.set_editor_property("translucency_lighting_mode",
                                unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    except Exception as e:
        log("translucency_lighting_mode 설정 생략: " + str(e))

    # --- 파라미터 노드 ---
    tiling = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, -200)
    tiling.set_editor_property("parameter_name", "Tiling")
    tiling.set_editor_property("default_value", 4.0)

    flow = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -900, 100)
    flow.set_editor_property("parameter_name", "FlowVelocity")   # ⚠️ WaterArea 코드와 일치
    flow.set_editor_property("default_value", unreal.LinearColor(0.05, 0.0, 0.0, 0.0))

    tint = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, 400)
    tint.set_editor_property("parameter_name", "WaterTint")
    tint.set_editor_property("default_value", unreal.LinearColor(0.0, 0.3, 0.5, 1.0))

    opacity = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 600)
    opacity.set_editor_property("parameter_name", "BaseOpacity")
    opacity.set_editor_property("default_value", 0.6)

    rough = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 750)
    rough.set_editor_property("r", 0.08)

    spec = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 850)
    spec.set_editor_property("r", 1.0)

    # --- 흐르는 UV 체인 ---
    texcoord = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -700, -300)

    uv_tiled = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -500, -250)
    MEL.connect_material_expressions(texcoord, "", uv_tiled, "A")
    MEL.connect_material_expressions(tiling, "", uv_tiled, "B")

    time = MEL.create_material_expression(mat, unreal.MaterialExpressionTime, -700, 100)

    flow_time = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -650, 50)
    MEL.connect_material_expressions(time, "", flow_time, "A")
    MEL.connect_material_expressions(flow, "", flow_time, "B")

    mask = MEL.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -500, 50)
    mask.set_editor_property("r", True)
    mask.set_editor_property("g", True)
    mask.set_editor_property("b", False)
    mask.set_editor_property("a", False)
    MEL.connect_material_expressions(flow_time, "", mask, "")

    uv_final = MEL.create_material_expression(mat, unreal.MaterialExpressionAdd, -300, -150)
    MEL.connect_material_expressions(uv_tiled, "", uv_final, "A")
    MEL.connect_material_expressions(mask, "", uv_final, "B")

    # --- 노멀 텍스처 샘플 ---
    # EAL.load_asset은 Asset Registry 의존이라 commandlet에서 엔진 콘텐츠 미등록 시 실패.
    # unreal.load_asset은 패키지를 디스크에서 직접 로드 → 엔진 콘텐츠도 견고.
    normal_tex = unreal.load_asset(NORMAL_TEX)
    if normal_tex is None:
        log("ERROR: 노멀 텍스처 로드 실패: " + NORMAL_TEX)
        return False

    tex_sample = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -100, -150)
    tex_sample.set_editor_property("texture", normal_tex)
    tex_sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    MEL.connect_material_expressions(uv_final, "", tex_sample, "UVs")

    # --- 머티리얼 프로퍼티 연결 ---
    MEL.connect_material_property(tex_sample, "", unreal.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)

    # --- 정리/컴파일/저장 ---
    MEL.layout_material_expressions(mat)
    MEL.recompile_material(mat)
    saved = EAL.save_asset(FULL_PATH)
    log("save_asset = " + str(saved))
    log("SUCCESS: " + FULL_PATH)
    return True


ok = False
try:
    ok = main()
except Exception as e:
    import traceback
    unreal.log_error("[M_River] EXCEPTION: " + str(e))
    unreal.log_error(traceback.format_exc())

if ok:
    unreal.log("[M_River] RESULT=OK")
else:
    unreal.log_error("[M_River] RESULT=FAIL")
